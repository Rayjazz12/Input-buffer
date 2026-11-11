#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "Buffer.hpp"

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Themed input buffer panel.
//   - Off / 50 / 90 / 130 / 180 / 250 ms pills
//   - big number + meter that glide between windows
//   - Orbs / Ground / Practice / Flash / HUD toggles
//   - 6 themes that fade into each other
// Built on geode::Popup so it gets a proper background, close button,
// Escape handling and the standard popup animation for free.
// ---------------------------------------------------------------------------
class BufferPopup : public geode::Popup {
protected:
	static constexpr float W = 400.f;
	static constexpr float H = 322.f;
	static constexpr int WIN_PILLS = ib::LEVELS + 1; // Off + 5 windows
	static constexpr int TOG_PILLS = 5;              // Orbs, Ground, Practice, Flash, HUD

	static constexpr float BAR_W = 320.f;
	static constexpr float BAR_H = 8.f;
	static constexpr float BAR_Y = 46.f;

	struct Pill {
		CCNode* cont = nullptr;
		CCScale9Sprite* bg = nullptr;
		CCLabelBMFont* lbl = nullptr;
	};

	struct Dot {
		CCNode* cont = nullptr;
		CCDrawNode* art = nullptr;
	};

	inline static BufferPopup* s_open = nullptr;

	CCMenu* m_menu = nullptr;

	CCScale9Sprite* m_panel = nullptr;
	CCScale9Sprite* m_track = nullptr;
	CCScale9Sprite* m_fill = nullptr;
	CCScale9Sprite* m_chipBG = nullptr;
	CCLabelBMFont* m_chipLbl = nullptr;
	CCLabelBMFont* m_bigNum = nullptr;
	CCLabelBMFont* m_info = nullptr;
	CCLabelBMFont* m_savedLbl = nullptr;

	std::vector<CCScale9Sprite*> m_cards;
	std::vector<CCLabelBMFont*> m_mutedLabels;

	std::array<Pill, WIN_PILLS> m_win{};
	std::array<Pill, TOG_PILLS> m_tog{};
	std::array<Dot, ib::THEME_COUNT> m_dots{};

	// number glides toward the selected window
	float m_msCur = 0.f;
	float m_msTarget = 0.f;
	int m_shownMs = -1;
	bool m_closing = false;

	// ---- helpers ---------------------------------------------------
	static ib::Theme const& th() { return ib::currentTheme(); }

	static CCPoint P(float x, float y) { return { W / 2.f + x, H / 2.f + y }; }

	template <class T>
	static void tint(T* n, cocos2d::ccColor3B c, bool animate) {
		if (!n) return;
		n->stopActionByTag(55);
		if (!animate) { n->setColor(c); return; }
		auto a = CCTintTo::create(0.24f, c.r, c.g, c.b);
		a->setTag(55);
		n->runAction(a);
	}

	template <class T>
	static void fade(T* n, GLubyte op, bool animate) {
		if (!n) return;
		n->stopActionByTag(56);
		if (!animate) { n->setOpacity(op); return; }
		auto a = CCFadeTo::create(0.24f, op);
		a->setTag(56);
		n->runAction(a);
	}

	static CCScale9Sprite* box(CCSize size, cocos2d::ccColor3B col, GLubyte opacity) {
		auto s = CCScale9Sprite::create("square02_001.png");
		s->setContentSize(size);
		s->setColor(col);
		s->setOpacity(opacity);
		return s;
	}

	CCScale9Sprite* addCard(CCSize size, CCPoint pos, int z = 4) {
		auto c = box(size, ccc3(255, 255, 255), 40);
		c->setPosition(pos);
		m_mainLayer->addChild(c, z);
		m_cards.push_back(c);
		return c;
	}

	CCLabelBMFont* addLabel(char const* text, float scale, CCPoint pos, CCPoint anchor, bool muted = true,
		char const* font = "bigFont.fnt") {
		auto l = CCLabelBMFont::create(text, font);
		l->setScale(scale);
		l->setAnchorPoint(anchor);
		l->setPosition(pos);
		m_mainLayer->addChild(l, 6);
		if (muted) {
			l->setColor(th().muted);
			m_mutedLabels.push_back(l);
		}
		return l;
	}

	CCLabelBMFont* addHeader(char const* text, CCPoint pos) {
		return addLabel(text, 0.34f, pos, { 0.f, 0.5f });
	}

	Pill makePill(CCSize size, char const* text, float textScale, int tag, SEL_MenuHandler cb, CCPoint pos) {
		auto cont = CCNode::create();
		cont->setContentSize(size);

		Pill p;
		p.bg = box(size, ccc3(255, 255, 255), 40);
		p.bg->setPosition({ size.width / 2.f, size.height / 2.f });
		cont->addChild(p.bg, 0);

		p.lbl = CCLabelBMFont::create(text, "bigFont.fnt");
		p.lbl->setScale(textScale);
		p.lbl->setPosition({ size.width / 2.f, size.height / 2.f + 0.5f });
		cont->addChild(p.lbl, 1);

		p.cont = cont;
		auto btn = CCMenuItemSpriteExtra::create(cont, this, cb);
		btn->setTag(tag);
		btn->setPosition(pos);
		m_menu->addChild(btn);
		return p;
	}

	static void stylePill(Pill& p, bool on, bool animate, bool pulse) {
		if (!p.bg || !p.cont) return;
		tint(p.bg, on ? th().accent : ccc3(255, 255, 255), animate);
		fade(p.bg, on ? 255 : 40, animate);
		tint(p.lbl, on ? ccc3(255, 255, 255) : th().muted, animate);

		if (animate && pulse) {
			p.cont->stopActionByTag(77);
			auto seq = CCSequence::create(
				CCScaleTo::create(0.07f, 1.12f),
				CCEaseBackOut::create(CCScaleTo::create(0.22f, 1.f)),
				nullptr
			);
			seq->setTag(77);
			p.cont->runAction(seq);
		}
	}

	static int selectedWindow() { return ib::enabled ? ib::level + 1 : 0; }

	static bool toggleValue(int i) {
		switch (i) {
			case 0: return ib::orbs;
			case 1: return ib::ground;
			case 2: return ib::practiceOnly;
			case 3: return ib::flash;
			default: return ib::hud;
		}
	}

	// ---- build -----------------------------------------------------
	bool init() override {
		if (!Popup::init(W, H)) return false;
		s_open = this;

		// use our own themed panel instead of the vanilla popup background
		if (m_bgSprite) m_bgSprite->setVisible(false);

		auto dim = CCLayerColor::create(ccc4(0, 0, 0, 110), W, H);
		auto win = CCDirector::sharedDirector()->getWinSize();
		dim->setContentSize(win);
		dim->setAnchorPoint({ 0.f, 0.f });
		dim->setPosition({ 0.f, 0.f });
		this->addChild(dim, -1);

		m_panel = CCScale9Sprite::create("GJ_square01.png");
		m_panel->setContentSize({ W, H });
		m_panel->setPosition({ W / 2.f, H / 2.f });
		m_mainLayer->addChild(m_panel, 0);

		m_menu = CCMenu::create();
		m_menu->setPosition({ 0.f, 0.f });
		m_mainLayer->addChild(m_menu, 20);

		// ---- header ----
		this->setTitle("Input Buffer", "bigFont.fnt", 0.62f, 24.f);

		m_chipBG = box({ 54.f, 22.f }, th().accent, 255);
		m_chipBG->setPosition(P(118.f, 136.f));
		m_mainLayer->addChild(m_chipBG, 5);

		m_chipLbl = addLabel("OFF", 0.42f, P(118.f, 136.5f), { 0.5f, 0.5f }, false);
		m_chipLbl->setZOrder(6);

		// ---- buffer window card ----
		addCard({ 356.f, 94.f }, P(0.f, 72.f));
		addHeader("BUFFER WINDOW", P(-170.f, 112.f));

		for (int i = 0; i < WIN_PILLS; i++) {
			std::string text = (i == 0) ? "Off" : std::to_string(ib::MS[i - 1]);
			m_win[i] = makePill(
				{ 46.f, 24.f }, text.c_str(), 0.4f, i,
				menu_selector(BufferPopup::onWindow),
				P(-137.5f + i * 55.f, 90.f)
			);
		}

		m_bigNum = addLabel("OFF", 0.62f, P(0.f, 64.f), { 0.5f, 0.5f }, false);
		m_info = addLabel("", 0.62f, P(0.f, 33.f), { 0.5f, 0.5f }, true, "chatFont.fnt");

		m_track = box({ BAR_W, BAR_H }, ccc3(255, 255, 255), 45);
		m_track->setAnchorPoint({ 0.f, 0.5f });
		m_track->setPosition(P(-BAR_W / 2.f, BAR_Y));
		m_mainLayer->addChild(m_track, 5);

		m_fill = box({ BAR_H, BAR_H }, th().accent, 255);
		m_fill->setAnchorPoint({ 0.f, 0.5f });
		m_fill->setPosition(P(-BAR_W / 2.f, BAR_Y));
		m_fill->setVisible(false);
		m_mainLayer->addChild(m_fill, 6);

		// small ticks where each selectable window sits on the meter
		for (int i = 0; i < ib::LEVELS; i++) {
			auto tick = box({ 2.f, 13.f }, ccc3(255, 255, 255), 120);
			tick->setAnchorPoint({ 0.5f, 0.5f });
			tick->setPosition(P(-BAR_W / 2.f + BAR_W * ib::MS[i] / ib::MAX_MS, BAR_Y));
			m_mainLayer->addChild(tick, 7);
		}

		// ---- apply-to card ----
		addCard({ 356.f, 52.f }, P(0.f, -9.f));
		addHeader("APPLY TO", P(-170.f, 8.f));

		static char const* togText[TOG_PILLS] = { "Orbs", "Ground", "Practice", "Flash", "HUD" };
		for (int i = 0; i < TOG_PILLS; i++) {
			m_tog[i] = makePill(
				{ 66.f, 24.f }, togText[i], 0.32f, 100 + i,
				menu_selector(BufferPopup::onToggle),
				P(-140.f + i * 70.f, -18.f)
			);
		}

		// ---- theme / stats card ----
		addCard({ 356.f, 60.f }, P(0.f, -88.f));
		addHeader("THEME", P(-170.f, -68.f));

		for (int i = 0; i < ib::THEME_COUNT; i++) {
			auto cont = CCNode::create();
			cont->setContentSize({ 26.f, 26.f });

			auto art = CCDrawNode::create();
			art->setPosition({ 13.f, 13.f });
			cont->addChild(art, 0);

			auto btn = CCMenuItemSpriteExtra::create(cont, this, menu_selector(BufferPopup::onTheme));
			btn->setTag(200 + i);
			btn->setPosition(P(-75.f + i * 30.f, -90.f));
			m_menu->addChild(btn);

			m_dots[i] = { cont, art };
		}

		m_savedLbl = addLabel("Clicks saved: 0", 0.62f, P(0.f, -126.f), { 0.5f, 0.5f }, true, "chatFont.fnt");
		m_savedLbl->setString(("Clicks saved: " + std::to_string(ib::saved)).c_str());

		auto hint = addLabel("Shift opens / closes this panel", 0.55f, P(0.f, -146.f), { 0.5f, 0.5f }, true, "chatFont.fnt");
		hint->setOpacity(140);

		this->applyTheme(false);
		this->refreshText(false);

		m_msCur = 0.f; // the number counts up from zero when the panel opens
		this->schedule(schedule_selector(BufferPopup::tick));

		this->playOpenAnimation();
		return true;
	}

	// ---- state -----------------------------------------------------
	void styleChip(bool animate) {
		bool on = ib::enabled;
		tint(m_chipBG, on ? th().accent : ccc3(255, 255, 255), animate);
		fade(m_chipBG, on ? 255 : 45, animate);
		tint(m_chipLbl, on ? ccc3(255, 255, 255) : th().muted, animate);
		m_chipLbl->setString(on ? "ON" : "OFF");
	}

	void drawDot(int i) {
		auto& d = m_dots[i];
		if (!d.art) return;
		bool on = (i == ib::theme);
		d.art->clear();
		if (on) d.art->drawDot({ 0.f, 0.f }, 12.f, ccc4f(1.f, 1.f, 1.f, 0.9f));
		d.art->drawDot({ 0.f, 0.f }, 9.f, ib::rgba(ib::THEMES[i].accent, 1.f));
		if (d.cont) d.cont->setScale(on ? 1.08f : 1.f);
	}

	// recolor everything for the current theme (fades when animate is true)
	void applyTheme(bool animate) {
		tint(m_panel, th().panel, animate);
		tint(m_track, ccc3(255, 255, 255), animate);

		for (auto c : m_cards) { tint(c, ccc3(255, 255, 255), animate); fade(c, 40, animate); }
		for (auto l : m_mutedLabels) tint(l, th().muted, animate);
		tint(m_fill, th().accent, animate);
		tint(m_bigNum, ib::enabled ? th().accent : th().muted, animate);

		int sel = selectedWindow();
		for (int i = 0; i < WIN_PILLS; i++) stylePill(m_win[i], i == sel, animate, false);
		for (int i = 0; i < TOG_PILLS; i++) stylePill(m_tog[i], toggleValue(i), animate, false);

		this->styleChip(animate);
		for (int i = 0; i < ib::THEME_COUNT; i++) this->drawDot(i);
	}

	// meter + big number follow the selected window
	void refreshText(bool animate) {
		if (!ib::enabled) {
			m_msTarget = 0.f;
			m_info->setString("Buffering is off");
		} else {
			int ms = ib::MS[ib::level];
			m_msTarget = static_cast<float>(ms);
			m_info->setString(("Saves clicks up to " + std::to_string(ms) + " ms early").c_str());
		}
		tint(m_bigNum, ib::enabled ? th().accent : th().muted, animate);

		if (animate) {
			m_info->stopActionByTag(56);
			m_info->setOpacity(0);
			auto a = CCFadeTo::create(0.22f, 255);
			a->setTag(56);
			m_info->runAction(a);
		}
		m_savedLbl->setString(("Clicks saved: " + std::to_string(ib::saved)).c_str());
	}

	void tick(float dt) {
		float k = 1.f - std::exp(-12.f * dt);
		m_msCur += (m_msTarget - m_msCur) * k;
		if (std::abs(m_msTarget - m_msCur) < 0.4f) m_msCur = m_msTarget;

		int shown = static_cast<int>(std::lround(m_msCur));
		if (shown != m_shownMs) {
			m_shownMs = shown;
			m_bigNum->setString(shown <= 0 ? "OFF" : (std::to_string(shown) + " MS").c_str());
		}

		float w = m_msCur / static_cast<float>(ib::MAX_MS) * BAR_W;
		bool show = w > 1.f;
		m_fill->setVisible(show);
		if (show) m_fill->setContentSize({ w, BAR_H });
	}

	// ---- animations ------------------------------------------------
	static void popIn(CCNode* node, float delay) {
		if (!node) return;
		node->setScale(0.f);
		node->runAction(CCSequence::create(
			CCDelayTime::create(delay),
			CCEaseBackOut::create(CCScaleTo::create(0.24f, 1.f)),
			nullptr
		));
	}

	void playOpenAnimation() {
		for (int i = 0; i < WIN_PILLS; i++) popIn(m_win[i].cont, 0.06f + 0.03f * i);
		for (int i = 0; i < TOG_PILLS; i++) popIn(m_tog[i].cont, 0.22f + 0.03f * i);
		for (int i = 0; i < ib::THEME_COUNT; i++) popIn(m_dots[i].cont, 0.38f + 0.03f * i);
	}

	// ---- callbacks -------------------------------------------------
	void onWindow(CCObject* sender) {
		int oldSel = selectedWindow();
		int i = static_cast<CCNode*>(sender)->getTag();
		if (i == 0) {
			ib::setEnabled(false);
		} else {
			ib::setLevel(i - 1);
			ib::setEnabled(true);
		}
		ib::reset();

		int sel = selectedWindow();
		for (int j = 0; j < WIN_PILLS; j++) {
			if (j == sel || j == oldSel) stylePill(m_win[j], j == sel, true, j == sel);
		}
		this->styleChip(true);
		this->refreshText(true);
	}

	void onToggle(CCObject* sender) {
		int i = static_cast<CCNode*>(sender)->getTag() - 100;
		if (i < 0 || i >= TOG_PILLS) return;
		switch (i) {
			case 0: ib::setOrbs(!ib::orbs); break;
			case 1: ib::setGround(!ib::ground); break;
			case 2: ib::setPracticeOnly(!ib::practiceOnly); break;
			case 3: ib::setFlash(!ib::flash); break;
			default: ib::setHud(!ib::hud); break;
		}
		ib::reset();
		stylePill(m_tog[i], toggleValue(i), true, true);
	}

	void onTheme(CCObject* sender) {
		int i = static_cast<CCNode*>(sender)->getTag() - 200;
		if (i < 0 || i >= ib::THEME_COUNT || i == ib::theme) return;
		ib::setTheme(i);
		this->applyTheme(true);
	}

	void onClose(CCObject* sender) override {
		if (m_closing) return;
		m_closing = true;
		ib::flushSaved();
		Popup::onClose(sender);
	}

public:
	~BufferPopup() override {
		if (s_open == this) s_open = nullptr;
	}

	static bool isOpen() { return s_open != nullptr; }

	// open the panel, or close it if it is already up
	static void toggle() {
		if (s_open) {
			if (!s_open->m_closing) s_open->onClose(nullptr);
			return;
		}
		if (auto popup = BufferPopup::create()) popup->show();
	}

	static BufferPopup* create() {
		auto ret = new BufferPopup();
		if (ret && ret->init()) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}
};
