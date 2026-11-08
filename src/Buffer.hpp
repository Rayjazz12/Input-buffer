#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <array>
#include <string>

namespace ib {
	// ---------------------------------------------------------------
	// Settings
	// ---------------------------------------------------------------
	constexpr int LEVELS = 5;
	// Buffer window for each level, in milliseconds. Edit to tune.
	constexpr int MS[LEVELS] = { 50, 90, 130, 180, 250 };
	constexpr int MAX_MS = 250;

	constexpr int MAX_QUEUE = 3;      // how many early clicks can wait at once
	constexpr float REARM_MS = 80.f;  // min gap before the same orb/landing can fire again

	inline bool enabled = false;
	inline int level = 1;             // index into MS
	inline bool orbs = true;          // buffer clicks for orbs
	inline bool ground = true;        // buffer clicks for landings
	inline bool practiceOnly = false;
	inline bool flash = true;         // small dot when a saved click fires
	inline int theme = 0;
	inline int saved = 0;             // lifetime count of clicks the buffer rescued

	// ---------------------------------------------------------------
	// Themes
	// ---------------------------------------------------------------
	constexpr int THEME_COUNT = 6;

	struct Theme {
		const char* name;
		cocos2d::ccColor3B panel;
		cocos2d::ccColor3B frame;
		cocos2d::ccColor3B accent;
		cocos2d::ccColor3B card;
		cocos2d::ccColor3B pill;
		cocos2d::ccColor3B muted;
		GLubyte cardOp;
		GLubyte pillOp;
	};

	inline const std::array<Theme, THEME_COUNT> THEMES = {{
		{ "Ocean",    cocos2d::ccc3(172, 222, 248), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(66, 140, 238),
		  cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(84, 118, 156), 105, 150 },
		{ "Sunset",   cocos2d::ccc3(252, 208, 178), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(240, 112, 72),
		  cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(150, 98, 80), 105, 150 },
		{ "Mint",     cocos2d::ccc3(178, 236, 214), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(36, 170, 122),
		  cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(66, 128, 106), 105, 150 },
		{ "Grape",    cocos2d::ccc3(216, 198, 250), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(140, 92, 230),
		  cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(112, 92, 152), 105, 150 },
		{ "Rose",     cocos2d::ccc3(250, 202, 224), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(232, 86, 142),
		  cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(150, 88, 114), 105, 150 },
		{ "Midnight", cocos2d::ccc3(36, 42, 70),    cocos2d::ccc3(84, 98, 150),   cocos2d::ccc3(100, 172, 255),
		  cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(255, 255, 255), cocos2d::ccc3(150, 166, 204), 22, 34 },
	}};

	inline const Theme& currentTheme() { return THEMES[std::clamp(theme, 0, THEME_COUNT - 1)]; }

	// ---------------------------------------------------------------
	// Save / load
	// ---------------------------------------------------------------
	inline void load() {
		auto mod = geode::Mod::get();
		enabled = mod->getSavedValue<bool>("enabled", false);
		level = std::clamp(mod->getSavedValue<int>("level", 1), 0, LEVELS - 1);
		orbs = mod->getSavedValue<bool>("orbs", true);
		ground = mod->getSavedValue<bool>("ground", true);
		practiceOnly = mod->getSavedValue<bool>("practice-only", false);
		flash = mod->getSavedValue<bool>("flash", true);
		theme = std::clamp(mod->getSavedValue<int>("theme", 0), 0, THEME_COUNT - 1);
		saved = std::max(0, mod->getSavedValue<int>("saved", 0));
	}

	inline void setEnabled(bool v) { enabled = v; geode::Mod::get()->setSavedValue("enabled", v); }
	inline void setLevel(int v) { level = std::clamp(v, 0, LEVELS - 1); geode::Mod::get()->setSavedValue("level", level); }
	inline void setOrbs(bool v) { orbs = v; geode::Mod::get()->setSavedValue("orbs", v); }
	inline void setGround(bool v) { ground = v; geode::Mod::get()->setSavedValue("ground", v); }
	inline void setPracticeOnly(bool v) { practiceOnly = v; geode::Mod::get()->setSavedValue("practice-only", v); }
	inline void setFlash(bool v) { flash = v; geode::Mod::get()->setSavedValue("flash", v); }
	inline void setTheme(int v) { theme = std::clamp(v, 0, THEME_COUNT - 1); geode::Mod::get()->setSavedValue("theme", theme); }
	inline void addSaved() { saved++; geode::Mod::get()->setSavedValue("saved", saved); }

	inline float windowMs() { return static_cast<float>(MS[level]); }

	// ---------------------------------------------------------------
	// Per-player buffer state (0 = player 1, 1 = player 2)
	// ---------------------------------------------------------------
	struct State {
		int held = 0;                       // real presses currently held
		std::array<float, MAX_QUEUE> ages{}; // early clicks waiting, oldest first (ms since click)
		int count = 0;
		bool releaseNext = false;           // we pressed for a tap, release next frame
		bool armed = true;                  // ready to fire for the current orb/landing
		float sinceFire = 1000.f;           // ms since a click was used

		void push() {
			if (count == MAX_QUEUE) popFront();
			ages[count++] = 0.f;
		}

		void popFront() {
			for (int i = 1; i < count; i++) ages[i - 1] = ages[i];
			if (count > 0) count--;
		}

		// age every waiting click, drop the ones past the window
		void age(float ms, float window) {
			int w = 0;
			for (int i = 0; i < count; i++) {
				float a = ages[i] + ms;
				if (a <= window) ages[w++] = a;
			}
			count = w;
		}

		// the game (or we) just used a click on an orb/landing
		void markUsed() {
			armed = false;
			sinceFire = 0.f;
		}
	};
	inline State st[2];

	// true while WE are sending a click, so our hook ignores our own clicks
	inline bool injecting = false;

	inline void reset() {
		st[0] = State{};
		st[1] = State{};
	}
}
