#pragma once

#include "types.h"
#include <intraFont.h>

class TextRenderer {
public:
  TextRenderer();
  ~TextRenderer();

  bool Initialize();
  void Shutdown();

  // Load font (uses firmware fonts from flash0:/font/)
  bool LoadFont(float scale = 1.0f);

  // Render text to screen using libintraFont
  void RenderText(const char *text, int x, int y, uint32_t color);

  // Measure text dimensions
  int MeasureTextWidth(const char *text);

  // Advanced: Get raw font pointer
  intraFont *GetFont() { return font; }

private:
  intraFont *font;
  float fontScale;
};
