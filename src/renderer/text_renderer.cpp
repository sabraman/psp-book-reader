#include "text_renderer.h"
#include <cstring>
#include <pspgu.h>

TextRenderer::TextRenderer() : font(nullptr), fontSize(16) {}

TextRenderer::~TextRenderer() { Shutdown(); }

bool TextRenderer::Initialize() {
  // Initialize libintraFont
  intraFontInit();
  return true;
}

void TextRenderer::Shutdown() {
  if (font) {
    intraFontUnload(font);
    font = nullptr;
  }
  intraFontShutdown();
}

bool TextRenderer::LoadFont(int size) {
  fontSize = size;

  // Load bundled Inter font (supports Cyrillic!)
  // This file is in the same directory as EBOOT.PBP
  font = intraFontLoad("Inter-Regular.ttf", INTRAFONT_CACHE_ASCII);
  if (!font) {
    return false;
  }

  // Set encoding to UTF-8 for Cyrillic support
  intraFontSetEncoding(font, INTRAFONT_STRING_UTF8);

  // Set font style: size, color, shadowColor, angle, options
  intraFontSetStyle(font, (float)fontSize, 0xFFFFFFFF, 0, 0.0f, 0);

  return true;
}

int TextRenderer::MeasureTextWidth(const char *text) {
  if (!font)
    return 0;

  return intraFontMeasureText(font, text);
}

void TextRenderer::RenderText(const char *text, int x, int y, uint32_t color) {
  if (!font)
    return;

  // Set text color: size, color, shadowColor, angle, options
  intraFontSetStyle(font, (float)fontSize, color, 0, 0.0f, 0);

  // Render text
  intraFontPrint(font, (float)x, (float)y, text);
}
