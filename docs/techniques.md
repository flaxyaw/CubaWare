# Techniques & Methods

High-level breakdown of the techniques used in CubaWare. Aimed at people new to this area.

---

## API Hashing

**What:** Instead of calling functions by name (e.g. `GetProcAddress("EtwEventWrite")`), we hash the name at compile time and look it up by hash at runtime.

**Why:** Strings like `ntdll.dll` and `EtwEventWrite` in a binary are an instant red flag for AV. With hashing, none of those strings exist in the file.

**How:** At startup the code walks the Windows process environment block (PEB), which contains a list of all loaded DLLs. It hashes each DLL name and each exported function name using FNV-1a and compares against the pre-computed target hash. No strings needed.

---

## Indirect Syscalls

**What:** Calling Windows kernel functions without going through the normal ntdll path.

**Why:** AV/EDR products patch ntdll at runtime by inserting a jump at the start of sensitive functions. This lets them intercept every call before it reaches the kernel. Indirect syscalls bypass the jump entirely.

**How:** The code reads the syscall number directly out of the ntdll stub, then executes the `syscall` instruction itself. If the stub is already patched, it finds the number from a neighboring function (Halo's Gate technique) since syscall numbers are sequential.

---

## ntdll Unhooking

**What:** Wiping all AV/EDR patches out of the live ntdll in memory.

**Why:** Even after bypassing individual hooks with indirect syscalls, a hooked ntdll is still a sign that something is watching. A clean ntdll means subsequent calls are also unmonitored.

**How:** Windows is asked to map a fresh copy of ntdll.dll from disk into memory. The clean `.text` section from that copy is then written over the live one, removing every injected jump.

---

## ETW Patching

**What:** Disabling Event Tracing for Windows.

**Why:** ETW is the main telemetry channel Windows uses to report what's happening to AV, EDR, and Defender. Blinding it means those tools stop receiving events.

**How:** The first byte of `EtwEventWrite` in ntdll is overwritten with `0xC3` (return). Every call to it now returns immediately without logging anything.

---

## AMSI Patching

**What:** Disabling the Antimalware Scan Interface.

**Why:** AMSI is how Windows lets AV scan scripts and in-memory content. PowerShell, the CLR, and other hosts call AMSI before running anything. After patching it always reports clean.

**How:** Same as ETW. First byte of `AmsiScanBuffer` overwritten with `0xC3`.

---

## PE Header Wipe

**What:** Zeroing the binary's own header in memory after startup.

**Why:** Memory scanners and forensic tools use the PE header to identify and reconstruct a running binary. Without it, the process looks like a blob of code with no identifiable structure.

**How:** After all evasion setup is done, the first 4096 bytes of the loaded image (DOS header, NT header, section table) are zeroed out using `SecureZeroMemory`.

---

## PPID Spoofing

**What:** Making the process appear to have been launched by Explorer.

**Why:** The process tree is one of the first things analysts look at. If CubaWare shows up as a child of `cmd.exe` or a browser, that's suspicious. As a child of Explorer it looks like a double-clicked file.

**How:** Windows allows specifying a custom parent process when creating a new process. The binary relaunches itself with Explorer's handle as the parent, then the original exits.

---

## Anti-VM Scoring

**What:** Detecting virtual machine environments.

**Why:** Analysts and sandboxes run malware inside VMs. Exiting in a VM means the sample never runs where it can be observed.

**How:** Multiple checks are scored. CPUID hypervisor bit (+45), timing anomalies (+15/+35), RDP session (+5), with a deduction for bare-metal Hyper-V (-30). Score above 45 = exit.

---

## Anti-Targeting (CIS Whitelist)

**What:** Exiting on machines with CIS-region keyboard layouts.

**Why:** Standard practice to avoid targeting users in Russia and neighboring countries, both for legal reasons and to reduce unwanted attention.

**How:** Checks the active keyboard layout and all loaded layouts against a list of CIS language IDs. Also checks the hostname and execution path against known sandbox patterns.

---

## Compile-Time String Encryption

**What:** Encrypting all sensitive string literals at compile time.

**Why:** Strings in a binary (DLL names, file paths, registry keys) are trivial to find with a hex editor. Encrypted strings make static analysis significantly harder.

**How:** The skCrypt library XOR-encrypts strings at compile time using a key derived from `__TIME__` and a per-build random constant. Plaintext never exists in the binary. Strings are decrypted to the stack at runtime and zeroed after use.

---

## Per-Build Polymorphism

**What:** Making every compiled binary unique.

**Why:** AV and detection systems often use byte-level signatures. If every build produces a different binary, a signature for one build won't match another.

**How:** CMake generates four random 4-byte constants (POLY_K1–K4) each time the project is configured. These flow into the string encryption keys, opaque predicate constants, and junk computations. The encrypted strings, dead code, and fake control flow are all different in every build. The PE postprocessor also randomises the timestamp and appends a variable-length junk overlay.

---

## Chromium Credential Decryption

**What:** Decrypting saved passwords and cookies from Chrome-based browsers.

**Why:** Chromium encrypts all saved credentials with AES-256-GCM using a master key. You can't read the SQLite databases without it.

**How:** See [flow-chrome-abe.md](flow-chrome-abe.md).

Supported: Chrome (including 127+ ABE), Edge, Brave, Arc, Opera, Yandex, Chromium.

---

## Firefox / Gecko Credential Decryption

**What:** Decrypting saved passwords and cookies from Firefox-based browsers.

**Why:** Firefox uses Mozilla's NSS library for encryption, which is completely different from the Windows crypto stack Chrome uses.

**How:** See [flow-gecko-nss.md](flow-gecko-nss.md).

Supported: Firefox, Waterfox, LibreWolf, Pale Moon, IceDragon, SeaMonkey, Thunderbird, Zen, K-Meleon.

---

## In-Memory Collection (mem_store)

**What:** A virtual filesystem that lives entirely in RAM.

**Why:** Writing collected data to disk creates forensic artifacts like temp files, prefetch entries, and AV scan triggers. Keeping everything in memory avoids all of that.

**How:** A global hash map (`unordered_map<string, vector<uint8_t>>`) acts as a virtual filesystem. Every stealer writes into it using a path like `chromium/Chrome/Default/passwords.txt`. Nothing hits disk until `zip_to_buf()` is called, which serialises the whole map into an encrypted ZIP and returns the raw bytes.

---

## AES-256 Encrypted ZIP Exfiltration

**What:** Packing all collected data into a password-protected ZIP before sending.

**Why:** HTTPS encrypts the transport, but the ZIP password adds a second layer. Without it, anyone who intercepts or gains access to the C2 uploads directory can read the logs. The password is per-build and never stored on the server.

**How:** minizip-ng with WinZIP AES-256 per entry, DEFLATE compression. The ZIP is built entirely in memory and POSTed directly. Never written to disk. Password is baked in at compile time via the builder.

---

## IAT Proxy

**What:** Hiding certain DLL imports from the binary's import table.

**Why:** The PE import table lists every DLL and function the binary uses. Analysts and AV check this immediately. Hiding sensitive imports (WinHTTP, CredEnumerateA, clipboard functions) makes the binary look less suspicious.

**How:** Instead of linking against these functions normally, a template wrapper resolves them by hash at runtime on first call. The DLL names and function names never appear in the import directory.

---

## Discord Token Extraction

**What:** Stealing Discord authentication tokens.

**Why:** Discord tokens let you log in as that user without a password or 2FA.

**How:** Discord (an Electron app) stores encrypted tokens in a LevelDB database with the prefix `dQw4w9WgXcQ:`. The code scans the LevelDB files for this prefix, extracts the base64 blob, and decrypts it using the same DPAPI + AES-256-GCM method Chrome uses. Discord is an Electron app so it uses the same Chromium crypto stack.

---

## PE Postprocessing

**What:** Modifying the compiled binary before it's ready for use.

**Why:** A freshly compiled binary has a predictable timestamp, fixed file size, and recognisable structure. Postprocessing makes it look less uniform.

**How:** After compilation, a script randomises the PE timestamp to somewhere in 2012–2022, appends 4KB–12KB of structured junk (fake function prologues, MOV/XOR/NOP chains, fake strings) to the end of the file, and recalculates the PE checksum so the file remains valid.
