#include "MashupPopup.hpp"

namespace fs = std::filesystem;

MashupPopup* MashupPopup::create(GJGameLevel* level) {
	auto ret = new MashupPopup();
	if (ret->init(level)) {
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

static ButtonSprite* makeButton(char const* text, float scale = 0.7f) {
	return ButtonSprite::create(text, "bigFont.fnt", "GJ_button_01.png", scale);
}

// Safe JSON readers (copy values out; no unwrapOr on reference-returning Results)
static std::string jsonStr(matjson::Value& obj, char const* key, std::string def) {
	auto& v = obj[key];
	if (v.isString()) {
		std::string out = v.asString().unwrap();
		return out;
	}
	return def;
}

static int jsonInt(matjson::Value& obj, char const* key, int def) {
	auto& v = obj[key];
	if (v.isNumber()) {
		return static_cast<int>(v.asInt().unwrap());
	}
	return def;
}

static std::string describeFailure(web::WebResponse const& res) {
	if (res.code() <= 0) return "Can't reach the server (check the Server URL in settings)";
	return fmt::format("Server error ({})", res.code());
}

bool MashupPopup::init(GJGameLevel* level) {
	if (!Popup::init(380.f, 270.f)) return false;

	m_levelID = level->m_levelID.value();
	this->setTitle("Mashup Share");

	auto size = m_mainLayer->getContentSize();

	// list background
	auto bg = CCScale9Sprite::create("square02b_001.png", {0, 0, 80, 80});
	bg->setColor({0, 0, 0});
	bg->setOpacity(90);
	bg->setContentSize({340.f, 150.f});
	m_mainLayer->addChildAtPosition(bg, Anchor::Center, {0, 15});

	// scroll list
	m_scroll = ScrollLayer::create({340.f, 150.f});
	m_scroll->setPosition({size.width / 2 - 170.f, size.height / 2 + 15 - 75.f});
	m_scroll->m_contentLayer->setLayout(
		ColumnLayout::create()
			->setAxisReverse(true)
			->setAxisAlignment(AxisAlignment::End)
			->setAutoGrowAxis(150.f)
			->setGap(2.f)
	);
	m_mainLayer->addChild(m_scroll);

	// status line
	m_status = CCLabelBMFont::create("", "chatFont.fnt");
	m_status->setScale(0.6f);
	m_mainLayer->addChildAtPosition(m_status, Anchor::Bottom, {0, 52});

	// buttons
	auto upload = CCMenuItemSpriteExtra::create(makeButton("Upload mine"), this, menu_selector(MashupPopup::onUpload));
	auto remove = CCMenuItemSpriteExtra::create(makeButton("Delete mine"), this, menu_selector(MashupPopup::onDeleteMine));
	auto original = CCMenuItemSpriteExtra::create(makeButton("Original"), this, menu_selector(MashupPopup::onOriginal));
	auto refreshBtn = CCMenuItemSpriteExtra::create(makeButton("Refresh"), this, menu_selector(MashupPopup::onRefresh));

	m_buttonMenu->addChildAtPosition(upload, Anchor::Bottom, {-130, 25});
	m_buttonMenu->addChildAtPosition(remove, Anchor::Bottom, {-30, 25});
	m_buttonMenu->addChildAtPosition(original, Anchor::Bottom, {55, 25});
	m_buttonMenu->addChildAtPosition(refreshBtn, Anchor::Bottom, {135, 25});

	this->refresh();
	return true;
}

void MashupPopup::setStatus(std::string const& text, bool error) {
	m_status->setString(text.c_str());
	m_status->setColor(error ? ccColor3B{255, 100, 100} : ccColor3B{255, 255, 255});
}

void MashupPopup::onRefresh(CCObject*) {
	this->refresh();
}

void MashupPopup::refresh() {
	setStatus("Loading mashups...");
	m_scroll->m_contentLayer->removeAllChildren();
	m_scroll->m_contentLayer->updateLayout();

	auto url = fmt::format("{}/api/mashups/{}", mashup::serverUrl(), m_levelID);

	m_listTask.spawn(
		web::WebRequest().get(url),
		[this](web::WebResponse res) {
			if (!res.ok()) return setStatus(describeFailure(res), true);

			auto json = res.json();
			if (!json) return setStatus("Bad response from server", true);

			std::vector<MashupEntry> entries;
			auto root = json.unwrap();
			auto& list = root["mashups"];
			if (list.isArray()) {
				auto arr = list.asArray().unwrap();
				for (auto& item : arr) {
					MashupEntry m;
					m.accountID = jsonInt(item, "accountID", 0);
					m.uploader = jsonStr(item, "uploader", "Unknown");
					m.title = jsonStr(item, "title", "Untitled");
					m.ext = jsonStr(item, "ext", "mp3");
					m.downloads = jsonInt(item, "downloads", 0);
					entries.push_back(std::move(m));
				}
			}
			this->populate(entries);
			setStatus(entries.empty() ? "No mashups yet - be the first!" : fmt::format("{} mashup(s)", entries.size()));
		}
	);
}

void MashupPopup::populate(std::vector<MashupEntry> const& entries) {
	auto content = m_scroll->m_contentLayer;
	content->removeAllChildren();

	auto active = mashup::getActivePath(m_levelID);

	for (auto const& m : entries) {
		auto row = CCNode::create();
		row->setContentSize({340.f, 32.f});

		auto path = mashup::mashupDir(m_levelID) / fmt::format("{}.{}", m.accountID, m.ext);
		bool isActive = !active.empty() && active == geode::utils::string::pathToString(path);

		auto label = CCLabelBMFont::create(
			fmt::format("{}{}", isActive ? "[ON] " : "", m.title).c_str(), "bigFont.fnt");
		label->limitLabelWidth(215.f, 0.5f, 0.2f);
		label->setAnchorPoint({0.f, 0.5f});
		label->setPosition({10.f, 22.f});
		if (isActive) label->setColor({120, 255, 120});
		row->addChild(label);

		auto sub = CCLabelBMFont::create(
			fmt::format("by {}  |  {} plays", m.uploader, m.downloads).c_str(), "chatFont.fnt");
		sub->setScale(0.5f);
		sub->setAnchorPoint({0.f, 0.5f});
		sub->setPosition({10.f, 8.f});
		sub->setOpacity(190);
		row->addChild(sub);

		auto menu = CCMenu::create();
		menu->setPosition({0.f, 0.f});
		menu->setContentSize(row->getContentSize());
		auto btn = CCMenuItemSpriteExtra::create(makeButton("Use", 0.6f), this, menu_selector(MashupPopup::onUse));
		btn->setTag(m.accountID);
		btn->setUserObject("ext", CCString::create(m.ext));
		btn->setPosition({300.f, 16.f});
		menu->addChild(btn);
		row->addChild(menu);

		content->addChild(row);
	}

	content->updateLayout();
	m_scroll->moveToTop();
}

void MashupPopup::onUse(CCObject* sender) {
	auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);
	int accountID = btn->getTag();
	std::string ext = "mp3";
	if (auto obj = static_cast<CCString*>(btn->getUserObject("ext"))) ext = obj->getCString();

	auto path = mashup::mashupDir(m_levelID) / fmt::format("{}.{}", accountID, ext);
	auto url = fmt::format("{}/api/mashups/{}/{}/file", mashup::serverUrl(), m_levelID, accountID);
	setStatus("Downloading...");

	m_downloadTask.spawn(
		web::WebRequest().get(url),
		[this, path](web::WebResponse res) {
			if (!res.ok()) return setStatus(describeFailure(res), true);

			auto written = geode::utils::file::writeBinary(path, res.data());
			if (!written) return setStatus("Couldn't save file", true);

			mashup::setActivePath(m_levelID, geode::utils::string::pathToString(path));
			setStatus("Mashup selected! It will play next time you start the level.");
			this->refresh();
		}
	);
}

void MashupPopup::onOriginal(CCObject*) {
	mashup::setActivePath(m_levelID, "");
	setStatus("Using the original song.");
	this->refresh();
}

void MashupPopup::onUpload(CCObject*) {
	if (GJAccountManager::sharedState()->m_accountID <= 0) {
		return setStatus("Log in to your GD account to upload.", true);
	}

	geode::utils::file::FilePickOptions options;
	options.filters.push_back({"Audio files", {"*.mp3", "*.ogg", "*.wav"}});

	m_pickTask.spawn(
		geode::utils::file::pick(geode::utils::file::PickMode::OpenFile, options),
		[this](PickResult result) {
			if (!result.isOk()) return setStatus("Couldn't open file", true);
			auto picked = result.unwrap();
			if (!picked) return; // user cancelled the dialog
			this->startUpload(*picked);
		}
	);
}

void MashupPopup::startUpload(fs::path const& path) {
	auto data = geode::utils::file::readBinary(path);
	if (!data) return setStatus("Couldn't read file", true);
	if (data.unwrap().size() > mashup::MAX_UPLOAD_BYTES) return setStatus("File too large (max 20 MB)", true);

	auto acct = GJAccountManager::sharedState();
	auto title = geode::utils::string::pathToString(path.stem());
	if (title.size() > 40) title.resize(40);

	setStatus("Uploading...");

	auto req = web::WebRequest();
	req.header("Content-Type", "application/octet-stream");
	req.header("X-Account-ID", std::to_string(acct->m_accountID));
	req.header("X-Username", mashup::urlEncode(std::string(acct->m_username)));
	req.header("X-Token", mashup::getToken());
	req.header("X-Title", mashup::urlEncode(title));
	req.body(data.unwrap());

	auto url = fmt::format("{}/api/mashups/{}", mashup::serverUrl(), m_levelID);

	m_uploadTask.spawn(
		req.post(url),
		[this](web::WebResponse res) {
			if (!res.ok()) {
				auto body = res.string();
				std::string msg = body.isOk() ? std::string(body.unwrap()) : std::string();
				if (res.code() <= 0) msg = describeFailure(res);
				return setStatus(fmt::format("Upload failed ({}) {}", res.code(), msg), true);
			}
			setStatus("Uploaded! Everyone can now use your mashup.");
			this->refresh();
		}
	);
}

void MashupPopup::onDeleteMine(CCObject*) {
	auto acct = GJAccountManager::sharedState();
	if (acct->m_accountID <= 0) return setStatus("Log in to your GD account first.", true);

	auto req = web::WebRequest();
	req.header("X-Account-ID", std::to_string(acct->m_accountID));
	req.header("X-Token", mashup::getToken());

	auto url = fmt::format("{}/api/mashups/{}", mashup::serverUrl(), m_levelID);

	m_uploadTask.spawn(
		req.send("DELETE", url),
		[this](web::WebResponse res) {
			if (!res.ok()) return setStatus(describeFailure(res), true);
			setStatus("Your mashup was deleted.");
			this->refresh();
		}
	);
}
