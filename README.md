# CNC Chip Conveyor Timer

Interval timer for a CNC chip conveyor. It presses the conveyor's **START** and
**STOP** buttons for you, in a loop, so the coolant on the swarf drains back
into the machine instead of leaving with the chips.

![MCU](https://img.shields.io/badge/MCU-STM32G071GBU6-03234B?logo=stmicroelectronics&logoColor=white)
![Toolchain](https://img.shields.io/badge/toolchain-STM32CubeMX%20%2B%20IAR%20EWARM-555)
![Status](https://img.shields.io/badge/status-hardware%20bring--up-orange)
![License](https://img.shields.io/badge/license-MIT-green)

<!-- photo of the box on the machine: docs/photo.jpg -->

## Why

On many machines the chip conveyor is either always on or run by hand. Run
continuously it pulls the swarf out of the coolant too fast: the chips land in
the bin still wet, and a surprising amount of coolant goes with them.

Run the conveyor in short bursts instead, with a pause in between, and the
coolant drains off the belt while it stands still. Chips come out drier, the
coolant stays where it belongs.

This box does the pressing. One illuminated button, five run / pause programs,
one relay for START and one for STOP.

## How it works

```mermaid
stateDiagram-v2
    direction LR
    [*] --> SETTLE : switched on
    SETTLE --> START_PULSE : 100 ms, both relays off
    START_PULSE --> RUN : START held 500 ms
    RUN --> STOP_PULSE : T1 elapsed
    STOP_PULSE --> PAUSE : STOP held 500 ms
    PAUSE --> START_PULSE : T2 elapsed
```

| Mode | T1 — conveyor runs | T2 — conveyor rests |
|:---:|:---:|:---:|
| 1 | 5 s | 5 s |
| 2 | 10 s | 10 s |
| 3 | 15 s | 15 s |
| 4 | 20 s | 30 s |
| 5 | 30 s | 60 s |

The two relays are never closed at the same time. The rule is enforced in the
relay driver itself — energising one relay releases the other first — and every
start of the cycle begins with 100 ms of both relays released.

After **4 hours** of continuous running the timer switches itself off and
sounds a 15 s alarm.

## Operating it

Everything is done with the one button. A command is a series of quick clicks;
the series ends when the button stays released for 0.7 s.

| Clicks | Action |
|:---:|---|
| 1 | Switch on in the stored mode / switch off |
| 2 | Mode 1 |
| 3 | Mode 2 |
| 4 | Mode 3 |
| 5 | Mode 4 |
| 6 | Mode 5 |

Selecting a mode stores it in flash and starts (or restarts) the cycle at once.
The stored mode survives a power cycle; the device always powers up switched
off.

**Sounds**

| Event | Buzzer |
|---|---|
| Power on | 2 s |
| Cycle started or restarted | 2 s |
| Switched off by the operator | 0.5 s |
| 4 h auto-off | 15 s |
| First power-on (blank settings) | SOS, once |
| Settings flash unusable | SOS at every power-on |

**Button lamp**

| State | Lamp |
|---|---|
| Off | dark |
| Running | *N* blinks, 3 s dark, *N* blinks… where *N* is the mode |
| Settings flash unusable, switched off | fast flicker |

**On-board LED (HL1)** mirrors the state for whoever has the lid open: lit
during start-up, a flash every 3 s when off, and a distinct pattern for each
phase of the cycle while running.

### When the settings flash fails

The mode is kept in a flash page that is rewritten only when the mode changes.
A power loss in the middle of a write leaves a damaged record; on the next
power-on the page is repaired automatically and the last good mode is kept. If
even that fails the timer keeps working with the mode held in RAM, and says so:
SOS instead of the power-on beep, and the button lamp flickers whenever the
device is off. It can still be switched on and off and used normally until the
board is repaired.

## Hardware

Board `PCB1_main_rev1`, 24 V supply.

| MCU pin | Signal | Circuit |
|---|---|---|
| PB0 | `BUTTON_IN` | 10 kΩ pull-up, button to GND through 1 kΩ — active low |
| PA6 | `BUTTON_LED_ON` | 1 kΩ → CPC1014N solid-state relay → 24 V → button lamp |
| PA7 | `START_RELAY` | 1 kΩ → CPC1014N → relay K2 — the machine's START button |
| PA5 | `STOP_RELAY` | 1 kΩ → CPC1014N → relay K1 — the machine's STOP button |
| PB3 | `BUZZER` | 1 kΩ → CPC1014N → MLT-9650 active buzzer |
| PB7 | `DEBUG_LED` | 1 kΩ → HL1 |

Programming connector X2 carries SWDIO, SWCLK, GND and 3V3 only — there is
**no NRST**, so connect-under-reset is not available.

### Rev 1 errata

- The relay drivers are wired active **low**. Handled in firmware
  (`RELAY_START_ACTIVE_LOW` / `RELAY_STOP_ACTIVE_LOW` in `relay.h`); the
  relays are put to rest microseconds after the pins become outputs, inside the
  1.5 ms the solid-state relays need to react.
- The 1 kΩ resistors feeding the CPC1014N inputs give ≈ 2.1 mA against a
  guaranteed turn-on of 2 mA. Works, but marginal; 330–470 Ω next revision.
- HL1 gets ≈ 1.2 mA and is dim.

Design sources (Altium) are in [`hardware/`](hardware/). Ready to
use: [schematic and assembly PDF](docs/CNC_rev1_2026-08-26.pdf) and
[gerbers, drill and pick-and-place](docs/CNC_rev1_gerber_2026-08-26.zip) in
`docs/`.

## Firmware

STM32CubeMX generates the HAL project (`firmware/CNC_TIMER.ioc`); IAR
Embedded Workbench builds it (`firmware/EWARM/Project.eww`). Everything the device does lives in
the `Core/` sources; the CubeMX-generated files are only touched inside their
`USER CODE` sections.

| Module | Job |
|---|---|
| `button.c` | Debounce, click-series detector, deaf/listening gate |
| `relay.c` | The two relays, mutual exclusion, polarity |
| `cycle.c` | START / RUN / STOP / PAUSE state machine |
| `settings.c` | Mode in the last flash page, append-only log, repair, fault flag |
| `indicator.c` | Button lamp: blink bursts, flicker |
| `buzzer.c` | Beeps and the Morse SOS |
| `debug_led.c` | HL1 patterns |
| `app.c` | Power-on signature, OFF / RUN, click dispatch, 4 h clock, sounds |
| `main.c` | Wiring only |

The main loop never blocks. `Button_Tick()` runs from SysTick every 1 ms; every
other module is polled from the loop and works from `HAL_GetTick()`.

The settings page is the last 2 KB of flash (`0x0801F800`); the linker script
ends ROM one page early so no code can land there. Every module exposes a
`volatile` `g_*` structure for the IAR Live Watch window — counters, live
values, measured timings — so the board can be read without a printf.

## Repository layout

```
firmware/   STM32CubeMX project (CNC_TIMER.ioc), HAL drivers, IAR workspace in EWARM/
hardware/   Altium Designer sources of PCB1_main_rev1
docs/       schematic and assembly PDF, gerbers
```

## Status

Hardware bring-up. The button, lamp, buzzer, HL1 and start-up sequence have
been verified on the board; the relay cycle, flash settings and the fault
handling are written but not yet run on hardware.

## License

MIT — see [`LICENSE`](LICENSE).
