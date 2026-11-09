#pragma once
#include "Mashup.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/async.hpp>

struct MashupEntry {
	int accountID = 0;
	std::string uploader;
	std::string title;
	std::string ext;
	int downloads = 0;
};

// Geode v5: Popup is no longer templated; arguments go to our own init().
class MashupPopup : public geode::Popup {
protected:
	using PickResult = Result<std::optional<std::filesystem::path>>;

	int m_levelID = 0;
	ScrollLayer* m_scroll = nullptr;
	CCLabelBMFont* m_status = nullptr;

	// Holders cancel their task automatically when the popup is destroyed.
	geode::async::TaskHolder<web::WebResponse> m_listTask;
	geode::async::TaskHolder<web::WebResponse> m_uploadTask;
	geode::async::TaskHolder<web::WebResponse> m_downloadTask;
	geode::async::TaskHolder<PickResult> m_pickTask;

	bool init(GJGameLevel* level);
	void setStatus(std::string const& text, bool error = false);
	void refresh();
	void populate(std::vector<MashupEntry> const& entries);
	void startUpload(std::filesystem::path const& path);

	void onRefresh(CCObject*);
	void onUpload(CCObject*);
	void onOriginal(CCObject*);
	void onUse(CCObject*);
	void onDeleteMine(CCObject*);

public:
	static MashupPopup* create(GJGameLevel* level);
};
