#pragma once
#include <Geode/Geode.hpp>
#include <filesystem>
#include <random>

using namespace geode::prelude;

namespace mashup {
	// Path of the mashup that should replace the music in the PlayLayer that is currently open.
	// Empty = play the original song.
	inline std::string g_activePath;

	inline std::string serverUrl() {
		auto url = Mod::get()->getSettingValue<std::string>("server-url");
		while (!url.empty() && url.back() == '/') url.pop_back();
		return url;
	}

	inline std::filesystem::path mashupDir(int levelID) {
		auto dir = Mod::get()->getSaveDir() / "mashups" / std::to_string(levelID);
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		return dir;
	}

	inline std::string activeKey(int levelID) {
		return fmt::format("active-{}", levelID);
	}

	inline std::string getActivePath(int levelID) {
		return Mod::get()->getSavedValue<std::string>(activeKey(levelID), "");
	}

	inline void setActivePath(int levelID, std::string const& path) {
		Mod::get()->setSavedValue<std::string>(activeKey(levelID), path);
	}

	// Random per-install token. The server binds it to your account ID the first time you upload,
	// so nobody else can overwrite your mashups afterwards.
	inline std::string getToken() {
		auto token = Mod::get()->getSavedValue<std::string>("token", "");
		if (token.size() != 32) {
			static constexpr char hex[] = "0123456789abcdef";
			std::random_device rd;
			std::mt19937_64 gen(rd());
			token.clear();
			for (int i = 0; i < 32; i++) token.push_back(hex[gen() % 16]);
			Mod::get()->setSavedValue<std::string>("token", token);
		}
		return token;
	}

	inline std::string urlEncode(std::string const& in) {
		static constexpr char hex[] = "0123456789ABCDEF";
		std::string out;
		for (unsigned char c : in) {
			if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
				out.push_back(static_cast<char>(c));
			} else {
				out.push_back('%');
				out.push_back(hex[c >> 4]);
				out.push_back(hex[c & 15]);
			}
		}
		return out;
	}

	constexpr size_t MAX_UPLOAD_BYTES = 20 * 1024 * 1024;
}
