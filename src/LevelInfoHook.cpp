#include "Mashup.hpp"
#include "MashupPopup.hpp"
#include <Geode/modify/LevelInfoLayer.hpp>

class $modify(MashupLevelInfo, LevelInfoLayer) {
	bool init(GJGameLevel* level, bool challenge) {
		if (!LevelInfoLayer::init(level, challenge)) return false;

		auto menu = this->getChildByID("left-side-menu");
		if (!menu) return true;

		CCNode* sprite = nullptr;
		if (auto frame = CCSprite::create("mashup-icon.png"_spr)) {
			sprite = CircleButtonSprite::create(frame, CircleBaseColor::Green, CircleBaseSize::Medium);
		}
		if (!sprite) sprite = ButtonSprite::create("Mashup");

		auto btn = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(MashupLevelInfo::onMashup));
		btn->setID("mashup-button"_spr);
		menu->addChild(btn);
		menu->updateLayout();
		return true;
	}

	void onMashup(CCObject*) {
		if (m_level->m_levelID.value() <= 0) {
			FLAlertLayer::create("Mashup Share", "Mashups only work on online / official levels.", "OK")->show();
			return;
		}
		MashupPopup::create(m_level)->show();
	}
};
