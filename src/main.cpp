#include <Geode/Geode.hpp>
#include "Buffer.hpp"
#include "BufferPopup.hpp"

using namespace geode::prelude;

// Load saved settings when the mod loads.
$execute {
	ib::load();
}

// ---------------------------------------------------------------
// Shift opens (and closes) the panel from the main menu or pause menu.
// We check the Shift key every frame while one of those menus is up,
// so it never interferes with typing on other screens or the editor.
// ---------------------------------------------------------------
namespace {
	bool g_shiftWasDown = false;

	void pollShiftKey() {
		bool down = CCKeyboardDispatcher::get()->getShiftKeyPressed();
		if (down && !g_shiftWasDown) BufferPopup::toggle();
		g_shiftWasDown = down;
	}
}

// ---------------------------------------------------------------
// Main menu button that opens the Input Buffer GUI
// ---------------------------------------------------------------
#include <Geode/modify/MenuLayer.hpp>
class $modify(IBMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;

		auto spr = CircleButtonSprite::create(
			CCLabelBMFont::create("IB", "bigFont.fnt"),
			CircleBaseColor::Blue,
			CircleBaseSize::Medium
		);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(IBMenuLayer::onBuffer));
		btn->setID("input-buffer-menu-button"_spr);

		if (auto menu = this->getChildByID("bottom-menu")) {
			menu->addChild(btn);
			menu->updateLayout();
		}

		this->schedule(schedule_selector(IBMenuLayer::pollShift));
		return true;
	}

	void pollShift(float) {
		pollShiftKey();
	}

	void onBuffer(CCObject*) {
		BufferPopup::toggle();
	}
};

// ---------------------------------------------------------------
// Pause menu button that opens the Input Buffer GUI
// ---------------------------------------------------------------
#include <Geode/modify/PauseLayer.hpp>
class $modify(IBPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto spr = CircleButtonSprite::create(
			CCLabelBMFont::create("IB", "bigFont.fnt"),
			CircleBaseColor::Blue,
			CircleBaseSize::Medium
		);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(IBPauseLayer::onBuffer));
		btn->setID("input-buffer-button"_spr);

		if (auto menu = this->getChildByID("right-button-menu")) {
			menu->addChild(btn);
			menu->updateLayout();
		} else {
			// fallback if the node ID isn't found
			auto win = CCDirector::sharedDirector()->getWinSize();
			auto menu2 = CCMenu::create();
			menu2->setPosition({ win.width - 35.f, 60.f });
			btn->setPosition({ 0.f, 0.f });
			menu2->addChild(btn);
			this->addChild(menu2, 10);
		}

		this->schedule(schedule_selector(IBPauseLayer::pollShift));
	}

	void pollShift(float) {
		pollShiftKey();
	}

	void onBuffer(CCObject*) {
		BufferPopup::toggle();
	}
};

// ---------------------------------------------------------------
// The buffer itself
//
//  1. You click. If the game can already use it (touching an orb, or
//     on the ground) it just works. If not, we SAVE the click.
//  2. Every frame we look for the first valid moment: touching an orb
//     or landing. When it comes we send a fresh click right then.
//  3. Clicks older than the window are thrown away. Up to 3 can wait,
//     so spamming into back-to-back orbs works too.
//  4. One click is used per orb/landing: after a click is used it
//     waits for you to leave that orb/ground (or 80 ms) before the
//     next saved click can fire, so one orb never eats two clicks.
// ---------------------------------------------------------------
namespace {
	bool alive(PlayerObject* p) {
		return p && !p->m_isDead;
	}

	bool touchingOrb(PlayerObject* p) {
		return !p->m_touchedRings.empty();
	}

	// landing only counts for gamemodes that click off the ground
	bool onGroundForClick(PlayerObject* p) {
		return p->m_isOnGround && !p->m_isShip && !p->m_isBird && !p->m_isDart && !p->m_isSwing;
	}

	// can a click do something right now?
	bool canAct(PlayerObject* p) {
		if (!alive(p)) return false;
		return (ib::orbs && touchingOrb(p)) || (ib::ground && onGroundForClick(p));
	}

	bool isActive(PlayLayer* pl) {
		if (!ib::enabled || !pl) return false;
		if (ib::practiceOnly && !pl->m_isPracticeMode) return false;
		return true;
	}

	// little dot in the corner when a saved click fires
	void flashDot(PlayLayer* pl) {
		if (!ib::flash) return;
		auto dot = CCSprite::createWithSpriteFrameName("smallDot.png");
		if (!dot) return;
		auto win = CCDirector::sharedDirector()->getWinSize();
		dot->setColor(ib::currentTheme().accent);
		dot->setPosition({ 22.f, win.height - 22.f });
		dot->setScale(1.2f);
		dot->runAction(CCSequence::create(
			CCSpawn::create(
				CCEaseOut::create(CCScaleTo::create(0.3f, 4.f), 2.f),
				CCFadeOut::create(0.3f),
				nullptr
			),
			CCRemoveSelf::create(),
			nullptr
		));
		pl->addChild(dot, 10000);
	}
}

#include <Geode/modify/GJBaseGameLayer.hpp>
class $modify(IBGameLayer, GJBaseGameLayer) {
	// send a click straight to the game without triggering our own hook
	void sendClick(bool down, bool isPlayer1) {
		ib::injecting = true;
		GJBaseGameLayer::handleButton(down, 1, isPlayer1);
		ib::injecting = false;
	}

	// Watch the real button presses.
	void handleButton(bool down, int button, bool isPlayer1) {
		GJBaseGameLayer::handleButton(down, button, isPlayer1);

		if (ib::injecting || button != 1) return;
		auto pl = typeinfo_cast<PlayLayer*>(this); // not the editor
		if (!isActive(pl)) return;

		auto& s = ib::st[isPlayer1 ? 0 : 1];
		if (down) {
			s.held++;
			auto p = isPlayer1 ? m_player1 : m_player2;
			if (canAct(p)) {
				s.markUsed();   // the game used this click right away
			} else if (alive(p)) {
				s.push();       // too early: save it
			}
		} else if (s.held > 0) {
			s.held--;
		}
	}

	void update(float dt) {
		auto pl = typeinfo_cast<PlayLayer*>(this);
		if (isActive(pl)) {
			float ms = dt * 1000.f;

			for (int i = 0; i < 2; i++) {
				auto& s = ib::st[i];
				bool p1 = (i == 0);
				auto p = p1 ? m_player1 : m_player2;

				// finish a tap: we pressed last frame, but you had already let go
				if (s.releaseNext) {
					s.releaseNext = false;
					this->sendClick(false, p1);
				}

				s.sinceFire += ms;
				s.age(ms, ib::windowMs());
				if (s.count == 0) continue;

				if (!canAct(p)) {
					s.armed = true; // left the orb / left the ground
					continue;
				}

				// same orb/landing already used a click: wait for the next one
				if (!s.armed && s.sinceFire < ib::REARM_MS) continue;

				// first valid frame: fire a fresh click
				if (s.held > 0) this->sendClick(false, p1); // still holding -> release first
				this->sendClick(true, p1);
				if (s.held == 0) s.releaseNext = true;       // you already let go -> tap

				s.popFront();
				s.markUsed();
				ib::addSaved();
				flashDot(pl);
			}
		}
		GJBaseGameLayer::update(dt);
	}
};

// Clear saved clicks on death/restart and when pausing.
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
};
