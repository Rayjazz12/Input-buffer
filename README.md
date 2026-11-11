# Input Buffer

Click a little early and the mod holds the click, then fires it on the **first
frame it can be used** - a new orb in range, or the frame you touch down.
Nothing is ever thrown away, and nothing fires randomly seconds later.

## Using it

- Press the blue **IB** button in the main menu or the pause menu, or press
  **Shift** in either menu.
- Pick a window: **Off**, **50 / 90 / 130 / 180 / 250 ms**.
- Choose what it applies to: **Orbs**, **Ground** (landings), **Practice**
  (practice mode only), **Flash** (dot when a saved click fires) and **HUD**
  (your waiting clicks in the bottom left).
- Pick a theme: Ocean, Sunset, Mint, Grape, Rose or Midnight.

Settings are saved, so the mod comes back exactly how you left it.

## How it behaves

- Up to 3 early clicks can wait at once, so spamming into back-to-back orbs
  works. One click is used per orb or per landing.
- A landing only counts once: standing on the ground will not eat your queue.
- Clicks older than your window are dropped, so a saved click can never fire
  late in the level.
- Nothing fires while you are physically holding the button, so the buffer
  never fights your own input.
- The queue is cleared on death, restart, pause and when you leave the level.

## Notes

This is a helper mod. It will not be accepted for official records, so use
**Practice** mode if you want to stay legit.

## Build

```sh
geode build
```

Requires the [Geode SDK](https://docs.geode-sdk.org/getting-started/).
