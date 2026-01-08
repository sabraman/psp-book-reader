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
  bool LoadFont(int fontSize = 16);

  // Render text to screen using libintraFont
  void RenderText(const char *text, int x, int y, uint32_t color);

  // Measure text dimensions
  int MeasureTextWidth(const char *text);

private:
  intraFont *font;
  int fontSize;
};
