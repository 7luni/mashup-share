#include "Mashup.hpp"
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

// Decide which mashup (if any) to use when a level starts.
class $modify(MashupPlayLayer, PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		mashup::g_activePath.clear();

		if (level && Mod::get()->getSettingValue<bool>("enabled")) {
			int id = level->m_levelID.value();
			if (id > 0) {
				auto path = mashup::getActivePath(id);
				if (!path.empty() && std::filesystem::exists(std::filesystem::path(path))) {
					mashup::g_activePath = path;
				}
			}
		}

		return PlayLayer::init(level, useReplay, dontCreateObjects);
	}

	void onQuit() {
		PlayLayer::onQuit();
		mashup::g_activePath.clear();
	}
};

// Swap the file the level music is loaded from.
class $modify(MashupAudio, FMODAudioEngine) {
	void playMusic(gd::string path, bool shouldLoop, float fadeInTime, int channel) {
		if (channel == 0 && !mashup::g_activePath.empty()) {
			path = mashup::g_activePath;
		}
		FMODAudioEngine::playMusic(path, shouldLoop, fadeInTime, channel);
	}
};
