#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/cocos.hpp>
#include "Buffer.hpp"
#include "BufferPopup.hpp"

using namespace geode::prelude;

// Load the saved settings as soon as the mod loads.
$execute {
	ib::load();
}

// ---------------------------------------------------------------------------
// Small helpers shared by the hooks below.
// ---------------------------------------------------------------------------
namespace {
	bool alive(PlayerObject* p) {
		return p && !p->m_isDead;
	}

	// Modes where a click while touching the ground actually does something.
	// Ship / UFO / wave / swing only click in the air, so landings are ignored.
	bool groundClicks(PlayerObject* p) {
		return !p->m_isShip && !p->m_isBird && !p->m_isDart && !p->m_isSwing;
	}

	bool touchingOrb(PlayerObject* p) {
		if (!p->m_touchedRings.empty()) return true;
		return p->m_touchingRings && p->m_touchingRings->count() > 0;
	}

	// Would a click do something on THIS frame? If so we let the game have it.
	bool canActNow(PlayerObject* p) {
		if (!alive(p)) return false;
		if (ib::orbs && touchingOrb(p)) return true;
		if (ib::ground && groundClicks(p) && p->m_isOnGround) return true;
		return false;
	}

	bool active(PlayLayer* pl) {
		if (!ib::enabled || !pl) return false;
		if (ib::practiceOnly && !pl->m_isPracticeMode) return false;
		return true;
	}

	// Two kinds of "first valid frame": a fresh orb in range, or a landing.
	struct Chance {
		bool orb = false;
		bool ground = false;
	};

	Chance detectChance(PlayerObject* p, ib::State& s) {
		Chance c;

		bool orbTouch = touchingOrb(p);
		if (orbTouch) {
			if (!p->m_touchedRings.empty()) {
				// ids let us tell a brand new orb from the one we just used
				int id = *p->m_touchedRings.begin();
				if (s.lastRing != id) { c.orb = true; s.lastRing = id; }
			} else if (!s.orbTouch) {
				c.orb = true;
			}
		} else {
			s.lastRing = -1;
		}
		s.orbTouch = orbTouch;

		// a landing is the rising edge of "on the ground", so one saved click
		// can never fire twice while you keep standing there
		bool onGround = p->m_isOnGround && groundClicks(p);
		if (onGround && !s.wasOnGround) c.ground = true;
		s.wasOnGround = onGround;

		return c;
	}
}

// ---------------------------------------------------------------------------
// Main menu button + Shift shortcut.
// ---------------------------------------------------------------------------
namespace {
	bool g_shiftDown = false;

	bool findAlert(CCNode* node) {
		if (!node) return false;
		if (typeinfo_cast<FLAlertLayer>(node)) return true;
		auto kids = node->getChildren();
		if (!kids) return false;
		for (unsigned i = 0; i < kids->count(); i++) {
			if (findAlert(static_cast<CCNode*>(kids->objectAtIndex(i)))) return true;
		}
		return false;
	}

	bool anotherPopupIsOpen() {
		return findAlert(CCDirector::sharedDirector()->getRunningScene());
	}

	void pollShift() {
		bool down = CCKeyboardDispatcher::get()->getShiftKeyPressed();
		bool pressed = down && !g_shiftDown;
		g_shiftDown = down;
		if (!pressed) return;

		if (BufferPopup::isOpen()) {
			BufferPopup::toggle();
			return;
		}
		// never open on top of another popup (settings, alerts, ...)
		if (anotherPopupIsOpen()) return;
		BufferPopup::toggle();
	}

	CCMenuItemSpriteExtra* makeBufferButton(CCObject* target, SEL_MenuHandler cb) {
		auto spr = CircleButtonSprite::create(
			CCLabelBMFont::create("IB", "bigFont.fnt"),
			CircleBaseColor::Blue,
			CircleBaseSize::Medium
		);
		auto btn = CCMenuItemSpriteExtra::create(spr, target, cb);
		btn->setID("input-buffer-button"_spr);
		return btn;
	}
}

#include <Geode/modify/MenuLayer.hpp>
class $modify(IBMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;

		auto btn = makeBufferButton(this, menu_selector(IBMenuLayer::onBuffer));

		if (auto menu = this->getChildByID("bottom-menu")) {
			menu->addChild(btn);
			menu->updateLayout();
		} else {
			// fallback: pin it to the bottom left corner if the menu id changed
			auto corner = CCMenu::create();
			corner->setPosition({ 30.f, 30.f });
			corner->addChild(btn);
			this->addChild(corner, 10);
		}

		this->schedule(schedule_selector(IBMenuLayer::pollShift));
		return true;
	}

	void pollShift(float) {
		::pollShift();
	}

	void onBuffer(CCObject*) {
		BufferPopup::toggle();
	}
};

// ---------------------------------------------------------------------------
// Pause menu button + Shift shortcut.
// ---------------------------------------------------------------------------
#include <Geode/modify/PauseLayer.hpp>
class $modify(IBPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto btn = makeBufferButton(this, menu_selector(IBPauseLayer::onBuffer));

		if (auto menu = this->getChildByID("right-button-menu")) {
			menu->addChild(btn);
			menu->updateLayout();
		} else {
			auto win = CCDirector::sharedDirector()->getWinSize();
			auto corner = CCMenu::create();
			corner->setPosition({ win.width - 32.f, 60.f });
			btn->setPosition({ 0.f, 0.f });
			corner->addChild(btn);
			this->addChild(corner, 10);
		}

		this->schedule(schedule_selector(IBPauseLayer::pollShift));
	}

	void pollShift(float) {
		::pollShift();
	}

	void onBuffer(CCObject*) {
		BufferPopup::toggle();
	}
};

// ---------------------------------------------------------------------------
// The buffer itself.
//
//  1. You click. If the game can already use it (touching an orb, standing on
//     the ground) the click is passed through untouched. If it can't, we save
//     it instead of throwing it away.
//  2. Every frame we look for the first valid moment - a new orb in range or
//     the frame you touch down - and send one fresh click right then.
//  3. Saved clicks older than the window are dropped, so a buffered click
//     never fires somewhere random seconds later.
//  4. One saved click per orb / per landing edge, and never while you are
//     physically holding the button, so nothing fights your own input.
// ---------------------------------------------------------------------------
#include <Geode/modify/GJBaseGameLayer.hpp>
class $modify(IBGameLayer, GJBaseGameLayer) {
	struct Fields {
		cocos2d::CCDrawNode* hud = nullptr;
	};

	// send a click straight to the game without letting our own hook see it
	void sendClick(bool down, bool isPlayer1) {
		ib::injecting = true;
		GJBaseGameLayer::handleButton(down, 1, isPlayer1);
		ib::injecting = false;
	}

	void handleButton(bool down, int button, bool isPlayer1) {
		GJBaseGameLayer::handleButton(down, button, isPlayer1);

		if (button != 1 || ib::injecting) return;
		auto pl = typeinfo_cast<PlayLayer*>(this); // the editor has no PlayLayer
		if (!active(pl)) return;

		auto& s = ib::st[isPlayer1 ? 0 : 1];
		if (!down) {
			if (s.held > 0) s.held--;
			return;
		}

		s.held++;
		auto p = isPlayer1 ? m_player1 : m_player2;
		if (!alive(p)) return;
		if (canActNow(p)) return; // the game uses this click right away
		s.push();                 // too early: keep it for later
	}

	void update(float dt) {
		auto pl = typeinfo_cast<PlayLayer*>(this);

		if (active(pl)) {
			float ms = dt * 1000.f;

			for (int i = 0; i < 2; i++) {
				bool p1 = (i == 0);
				auto& s = ib::st[i];
				auto p = p1 ? m_player1 : m_player2;
				if (!alive(p)) { s = ib::State{}; continue; }

				// finish the synthetic tap we started last frame
				if (s.tapRelease) {
					s.tapRelease = false;
					this->sendClick(false, p1);
				}

				s.sinceFire += ms;
				s.age(ms, ib::windowMs());

				auto chance = detectChance(p, s);
				if (!chance.orb && !chance.ground) continue;
				if (s.count == 0 || s.held > 0) continue;
				if (s.sinceFire < ib::REARM_MS) continue;

				// first valid frame: fire one saved click
				this->sendClick(true, p1);
				s.tapRelease = true;
				s.popFront();
				s.sinceFire = 0.f;
				ib::addSaved();
				if (ib::flash) this->flash();
			}
		}

		GJBaseGameLayer::update(dt);
		this->drawHud(pl);
	}

	// a small dot that pops when a saved click fires
	void flash() {
		auto win = CCDirector::sharedDirector()->getWinSize();
		auto dot = CCDrawNode::create();
		dot->drawDot({ 0.f, 0.f }, 6.f, ib::rgba(ib::currentTheme().accent, 1.f));
		dot->setPosition({ 26.f, win.height - 26.f });
		this->addChild(dot, 10000);
		dot->runAction(CCSequence::create(
			CCSpawn::create(
				CCFadeOut::create(0.35f),
				CCEaseOut::create(CCScaleTo::create(0.35f, 2.4f), 2.f),
				nullptr
			),
			CCRemoveSelf::create(),
			nullptr
		));
	}

	// waiting clicks in the bottom left corner, fading as they run out
	void drawHud(PlayLayer* pl) {
		if (!pl) return;

		if (!m_fields->hud) {
			m_fields->hud = CCDrawNode::create();
			m_fields->hud->setID("input-buffer-hud"_spr);
			this->addChild(m_fields->hud, 900);
		}
		auto hud = m_fields->hud;
		hud->clear();

		if (!ib::enabled || !ib::hud) return;

		float window = ib::windowMs();
		for (int p = 0; p < 2; p++) {
			auto& s = ib::st[p];
			for (int i = 0; i < s.count; i++) {
				float frac = 1.f - s.ages[i] / window;
				if (frac < 0.f) frac = 0.f;
				hud->drawDot(
					{ 28.f + i * 15.f, 30.f + p * 14.f },
					3.f + 2.f * frac,
					ib::rgba(ib::currentTheme().accent, 0.2f + 0.8f * frac)
				);
			}
		}
	}
};

// ---------------------------------------------------------------------------
// Clear saved clicks on death/restart, on pause, and when leaving a level.
// ---------------------------------------------------------------------------
#include <Geode/modify/PlayLayer.hpp>
class $modify(IBPlayLayer, PlayLayer) {
	void resetLevel() {
		ib::reset();
		PlayLayer::resetLevel();
	}

	void pauseGame(bool p0) {
		ib::reset();
		PlayLayer::pauseGame(p0);
	}

	void onQuit() {
		ib::reset();
		ib::flushSaved();
		PlayLayer::onQuit();
	}
};
