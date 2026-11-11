#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <array>
#include <string>

// ---------------------------------------------------------------------------
// Input Buffer - shared settings, themes and per-player buffer state.
// ---------------------------------------------------------------------------
namespace ib {
	// ---------------------------------------------------------------
	// Tuning
	// ---------------------------------------------------------------
	constexpr int LEVELS = 5;
	// Buffer window for each level, in milliseconds (index 0 = first pill).
	constexpr int MS[LEVELS] = { 50, 90, 130, 180, 250 };
	constexpr int MAX_MS = 250;

	constexpr int MAX_QUEUE = 3;      // how many early clicks can wait at once
	constexpr float REARM_MS = 40.f;  // safety gap between two saved clicks firing

	// ---------------------------------------------------------------
	// Settings (persisted)
	// ---------------------------------------------------------------
	inline bool enabled = false;
	inline int level = 2;             // default 130 ms
	inline bool orbs = true;          // buffer clicks for orbs
	inline bool ground = true;        // buffer clicks for landings
	inline bool practiceOnly = false; // only buffer in practice mode
	inline bool flash = true;         // flash a dot when a saved click fires
	inline bool hud = true;           // show waiting clicks in the corner

	inline int theme = 0;
	inline int saved = 0;             // lifetime clicks the buffer rescued
	inline int session = 0;           // clicks rescued this session

	// ---------------------------------------------------------------
	// Themes: one panel colour, one accent, one muted text colour.
	// ---------------------------------------------------------------
	constexpr int THEME_COUNT = 6;

	struct Theme {
		char const* name;
		cocos2d::ccColor3B panel;
		cocos2d::ccColor3B accent;
		cocos2d::ccColor3B muted;
	};

	inline const std::array<Theme, THEME_COUNT> THEMES = { {
		{ "Ocean",    cocos2d::ccc3(120, 190, 240), cocos2d::ccc3( 66, 140, 238), cocos2d::ccc3(235, 243, 255) },
		{ "Sunset",   cocos2d::ccc3(238, 150, 110), cocos2d::ccc3(240, 112,  72), cocos2d::ccc3(255, 240, 230) },
		{ "Mint",     cocos2d::ccc3(110, 205, 175), cocos2d::ccc3( 36, 170, 122), cocos2d::ccc3(235, 255, 246) },
		{ "Grape",    cocos2d::ccc3(160, 132, 235), cocos2d::ccc3(140,  92, 230), cocos2d::ccc3(244, 238, 255) },
		{ "Rose",     cocos2d::ccc3(236, 132, 176), cocos2d::ccc3(232,  86, 142), cocos2d::ccc3(255, 238, 246) },
		{ "Midnight", cocos2d::ccc3( 58,  70, 110), cocos2d::ccc3(100, 172, 255), cocos2d::ccc3(196, 210, 240) },
	} };

	inline Theme const& currentTheme() {
		return THEMES[std::clamp(theme, 0, THEME_COUNT - 1)];
	}

	inline cocos2d::ccColor4F rgba(cocos2d::ccColor3B c, float a) {
		return cocos2d::ccc4f(c.r / 255.f, c.g / 255.f, c.b / 255.f, a);
	}

	inline float windowMs() {
		return static_cast<float>(MS[std::clamp(level, 0, LEVELS - 1)]);
	}

	// ---------------------------------------------------------------
	// Save / load
	// ---------------------------------------------------------------
	inline void load() {
		auto mod = geode::Mod::get();
		enabled = mod->getSavedValue<bool>("enabled", false);
		level = std::clamp(mod->getSavedValue<int>("level", 2), 0, LEVELS - 1);
		orbs = mod->getSavedValue<bool>("orbs", true);
		ground = mod->getSavedValue<bool>("ground", true);
		practiceOnly = mod->getSavedValue<bool>("practice-only", false);
		flash = mod->getSavedValue<bool>("flash", true);
		hud = mod->getSavedValue<bool>("hud", true);
		theme = std::clamp(mod->getSavedValue<int>("theme", 0), 0, THEME_COUNT - 1);
		saved = std::max(0, mod->getSavedValue<int>("saved", 0));
		level = std::clamp(level, 0, LEVELS - 1);
	}

	inline void setEnabled(bool v) { enabled = v; geode::Mod::get()->setSavedValue("enabled", v); }
	inline void setLevel(int v) { level = std::clamp(v, 0, LEVELS - 1); geode::Mod::get()->setSavedValue("level", level); }
	inline void setOrbs(bool v) { orbs = v; geode::Mod::get()->setSavedValue("orbs", v); }
	inline void setGround(bool v) { ground = v; geode::Mod::get()->setSavedValue("ground", v); }
	inline void setPracticeOnly(bool v) { practiceOnly = v; geode::Mod::get()->setSavedValue("practice-only", v); }
	inline void setFlash(bool v) { flash = v; geode::Mod::get()->setSavedValue("flash", v); }
	inline void setHud(bool v) { hud = v; geode::Mod::get()->setSavedValue("hud", v); }
	inline void setTheme(int v) { theme = std::clamp(v, 0, THEME_COUNT - 1); geode::Mod::get()->setSavedValue("theme", theme); }

	// count a rescued click; the file is only written on flush() so we never
	// hitch the game by saving during play
	inline void addSaved() { saved++; session++; }
	inline void flushSaved() { geode::Mod::get()->setSavedValue("saved", saved); }

	// ---------------------------------------------------------------
	// Per-player buffer state (0 = player 1, 1 = player 2)
	// ---------------------------------------------------------------
	struct State {
		std::array<float, MAX_QUEUE> ages{}; // early clicks waiting, oldest first
		int count = 0;
		int held = 0;              // real (physical) presses currently held
		float sinceFire = 10000.f;// ms since a saved click was used
		bool tapRelease = false;   // we pressed for a tap -> release next frame
		bool wasOnGround = false;  // last frame's ground state (landing edges)
		bool orbTouch = false;     // last frame's orb state (fallback detection)
		int lastRing = -1;         // last orb we already handled

		// clamp: pushing over the limit drops the oldest waiting click
		void push() {
			if (count >= MAX_QUEUE) popFront();
			ages[count++] = 0.f;
		}

		void popFront() {
			for (int i = 1; i < count; i++) ages[i - 1] = ages[i];
			if (count > 0) count--;
		}

		// age every waiting click and throw away the ones past the window
		void age(float ms, float window) {
			if (window <= 0.f) { count = 0; return; }
			int w = 0;
			for (int i = 0; i < count; i++) {
				float a = ages[i] + ms;
				if (a <= window) ages[w++] = a;
			}
			count = w;
		}
	};

	inline State st[2];

	// true while WE are sending a click, so our own hook ignores it
	inline bool injecting = false;

	inline void reset() {
		st[0] = State{};
		st[1] = State{};
	}
}
