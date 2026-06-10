#pragma once
namespace antivm {
    bool is_rdp();
    bool is_vm_timing();
    bool is_hypervisor_present();
    bool hv_is_hyperv();
    bool is_tsc_spoofed();
    bool has_sandbox_dlls();
    bool has_vm_registry();
    bool has_vm_mac();
    bool is_low_resources();
    bool is_low_uptime();
    int  calc_risk();
}
