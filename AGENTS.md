# Agent verification contract

These rules apply to every change in this repository, by a person or an agent.

## Definition of done

A change is complete only after all applicable checks below pass in the same working tree that
is handed back. Never claim that something was verified because it compiled or looked right in
source.

1. Configure and compile from the repository root:
   `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build`
   (offline: add `-DFETCHCONTENT_SOURCE_DIR_CLAP=...`).
2. Run the complete suite: `ctest --test-dir build --output-on-failure`.
3. For any editor change, additionally run `build/honkr-ui-preview <dir>` and look at the
   rendered pages with an image viewer. Checking only that the files exist is forbidden.
4. Repeat the complete suite after the final edit. A run made before the final edit does not count.
5. Report exactly which checks ran and disclose anything that remains untested.
6. On this machine, run builds and suites inside a capped systemd scope (see the global notes):
   `systemd-run --user --scope -p MemoryMax=8G -p CPUQuota=300% nice -n 15 <cmd>` (a thermal
   guard stops processes above about 300% CPU).

## Test design rules

- User-visible behaviour has a scenario in `features/` in Given/When/Then form and an executable
  test mapped to it; the feature file names the test binary and scenario function.
- Prove behaviour from audio, in the unit the parameter is stated in (Hz, cents, dB, ms, N). A
  parameter's value having changed is not evidence that the sound changed.
- Prove physics from the model's own numbers where a number is what the claim is about (a
  string's tension, a bar's mode ratio), and from audio where the claim is about what is heard.
- CLAP behaviour is proved by loading the built `.clap` through the public ABI
  (`tests/clap_harness.hpp`): discovery, instantiation, activation, processing, sample-offset
  events, parameters and state streams. The engine suites exist for speed, not as a substitute.
- Every parameter has a proof that it does what it advertises (`advertised_tests`); the list is
  closed by construction, so a new parameter cannot ship without one.
- Every preset is rendered and must be audible, finite and within the output ceiling.
- A regression fix starts with a failing test that demonstrates the regression.
- Tests are deterministic and never depend on installed plug-ins, audio hardware, a network
  connection, a display, or wall-clock timing. The UI suite uses a fake host and a recording
  canvas; the X11 preview is a review tool, not a test.

## Architectural invariants

- The engine (`src/engine.hpp`, `src/dsp/`) knows nothing about CLAP; `src/honkr.cpp` is the
  only file that includes the CLAP headers besides the tests.
- Real-time code (`Engine::render`, `note_on`/`note_off` and everything they call) must not
  allocate, lock, do I/O, log, or touch the editor. The pitch calibration
  (`dsp::winds::Calibration`) runs in `Engine::prepare`, at activation, never on the audio thread;
  it is shared per sample rate and documented in `docs/PHYSICS.md`. When a model's acoustics
  change, the calibration follows automatically, but the level trims (`levels::` in
  `src/engine.hpp`, from `honkr-preset-audit --levels`) and the presets' Master (to the bar in
  `docs/QUALITY.md`) must be refitted.
- Parameter IDs are append-only: a new parameter goes just before `kParamCount`, never in the
  middle, because IDs are automation handles and index the saved state.
- Parameter modulation (`CLAP_EVENT_PARAM_MOD`) offsets a value; automation sets it. Neither
  erases the other.
- The editor is written once against `honkr::ui::Canvas` and `EditorHost`; platform code lives
  only in `src/ui/canvas_*.hpp` and `src/ui/backends.hpp`. Every edit from the editor sits inside
  a gesture for its parameter.
- Third-party code is pinned by tag or commit and recorded in `THIRD_PARTY_NOTICES.md`.

## Evidence

`honkr-ui-preview` writes PPM renders of every page. When handing editor work back, name the
images that were inspected and say what is visibly demonstrated.
