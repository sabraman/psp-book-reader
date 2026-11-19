# Host-test stubs

The extractor's only PSP-side dependencies are the `TextStyle` enum (via
`text_renderer.h`, which needs SDL2) and `DebugLogger`. These stubs let
`tests/host_test.cpp` and `src/parser/html_text_extractor.cpp` compile
and run on any host with no PSP toolchain:

- `text_renderer.h` — the real header's `TextStyle` enum, exactly mirrored.
- `debug_logger.h` — same public API, implemented inside the test itself
  so tests can count and capture log lines and stay quiet during fuzz.
- `html_text_extractor.h` — a byte-exact copy of
  `include/html_text_extractor.h`. It must stay identical because the
  real header's `#include "text_renderer.h"` would resolve to the
  SDL-dependent original; this copy resolves it to the stub above.
  The `make host-test` recipe enforces the sync with `cmp`, so any
  change to the real header fails the test build until the copy is
  refreshed: `cp include/html_text_extractor.h tests/stubs/`.
