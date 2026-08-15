# NORA migration — where these sources come from (2026-08-16)

This repository is a **first publication**, not a rename of an older one: the touch
HAL has never lived anywhere else. It is nonetheless a **published snapshot** under
the same rule as the rest of the NORA-HAL fleet — `src/` is filled from the tree
that is actually built and run on hardware, and this file records which tree, which
commit, and how the equality was checked.

## The chain

```
dsp-sonora audio board project        the tree that runs on hardware
  main = 374b828
  (the files landed from feat/open-itc-touch = 268bd39;
   the comment rewrites landed as ab970a0, 2653def;
   the cold gate and the learned-threshold floor landed as
   2e31076 / 22c920a / 0e4a93d, merged at 4e089cc;
   the 7 h 52 m soak recorded at 374b828)
        |  vendored, byte-for-byte
        v
sulaolab/dspic33ak-hal-starter        MPLAB X project, NORA-HAL modules
  main = 58029ba
  (the five files below entered at 189d82a;
   their comments were rewritten at fcc8e4c and 1f03f38;
   re-vendored at 763d772, AK512 build PASS;
   soak and two stale doc clauses fixed at 58029ba)
        |  published, byte-for-byte
        v
sulaolab/nora-hal-dspic33ak-touch     this repository
```

Direction matters, and for this module it matters more than for most: only the
audio-board tree has the pads, the disturbers, and the console that drives the
tuning procedure, so it is the only place a change to the detection layer can be
shown to still count 10 taps out of 10. **A fix made here and not upstream would
be a fork**, and it would be a fork of the one copy nobody can test.

The starter carries these files on `main` as of `58029ba`: development happened on
`feat/open-itc-touch`, and that branch was fast-forwarded onto `main` so that the
default branch a reader of this repository lands on is the tree described here.

## Naming and structure decisions recorded by this snapshot

There is no rename mapping table here, because there are no renamed published
files. There are three decisions the layout encodes, and they are worth stating
because a reader comparing this repository against an older description of the
upstream tree will notice all three.

| decision | what it means here |
|---|---|
| **One HAL, one directory** | Upstream used to have two: `src/hal_itc/` (the ITC driver, with a public `nora_itc.h`) and `src/hal_touch/` (detection). `src/hal_itc/` is abolished; all five files live in one directory, and here they are flat in `src/`. |
| **The acquisition layer has no public header** | The old `nora_itc.h` is now `nora_itc_internal.h`. The peripheral is reachable only through the `nora_touch_hw_*()` entry points in `nora_touch.h`. |
| **Backend tag `_dspic33ak`** | `nora_itc_dspic33ak.c` / `_reg.h` are the dsPIC33AK backend of the neutral contract. Not `_dspic33a`: `dsPIC33A` is the *core* family name and one level too coarse. A dsPIC33CK backend would be `_dspic33ck`. |

The reason for the first two is not tidiness. While the vendor library was still
linked beside this HAL, the ITC had two owners, and the second one moved the
baseline under the first: **3,000 counts of offset**, visible only as pads that had
gone insensitive. A public acquisition header is an invitation to reproduce that.
Removing it is the fix that cannot regress.

## Proof of identity with the upstream tree

Git blob hashes, this repository vs `dspic33ak-hal-starter` at `main` = `58029ba`
**and** `dsp-sonora` at `main` = `374b828`. Identical hash = identical bytes; git
normalises EOLs into the blob on all three sides, so the CRLF working trees do not
disturb the comparison.

| file | blob | bytes |
|---|---|---|
| `nora_touch.h` | `d6d8172c7e37` | 27382 |
| `nora_touch.c` | `d603355cebec` | 65229 |
| `nora_itc_internal.h` | `f159d19144f7` | 13722 |
| `nora_itc_dspic33ak.c` | `a00aedd8720a` | 38372 |
| `nora_itc_dspic33ak_reg.h` | `ba996177e537` | 8155 |

**5 of 5 identical**, against both upstreams at once. Upstream paths are
`src/hal_touch/<file>` in the starter and `src/app/hal_touch/<file>` in
`dsp-sonora`; the extra `src/app/` level is that project's own layout and not a
divergence in these files.

`src/README.md` and `README.md` are written for this repository and have no
upstream counterpart, so they are not in the table. Neither is
`examples/touch_keys_example.c` — see below.

## What is deliberately *not* published here

| upstream file | why it stays upstream |
|---|---|
| `touch_console.c` / `.h` | The bring-up and tuning console. It is a consumer of this HAL, not part of it: it depends on the application's command dispatcher (`app_console`). It is published in `dspic33ak-hal-starter` as module `k`, where a command parser exists. |
| `docs_internal/shared/open_touch/*` | Provenance rules, the ITC hardware reference, and the measurement logs. Internal by policy. |

`examples/touch_keys_example.c` was written **for this repository** rather than
vendored, for exactly the reason above: the natural example in the upstream tree is
the console, and the console cannot be lifted out of its application. The example
is therefore the minimal `app_touch_*` integration — init, poll, consume events —
and every number in it (the CVDANx list `{1, 8, 10}`, the 200 MHz clock, the
2,000 ns / 1,000 ns acquisition defaults) is read from the upstream integration
point, not invented.

## Closed item: `docs_internal` references in published comments

An earlier revision of this file recorded eleven comment references that pointed at
internal documents a reader of this repository cannot see, plus one that named
`sonora_clock_boot_init()` — a symbol that exists only in the audio-board project
and could be read here as a dependency. **They are gone**, fixed where the chain
requires: upstream in `dsp-sonora`
(`ab970a0` on `main`), re-vendored to the starter
(`fcc8e4c`), re-copied here, with 5-of-5 identity re-proved above.

The edits were comment-only, and each one states the fact the reference stood in
for rather than pointing somewhere else: the threshold constants cite this board's
own measured figures, the bit-alias atomicity rule is spelled out (the compiler
emits a single-bit atomic write only for a compile-time-constant value, and
otherwise read-modify-writes a word that carries other subsystems' bits), and the
clock line says "the project's clock boot code" instead of naming a function.
The HAL depends only on being *told* the ITC clock frequency (`clock_hz`), by
whatever raised it.

The internal manual's **section numbers** are gone too, in a second pass
(`2653def`, re-vendored at `1f03f38`): about two dozen comments cited "appendix A
§A.7", "chapter 1" or "§A.9", and the published manual
(`dspic33ak-hal-starter/docs/open-touch-tuning.md`) does not carry that numbering.
Each citation stood beside the measurement it referenced, so the evidence stayed
and the pointer went — dates where the trace has one ("a scan-resolution trace
taken on 2026-08-14"), "the bench run" for the counted-tap runs. Numberless
references to the tuning manual remain, and resolve in the starter. The internal
roadmap's "Phase 0" is gone from six comments for the same reason: what it meant
in each place is now stated as a property of the HAL.

**Nothing in the published comments points at a document a reader of this
repository cannot open.**

## Hardware evidence

There is no build and no test in this repository — it is sources only. The evidence
is the upstream project's, and for this module it is unusually direct, because the
thing under test is a human finger and there is no substitute for one.

dsPIC33A Curiosity Platform Development Board, AK512 DIM (dsPIC33AK512MPS512), ITC
clock 200 MHz, the three motherboard pads. Counted-tap run, 10 deliberate taps per
pad:

| measure | result |
|---|---|
| taps counted | 10 / 10 / 10 |
| press peaks | 6,370 – 9,992 against a 700 threshold |
| idle magnitude | 139 – 172 |
| learner | stable at press 700 / release 350 |
| rejected scans | 1 in 338,914 |
| implausible samples | 0 |
| unattended soak from that state | **7 h 52 m, zero press/release events** (2026-08-16; 5,692,179 scans, `idle_ref` 67 / 63 / 119) |

The soak is listed as a separate line because it tests the opposite failure: the
counted-tap run proves a finger is detected, and only an idle run proves nothing else
is. It also only counts as evidence because the same console answered a live query at
the end of it — an empty log on its own would prove the link died.

Three limits of that evidence, stated rather than left to be discovered:

* **One board.** Per-board individuality is handled in the HAL (the per-pad
  learner) but has been *measured* on one board only. A second board is expected to
  differ in idle level and press peak; it is not expected to need code changes.
* **The near-zero-read mechanism is not explained.** The implausible-sample
  rejection path exists because such samples were observed; the count above is
  zero, and the rejection is a guard rather than a diagnosis.

* **AK512 only, as a scope claim.** The `#if defined(ITCLS0CON)` guard exists so a
  shared source tree still compiles for a part with no ITC (`dsPIC33AK128MC106` has
  no `ITCCON1`, and there is no fallback), and on such a part every entry point is
  a stub returning `NORA_ITC_ERR_UNSUPPORTED` with `nora_touch_init()` returning
  false. That is a property of the source, not a tested configuration: neither
  upstream project carries an ITC-less build of these files, so the guard is
  reviewed, not exercised.

## Consumer impact

* **Nothing to migrate.** This is a first publication; there is no earlier
  published namespace, and no compatibility aliases are needed or provided.
* A consumer coming from the upstream trees changes include paths only
  (`hal_touch/nora_touch.h` → `nora_touch.h`, since `src/` is flat here). This is
  the sole difference the starter itself carries, and it is the one documented
  divergence in `dspic33ak-hal-starter`'s `docs/open-touch-sync.md`.
* A consumer that used to include `hal_itc/nora_itc.h` has no replacement include,
  by design. Everything it legitimately needed is in `nora_touch.h` under
  `nora_touch_hw_*()`.
* `nora_touch.c` includes `<stdio.h>` and calls `printf()` on diagnostic paths, so
  the project needs stdio retargeting to link even with `verbose` off.
