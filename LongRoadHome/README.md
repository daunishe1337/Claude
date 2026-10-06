# The Long Road Home

Horror game clone-of-mechanics (inspired by *The Deadseat*), C++17, **no third-party libraries**:
only the standard library and WinAPI (window, GDI `StretchDIBits`, `waveOut`).

Night drive. You are a kid in the back seat; your parents argue and don't notice something
crawling on the roof. Your handheld console mini-game collects **fuel** and **film** that matter
in the real world. The mini-game never pauses while you look at the real world.

## Build (Visual Studio 2022, one project)
1. Open `LongRoadHome.sln`.
2. Pick `Release | x64` (or Debug) and press **F5**.
   Libraries (`winmm`, `gdi32`, `user32`) are linked via `#pragma comment`, no setup needed.
   (VS 2019: accept "Retarget Solution" or set Platform Toolset to v142.)

## Controls
| Key | Action |
|---|---|
| `Tab` / `Space` | switch real world <-> console |
| `A` / `D` or arrows | steer in the console game |
| `F` / `Enter` | camera flash (real world only) |
| `R` | restart (any time after the menu) |
| `M` | mute, `F11` fullscreen, `Esc` quit |

## Rules
* Monster: crawls on the roof (steps + scraping, pan = direction) -> appears at a window/sunroof
  -> breaches (glass cracks, arm reaches in) -> or retreats if you flash it. It gets faster over time.
* Camera flash only repels the monster **while it is at a window/hatch**; otherwise the shot is wasted.
  Film comes from the console game, recharge is 2.6 s.
* Fuel drains constantly and is refilled **only** by canisters in the console game. Empty = the engine dies (loss).
* Crashing in the console game costs fuel and slows the car (the trip takes longer).
* Lose: monster breaches (jump-scare, loss only) or no fuel. Win: reach home (progress bar at top).
* The red LED on the console body blinks faster as danger rises (peripheral warning).

## Code layout
| File | Role |
|---|---|
| `Common.h` | constants, math, RNG, `SharedState` (fuel, progress, film...) |
| `Renderer.*` | 320x180 software renderer (`uint32_t` buffer), primitives, 5x7 font, post effects |
| `Input.h` | keyboard state with edge detection |
| `Audio.*` | procedural synth (engine, road, scrape, heartbeat, murmuring voices, SFX) over `waveOut` |
| `Monster.*` | monster state machine (pure logic, emits events) |
| `RealWorldScene.*` | back-seat view, windows, parents, monster drawing, camera, HUD |
| `ConsoleMiniGame.*` | 4-colour top-down racer on the handheld |
| `Game.*` | state machine: Menu / Playing / Stalling / Caught / End / Win |
| `main.cpp` | WinAPI window, fixed 60 Hz step loop, `StretchDIBits` presentation |

All art is drawn in code; all sound is synthesized. No external files.
