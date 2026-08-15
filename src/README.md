# NORA touch HAL

`nora_touch.h` is the public capacitive-touch contract: configure a set of
electrodes, poll, and read press/release events and per-key state. It exposes no
compiler SFR types and no register names.

## Ownership boundary

- Application and board code state the two board facts: the CVDANx analog-input
  numbers of the electrodes, and the frequency of the clock generator feeding the
  Integrated Touch Controller (`clock_hz`, which has no default — init refuses 0).
- The HAL owns acquisition timing, accumulation, baseline tracking, magnitude
  thresholds with hysteresis and debounce, and per-pad threshold learning.
- The dsPIC33AK backend owns register access and the ITC's own limits. Its files
  use explicit `nora_itc_dspic33ak_*` names.
- **One HAL, one directory.** The acquisition layer has no public header: it is
  reachable only through the `nora_touch_hw_*()` entry points in `nora_touch.h`.
  A caller that can configure the ITC directly becomes a second owner of the
  peripheral, and the baseline then moves under the one that is not looking.
- No interrupt is used and none is defined. Scanning is non-blocking and this HAL
  counts scans, not milliseconds, so it needs no timer from the caller either.

## Layers

| file | layer |
|---|---|
| `nora_touch.h` | public contract: detection, state, tuning, and the `nora_touch_hw_*` bring-up window |
| `nora_touch.c` | detection: baseline, magnitude, hysteresis, debounce, learning |
| `nora_itc_internal.h` | acquisition contract — internal to this HAL, not an application include |
| `nora_itc_dspic33ak.c` | acquisition backend: ITC registers, timing, results |
| `nora_itc_dspic33ak_reg.h` | backend-private register/bit definitions and the per-list register table's type |

## Non-goals

Drift compensation over temperature and humidity, wet-finger rejection, frequency
hopping and scrollers are the integrator's. So are long press, double tap and any
application state built on the events — those belong above this HAL, not in it.
Threshold learning deliberately keeps nothing across a power cycle: boot behaviour
that depends on the last session is harder to reason about than a fixed start.
