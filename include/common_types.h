#pragma once

#include <cstdint>

enum class Theme { NIGHT = 0, SEPIA = 1, LIGHT = 2 };
enum class MarginPreset { NARROW = 0, NORMAL = 1, WIDE = 2 };
enum class SpacingPreset { TIGHT = 0, NORMAL = 1, LOOSE = 2 };

struct ThemeColors {
  uint32_t background;
  uint32_t text;
  uint32_t heading;
  uint32_t dimmed;
  uint32_t selection;

  ThemeColors(uint32_t bg, uint32_t txt, uint32_t h, uint32_t d, uint32_t s)
      : background(bg), text(txt), heading(h), dimmed(d), selection(s) {}
  ThemeColors() : background(0), text(0), heading(0), dimmed(0), selection(0) {}
};
