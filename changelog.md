# v2.0.0
- Buffer rewritten around real frames: a saved click now fires only on a fresh
  orb or on the exact landing frame, and each of those uses one click. Standing
  on the ground can no longer drain or re-fire your queue.
- Saved clicks never fire while you are holding the button, so the mod can't
  fight your own input.
- The panel is a proper Geode popup now: correct background, close button,
  Escape to close and no leftover invisible menu. Reopening it twice no longer
  leaves a broken panel behind.
- New **HUD** toggle: your waiting clicks show in the bottom left, fading as
  their window runs out, so you can see the buffer working.
- Flash dot is drawn directly, so it works even when the sprite sheets change.
- Themes retinted for the new panel: Ocean, Sunset, Mint, Grape, Rose, Midnight.
- Click counter is saved on close/exit instead of on every click (no hitching).
- Buttons fall back to a corner menu if the vanilla menu ids ever change.
- Shift is ignored while another popup is open.
- CI now builds Windows, macOS, Android and iOS.

# v1.2.1
- Shift now uses a safer per-frame key check (fixes a build error risk).

# v1.2.0
- New button on the main menu that opens the panel.
- Press Shift in the main menu or pause menu to open the panel (press it again to close).

# v1.1.0
- Smarter buffer: up to 3 early clicks can wait at once, one click per orb/landing, dead players are ignored, saved clicks never fire twice on the same orb.
- 6 themes (Ocean, Sunset, Mint, Grape, Rose, Midnight) that fade between each other.
- New GUI: ON/OFF chip, counting ms number, meter with level ticks, breathing glass sheen, Flash toggle, clicks-saved counter.
- Flash: a small dot in the corner when a saved click fires.

# v1.0.0
- First release: input buffer with 4 window sizes + Max, orb and ground buffering, optional practice-only mode, glassy GUI in the pause menu.
