# nora-hal-dspic33ak-touch

**NORA-HAL** — *Native On-chip Resource Assistant*

Small, readable capacitive-touch HAL for Microchip dsPIC33AK devices — part of
**NORA-HAL**, a HAL family whose public API is namespaced `nora_*` / `NORA_*`.

> Want to run it on hardware first?
> Start with [dspic33ak-hal-starter](https://github.com/sulaolab/dspic33ak-hal-starter),
> which vendors validated snapshots of the NORA-HAL repositories and provides a
> ready-to-build MPLAB X project for the dsPIC33AK Curiosity board — including the
> module `k` bring-up console for these pads.

> **This repository is a published snapshot, not the development tree.** Every
> file under `src/` is byte-identical to its counterpart in
> `dspic33ak-hal-starter`, which is in turn byte-identical to the audio-board
> project that runs these sources on hardware. Fixes flow *into* here from that
> validated tree — see [docs/nora_migration.md](docs/nora_migration.md).

The three touch pads of the Curiosity Platform motherboard work with this HAL and
nothing else: it is an open implementation written from the family reference manual
and bench measurement, **not** Microchip's QTouch Modular Library. See
[Provenance](#provenance) for what that means precisely.

## What it drives

The **Integrated Touch Controller** (ITC) — the data sheet's own name for the
block, and its SFR prefix. It is a micro-sequencer that drives the electrode pins
and the internal CVD capacitor array, converts on ADC 5, and does the
pseudo-differential arithmetic itself, so this HAL is a configurator and a result
reader rather than a sampling driver: there is no per-sample entry point, because
the CPU has no per-sample work.

Above that sits the detection layer, and one decision there is worth stating up
front because it looks like a fault:

**A touched pad's signal alternates sign from scan to scan.** Detection therefore
works on the mean of `|delta|` — a *magnitude* — not on a signed level. Thresholds
are in magnitude units; a number carried over from a signed scheme is roughly 3×
too large and will look like a dead pad.

## Status

Hardware-validated.

Validated on the dsPIC33A Curiosity Platform Development Board with an AK512 DIM
(dsPIC33AK512MPS512), ITC clock 200 MHz, three motherboard pads. Latest counted-tap
run, 10 deliberate taps per pad:

| measure | result |
|---|---|
| taps counted | 10 / 10 / 10 |
| press peaks | 6,370 – 9,992 against a 700 threshold (9–14× margin) |
| idle magnitude | 139 – 172 |
| learner | stable at press 700 / release 350 |
| rejected scans | 1 in 338,914 (0.0003 %) |
| implausible samples | 0 |

Quiet-board behaviour was measured separately, because it is the failure mode that
does not announce itself: from the learned state above, **7 h 52 m unattended with
zero press or release events** (2026-08-16, 5,692,179 scans, 0 rejected, 0
implausible, `idle_ref` 67 / 63 / 119). A pad that presses itself once an hour passes
every counted-tap run there is.

## Repository layout

```text
src/
  nora_touch.h                 Public touch HAL API
  nora_touch.c                 Detection: baseline, magnitude, hysteresis, debounce, learning
  nora_itc_internal.h          Acquisition contract — internal to this HAL
  nora_itc_dspic33ak.c         Acquisition backend: ITC registers, timing, results
  nora_itc_dspic33ak_reg.h     Backend-private register / bit definitions
  README.md                    Source-tree guide and ownership boundary
examples/
  touch_keys_example.c         Minimal three-key integration
```

There is deliberately **no public acquisition header**. Everything a bring-up
console legitimately needs from the layer below — hardware state, one scan, raw
counts, a register dump, test injection — is reachable through the
`nora_touch_hw_*()` entry points in `nora_touch.h`. A caller that can configure the
ITC directly becomes a second owner of the peripheral; when that happened here
against the vendor library, it cost 3,000 counts of baseline offset before anyone
noticed.

## What the board must provide

Two facts, both stated by the caller because both are properties of the board:

* **The electrodes**, as CVDANx analog-input numbers, in pad order.
* **`clock_hz`**, the frequency of the clock generator feeding the ITC (CLKGEN6 on
  this family). It has **no default** — `nora_touch_init()` refuses 0. Acquisition
  times are held in nanoseconds and converted to timer counts against this number,
  so a wrong frequency reports no error at all: the pads simply read weak.

The HAL does not configure clock generators, PPS routing, GPIO direction or
analog-disable settings, and it defines no interrupt vector. Electrode pins want
no GPIO or PPS setup — the ITC drives and senses them itself — but a pin left
digital or claimed by another peripheral reads as a dead key.

## Public API overview

```c
#include "nora_touch.h"
```

```text
Setup:
  nora_touch_default_config()
  nora_touch_init()

Run:
  nora_touch_process()          non-blocking; call from the main loop
  nora_touch_get_event()        PRESSED / RELEASED, consumed by reading
  nora_touch_is_pressed()       held state; consumes nothing

Health and per-key view:
  nora_touch_get_status()
  nora_touch_get_key_state()
  nora_touch_reset_peaks()

Tuning:
  nora_touch_set_thresholds()        nora_touch_get_thresholds()
  nora_touch_set_key_thresholds()    per pad, when one pad really differs
  nora_touch_set_acquisition()       nora_touch_get_acquisition()
  nora_touch_calibrate()             nora_touch_get_calibration()
  nora_touch_trace_arm() / _ready() / _count() / _get()

Bring-up window onto the acquisition layer:
  nora_touch_hw_get_info()      nora_touch_hw_configure()
  nora_touch_hw_scan_once()     nora_touch_hw_read_raw()
  nora_touch_hw_debug_reg()     nora_touch_hw_test_inject()
```

Per-pad threshold learning is on by default (`learn_presses`, default 3): each pad
recomputes its own thresholds from its own measured presses, because idle noise was
measured **not** to predict a press. Set it to 0 to keep the shipped pair fixed.
Nothing is stored across a power cycle either way — boot behaviour that depends on
the last session is harder to reason about than a fixed start.

Until a pad reports its **first** event it runs on a second, stricter pair
(`cold_press_threshold`, default 900, and `cold_debounce_scans`), because idle noise
cannot tell you a finger is near and the learner has nothing to consume yet. One
event is trusted to say "a human is here" and nothing more — it does not set a
threshold. Both cold values may only make a pad stricter; a pinned pair switches the
gate off for that pad; `nora_touch_calibrate()` re-arms it. `cold_gate` in
`nora_touch_calibration_t` reports the state, because in a log a cold pad and an
insensitive pad look the same.

Learning may only make a pad **more** sensitive, and it stops at a floor of
`max(700, idle_ref × 6)` — an absolute minimum plus a multiple of the pad's own quiet
magnitude, applied after the ceiling so that it outranks it. A pad therefore cannot
learn a threshold that its own idle noise would trip. If your product needs a lower
pair than the floor allows, set it explicitly with `nora_touch_set_key_thresholds()`,
which outranks learning.

## Build notes

Add these C files to your project:

```text
src/nora_touch.c
src/nora_itc_dspic33ak.c
```

Add `src/` to your include path. Application code includes only
`nora_touch.h`. `nora_itc_internal.h` and `nora_itc_dspic33ak_reg.h` are internal
to the HAL; they are part of the source distribution but not for application use.

Two dependencies worth knowing before you add the files:

* The backend includes `<xc.h>` and reads the ITC SFRs through the DFP header, so
  it wants the device pack for the part you are building.
* `nora_touch.c` includes `<stdio.h>` and uses `printf()` on the diagnostic paths
  only (`cfg.verbose`, and the learner's report). Leaving `verbose` false costs no
  output, but the calls are compiled in, so the project needs `printf()` to link.

### Parts without an ITC

The backend is wrapped in `#if defined(ITCLS0CON)` — the DFP header's own macro,
so the question "does this part have an ITC?" is answered by the device pack
rather than by a list of part numbers kept here. On a part that has none, every
entry point compiles to a stub returning `NORA_ITC_ERR_UNSUPPORTED`,
`nora_touch_init()` returns false, and nothing needs excluding from the project
file.

The scope claim is narrower than the guard, and deliberately so: the HAL is
validated on **AK512 only**. `dsPIC33AK128MC106`, for instance, has no `ITCCON1`
and there is no fallback — the guard is there so that a shared source tree still
compiles for such a part, not so that touch works on it.

## Design policy

* Public API does not expose XC-DSC / DFP bitfield types or register names.
* One HAL, one directory: the acquisition layer has no public header of its own.
* Board-specific clock-generator setup, PPS routing and GPIO setup stay outside.
* No interrupt vector is defined here, and none is needed.
* No dynamic memory allocation.
* No persistent state: nothing is written to flash, and nothing survives a reset.
* Detection works on magnitude, and thresholds are in magnitude units.

## Provenance

Written for the `dspic33ak-audio-dsp-sonora` firmware from DS70005591 ch.18 (ITC),
the DFP SFR header, and measurements on this same board. **No vendor touch-library
source, header or binary was consulted, and no vendor detection algorithm was
inspected.** The file headers state this per file. Behavioural observation of a
vendor demo — pads respond, at roughly this sensitivity — was used; nothing was
disassembled.

This is not a claim that the implementation is better than the vendor's. It is a
claim about its licence: it is this project's own code, MIT-0, and therefore
publishable with the rest of the starter.

## Scope and limitations

Provides self-capacitance key detection: up to `NORA_TOUCH_KEY_MAX` (8) keys, one
scan list, press/release with hysteresis and debounce, per-pad learning, and a
bring-up/tuning surface.

Does not provide, and leaves to the integrator:

* drift compensation over temperature and humidity
* wet-finger rejection, frequency hopping, scrollers, sliders
* mutual-capacitance sensing (CVD pseudo-differential acquisition only — the sole
  documented use of this block)
* long press, double tap, or any application state built on the events
* per-board calibration storage
* the console that drives the tuning procedure (it lives in the consumer, where
  the command parser is — see `dspic33ak-hal-starter`, module `k`)

## License

MIT No Attribution License (MIT-0). See [LICENSE](LICENSE).

Attribution is appreciated but not required.
