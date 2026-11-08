#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "Buffer.hpp"

using namespace geode::prelude;

// Themed glass popup:
//  - Off / 50 / 90 / 130 / 180 / 250 ms pills
//  - a window meter with level ticks and a number that counts up/down
//  - Orbs / Ground / Practice / Flash toggles
//  - 6 themes you can switch live (colors fade between themes)
class BufferPopup : public FLAlertLayer {
protected:
	static constexpr int WIN_PILLS = ib::LEVELS + 1; // Off + 5 windows
	static constexpr int TOG_PILLS = 4;              // Orbs, Ground, Practice, Flash

	static constexpr float BAR_W = 300.f;
	static constexpr float BAR_H = 8.f;
	static constexpr float BAR_Y = 0.f;

	struct Pill {
		CCMenuItemSpriteExtra* btn = nullptr;
		CCScale9Sprite* bg = nullptr;
		CCLabelBMFont* lbl = nullptr;
	};

	struct Dot {
		CCMenuItemSpriteExtra* btn = nullptr;
		CCSprite* ring = nullptr;
		CCSprite* body = nullptr;
	};

	// the popup that's currently on screen (so Shift can toggle it)
	inline static BufferPopup* s_open = nullptr;

	CCNode* m_root = nullptr;

	// themed pieces
	CCScale9Sprite* m_panel = nullptr;
	CCScale9Sprite* m_frame = nullptr;
	CCScale9Sprite* m_track = nullptr;
	CCScale9Sprite* m_chipBG = nullptr;
	CCLabelBMFont* m_chipLbl = nullptr;
	std::vector<CCScale9Sprite*> m_cards;
	std::vector<CCLabelBMFont*> m_mutedLabels;
	std::vector<CCScale9Sprite*> m_accentBoxes;
	std::vector<CCLabelBMFont*> m_accentLabels;

	std::array<Pill, WIN_PILLS> m_win{};
	std::array<Pill, TOG_PILLS> m_tog{};
	std::array<Dot, ib::THEME_COUNT> m_dots{};

	CCLabelBMFont* m_title = nullptr;
	CCLabelBMFont* m_info = nullptr;
	CCScale9Sprite* m_barFill = nullptr;

	float m_msCur = 0.f;     // number shown (counts toward target)
	float m_msTarget = 0.f;
	int m_shownMs = -1;
	bool m_closing = false;

	// ---- theme helpers ---------------------------------------------
	static const ib::Theme& th() { return ib::currentTheme(); }

	template <class T>
	static void tintNode(T* n, ccColor3B c, bool animate) {
		if (!n) return;
		n->stopActionByTag(55);
		if (!animate) { n->setColor(c); return; }
		auto a = CCTintTo::create(0.25f, c.r, c.g, c.b);
		a->setTag(55);
		n->runAction(a);
	}

	template <class T>
	static void opacityNode(T* n, GLubyte op, bool animate) {
		if (!n) return;
		n->stopActionByTag(56);
		if (!animate) { n->setOpacity(op); return; }
		auto a = CCFadeTo::create(0.25f, op);
		a->setTag(56);
		n->runAction(a);
	}

	static CCScale9Sprite* box(CCSize size, ccColor3B col, GLubyte opacity) {
		auto s = CCScale9Sprite::create("square02_001.png", { 0.f, 0.f, 80.f, 80.f });
		s->setContentSize(size);
		s->setColor(col);
		s->setOpacity(opacity);
		return s;
	}

	CCScale9Sprite* addCard(CCSize size, CCPoint pos, int z = 4) {
		auto c = box(size, th().card, th().cardOp);
		c->setPosition(pos);
		m_root->addChild(c, z);
		m_cards.push_back(c);
		return c;
	}

	CCLabelBMFont* addLabel(const char* text, const char* font, float scale, CCPoint pos, CCPoint anchor, bool muted = true) {
		auto l = CCLabelBMFont::create(text, font);
		l->setScale(scale);
		l->setAnchorPoint(anchor);
		l->setPosition(pos);
		m_root->addChild(l, 5);
		if (muted) {
			l->setColor(th().muted);
			m_mutedLabels.push_back(l);
		}
		return l;
	}

	CCLabelBMFont* addHeader(const char* text, CCPoint pos) {
		return addLabel(text, "bigFont.fnt", 0.32f, pos, { 0.f, 0.5f });
	}

	Pill makePill(CCSize size, const char* text, float textScale, int tag, SEL_MenuHandler cb, CCPoint pos) {
		auto cont = CCNode::create();
		cont->setContentSize(size);

		Pill p;
		p.bg = box(size, th().pill, th().pillOp);
		p.bg->setPosition({ size.width / 2.f, size.height / 2.f });
		cont->addChild(p.bg, 0);

		p.lbl = CCLabelBMFont::create(text, "bigFont.fnt");
		p.lbl->setScale(textScale);
		p.lbl->setPosition({ size.width / 2.f, size.height / 2.f + 0.5f });
		cont->addChild(p.lbl, 1);

		p.btn = CCMenuItemSpriteExtra::create(cont, this, cb);
		p.btn->setTag(tag);
		p.btn->setPosition(pos);
		m_buttonMenu->addChild(p.btn);
		return p;
	}

	static void stylePill(Pill& p, bool on, bool animate, bool pulse) {
		tintNode(p.bg, on ? th().accent : th().pill, animate);
		opacityNode(p.bg, on ? 255 : th().pillOp, animate);
		tintNode(p.lbl, on ? ccc3(255, 255, 255) : th().muted, animate);

		if (animate && pulse) {
			auto img = p.btn->getNormalImage();
			img->stopActionByTag(77);
			auto seq = CCSequence::create(
				CCScaleTo::create(0.07f, 1.14f),
				CCEaseBackOut::create(CCScaleTo::create(0.22f, 1.f)),
				nullptr
			);
			seq->setTag(77);
			img->runAction(seq);
		}
	}

	static int selectedWindow() {
		return ib::enabled ? ib::level + 1 : 0;
	}

	static bool toggleValue(int i) {
		switch (i) {
			case 0: return ib::orbs;
			case 1: return ib::ground;
			case 2: return ib::practiceOnly;
			default: return ib::flash;
		}
	}

	// ---- build -----------------------------------------------------
	bool init() {
		if (!FLAlertLayer::init(150)) return false;

		s_open = this;
		m_noElasticity = true;
		this->setTouchEnabled(true);
		this->setKeypadEnabled(true);

		auto win = CCDirector::sharedDirector()->getWinSize();

		// everything lives under m_root so the panel scales around its centre
		m_root = CCNode::create();
		m_root->setPosition({ win.width / 2.f, win.height / 2.f });
		m_mainLayer->addChild(m_root);

		m_buttonMenu = CCMenu::create();
		m_buttonMenu->setPosition({ 0.f, 0.f });
		m_buttonMenu->setTouchPriority(-504);
		m_root->addChild(m_buttonMenu, 10);

		// shadow + frame + panel + breathing glass sheen
		auto shadow = box({ 368.f, 276.f }, ccc3(0, 0, 0), 70);
		shadow->setPosition({ 0.f, -4.f });
		m_root->addChild(shadow, 0);

		m_frame = box({ 364.f, 272.f }, th().frame, 235);
		m_root->addChild(m_frame, 1);

		m_panel = box({ 354.f, 262.f }, th().panel, 255);
		m_root->addChild(m_panel, 2);

		auto sheen = box({ 340.f, 64.f }, ccc3(255, 255, 255), 45);
		sheen->setPosition({ 0.f, 98.f });
		sheen->runAction(CCRepeatForever::create(CCSequence::create(
			CCFadeTo::create(2.4f, 25),
			CCFadeTo::create(2.4f, 60),
			nullptr
		)));
		m_root->addChild(sheen, 3);

		// ---- header ----
		auto title = addLabel("Input Buffer", "bigFont.fnt", 0.55f, { -160.f, 112.f }, { 0.f, 0.5f }, false);
		title->setColor(th().accent);
		m_accentLabels.push_back(title);

		// ON / OFF chip
		m_chipBG = box({ 44.f, 20.f }, th().pill, th().pillOp);
		m_chipBG->setPosition({ 104.f, 112.f });
		m_root->addChild(m_chipBG, 5);
		m_chipLbl = addLabel("OFF", "bigFont.fnt", 0.36f, { 104.f, 112.5f }, { 0.5f, 0.5f }, false);
		m_chipLbl->setZOrder(6);

		auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
		closeSpr->setScale(0.65f);
		auto closeBtn = CCMenuItemSpriteExtra::create(closeSpr, this, menu_selector(BufferPopup::onCloseBtn));
		closeBtn->setPosition({ 156.f, 112.f });
		m_buttonMenu->addChild(closeBtn);

		addCard({ 330.f, 2.f }, { 0.f, 95.f });

		// ---- buffer window card ----
		addCard({ 330.f, 100.f }, { 0.f, 40.f });
		addHeader("BUFFER WINDOW", { -152.f, 78.f });

		for (int i = 0; i < WIN_PILLS; i++) {
			std::string text = i == 0 ? "Off" : std::to_string(ib::MS[i - 1]);
			m_win[i] = makePill(
				{ 42.f, 26.f }, text.c_str(), 0.42f, i,
				menu_selector(BufferPopup::onWindow),
				{ (i - 2.5f) * 50.f, 56.f }
			);
		}

		m_title = addLabel("", "bigFont.fnt", 0.6f, { 0.f, 28.f }, { 0.5f, 0.5f }, false);
		m_info = addLabel("", "chatFont.fnt", 0.8f, { 0.f, 13.f }, { 0.5f, 0.5f });

		m_track = box({ BAR_W, BAR_H }, th().pill, th().pillOp);
		m_track->setPosition({ 0.f, BAR_Y });
		m_root->addChild(m_track, 5);

		m_barFill = box({ BAR_H, BAR_H }, th().accent, 255);
		m_barFill->setPosition({ -BAR_W / 2.f, BAR_Y });
		m_root->addChild(m_barFill, 6);
		m_accentBoxes.push_back(m_barFill);

		// tick marks where each level sits on the bar
		for (int i = 0; i < ib::LEVELS; i++) {
			auto tick = box({ 3.f, 12.f }, ccc3(255, 255, 255), 150);
			tick->setPosition({ -BAR_W / 2.f + BAR_W * ib::MS[i] / ib::MAX_MS - 1.5f, BAR_Y });
			m_root->addChild(tick, 7);
		}

		// ---- apply-to card ----
		addCard({ 330.f, 56.f }, { 0.f, -44.f });
		addHeader("APPLY TO", { -152.f, -25.f });

		const char* togText[TOG_PILLS] = { "Orbs", "Ground", "Practice", "Flash" };
		for (int i = 0; i < TOG_PILLS; i++) {
			m_tog[i] = makePill(
				{ 76.f, 26.f }, togText[i], 0.38f, 100 + i,
				menu_selector(BufferPopup::onToggle),
				{ (i - 1.5f) * 82.f, -52.f }
			);
		}

		// ---- footer: themes + saved counter ----
		addCard({ 330.f, 40.f }, { 0.f, -98.f });
		addHeader("THEME", { -152.f, -98.f });

		for (int i = 0; i < ib::THEME_COUNT; i++) {
			auto cont = CCNode::create();
			cont->setContentSize({ 24.f, 24.f });

			auto ring = CCSprite::createWithSpriteFrameName("smallDot.png");
			auto body = CCSprite::createWithSpriteFrameName("smallDot.png");
			if (ring && body) {
				ring->setScale(23.f / ring->getContentSize().width);
				body->setScale(15.f / body->getContentSize().width);
				ring->setPosition({ 12.f, 12.f });
				body->setPosition({ 12.f, 12.f });
				body->setColor(ib::THEMES[i].accent);
				cont->addChild(ring, 0);
				cont->addChild(body, 1);
			}

			auto btn = CCMenuItemSpriteExtra::create(cont, this, menu_selector(BufferPopup::onTheme));
			btn->setTag(200 + i);
			btn->setPosition({ -92.f + i * 28.f, -98.f });
			m_buttonMenu->addChild(btn);

			m_dots[i] = { btn, ring, body };
		}

		addLabel(("Clicks saved: " + std::to_string(ib::saved)).c_str(), "chatFont.fnt", 0.8f, { 150.f, -98.f }, { 1.f, 0.5f });

		this->applyTheme(false);
		this->refreshText(false);

		m_msCur = 0.f; // number counts up from zero on open
		this->schedule(schedule_selector(BufferPopup::tick));

		this->playOpenAnimation();
		return true;
	}

	// ---- state -----------------------------------------------------
	void styleChip(bool animate) {
		bool on = ib::enabled;
		tintNode(m_chipBG, on ? th().accent : th().pill, animate);
		opacityNode(m_chipBG, on ? 255 : th().pillOp, animate);
		tintNode(m_chipLbl, on ? ccc3(255, 255, 255) : th().muted, animate);
		m_chipLbl->setString(on ? "ON" : "OFF");
	}

	void styleDots(bool animate) {
		for (int i = 0; i < ib::THEME_COUNT; i++) {
			auto& d = m_dots[i];
			if (!d.ring) continue;
			bool on = (i == ib::theme);
			d.ring->setColor(th().muted);
			opacityNode(d.ring, on ? 255 : 0, animate);
			if (animate && on) {
				auto img = d.btn->getNormalImage();
				img->stopActionByTag(77);
				auto seq = CCSequence::create(
					CCScaleTo::create(0.07f, 1.25f),
					CCEaseBackOut::create(CCScaleTo::create(0.25f, 1.f)),
					nullptr
				);
				seq->setTag(77);
				img->runAction(seq);
			}
		}
	}

	// recolor everything for the current theme (fades if animate)
	void applyTheme(bool animate) {
		tintNode(m_panel, th().panel, animate);
		tintNode(m_frame, th().frame, animate);
		tintNode(m_track, th().pill, animate);
		opacityNode(m_track, th().pillOp, animate);

		for (auto c : m_cards) {
			tintNode(c, th().card, animate);
			opacityNode(c, th().cardOp, animate);
		}
		for (auto l : m_mutedLabels) tintNode(l, th().muted, animate);
		for (auto b : m_accentBoxes) tintNode(b, th().accent, animate);
		for (auto l : m_accentLabels) tintNode(l, th().accent, animate);

		int sel = selectedWindow();
		for (int i = 0; i < WIN_PILLS; i++) stylePill(m_win[i], i == sel, animate, false);
		for (int i = 0; i < TOG_PILLS; i++) stylePill(m_tog[i], toggleValue(i), animate, false);

		tintNode(m_title, ib::enabled ? th().accent : th().muted, animate);
		this->styleChip(animate);
		this->styleDots(animate);
	}

	// text, number target and meter for the current window
	void refreshText(bool animate) {
		if (!ib::enabled) {
			m_msTarget = 0.f;
			m_info->setString("Early clicks are not saved");
		} else {
			int ms = ib::MS[ib::level];
			m_msTarget = static_cast<float>(ms);
			m_info->setString(("Saves clicks made up to " + std::to_string(ms) + " ms early").c_str());
		}
		tintNode(m_title, ib::enabled ? th().accent : th().muted, animate);

		if (animate) {
			m_info->stopActionByTag(56);
			m_info->setOpacity(0);
			auto a = CCFadeTo::create(0.22f, 255);
			a->setTag(56);
			m_info->runAction(a);
		}
	}

	// number counts toward the target and the meter glides with it
	void tick(float dt) {
		float k = 1.f - std::exp(-12.f * dt);
		m_msCur += (m_msTarget - m_msCur) * k;
		if (std::abs(m_msTarget - m_msCur) < 0.4f) m_msCur = m_msTarget;

		int shown = static_cast<int>(std::lround(m_msCur));
		if (shown != m_shownMs) {
			m_shownMs = shown;
			m_title->setString(shown <= 0 ? "OFF" : (std::to_string(shown) + " MS").c_str());
		}

		float w = m_msCur / ib::MAX_MS * BAR_W;
		bool show = w > BAR_H;
		m_barFill->setVisible(show);
		if (show) {
			m_barFill->setContentSize({ w, BAR_H });
			m_barFill->setPosition({ -BAR_W / 2.f + w / 2.f, BAR_Y });
		}
	}

	// ---- animations ------------------------------------------------
	static void popIn(CCNode* node, float delay) {
		if (!node) return;
		node->setScale(0.f);
		node->runAction(CCSequence::create(
			CCDelayTime::create(delay),
			CCEaseBackOut::create(CCScaleTo::create(0.25f, 1.f)),
			nullptr
		));
	}

	void playOpenAnimation() {
		this->setOpacity(0);
		this->runAction(CCFadeTo::create(0.2f, 150));

		m_root->setScale(0.82f);
		m_root->runAction(CCEaseBackOut::create(CCScaleTo::create(0.28f, 1.f)));

		for (int i = 0; i < WIN_PILLS; i++) popIn(m_win[i].btn->getNormalImage(), 0.08f + 0.04f * i);
		for (int i = 0; i < TOG_PILLS; i++) popIn(m_tog[i].btn->getNormalImage(), 0.3f + 0.04f * i);
		for (int i = 0; i < ib::THEME_COUNT; i++) popIn(m_dots[i].btn->getNormalImage(), 0.45f + 0.04f * i);
	}

	void finishClose() {
		this->setKeypadEnabled(false);
		this->removeFromParentAndCleanup(true);
	}

	void close() {
		if (m_closing) return;
		m_closing = true;
		m_buttonMenu->setEnabled(false);

		this->runAction(CCFadeTo::create(0.14f, 0));
		m_root->runAction(CCSequence::create(
			CCEaseIn::create(CCScaleTo::create(0.14f, 0.88f), 2.f),
			CCCallFunc::create(this, callfunc_selector(BufferPopup::finishClose)),
			nullptr
		));
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
		if (i == 0) ib::setOrbs(!ib::orbs);
		else if (i == 1) ib::setGround(!ib::ground);
		else if (i == 2) ib::setPracticeOnly(!ib::practiceOnly);
		else ib::setFlash(!ib::flash);
		ib::reset();
		stylePill(m_tog[i], toggleValue(i), true, true);
	}

	void onTheme(CCObject* sender) {
		int i = static_cast<CCNode*>(sender)->getTag() - 200;
		if (i == ib::theme) return;
		ib::setTheme(i);
		this->applyTheme(true);
	}

	void onCloseBtn(CCObject*) {
		this->close();
	}

	void keyBackClicked() override {
		this->close();
	}

public:
	~BufferPopup() override {
		if (s_open == this) s_open = nullptr;
	}

	// open the panel, or close it if it's already open
	static void toggle() {
		if (s_open) {
			if (!s_open->m_closing) s_open->close();
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
