#include "text_renderer.h"
#include <cstring>
#include <pspgu.h>

TextRenderer::TextRenderer() : font(nullptr), fontScale(1.0f) {}

TextRenderer::~TextRenderer() { Shutdown(); }

bool TextRenderer::Initialize() {
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

bool TextRenderer::LoadFont(float scale) {
  fontScale = scale;

  if (font) {
    intraFontUnload(font);
    font = nullptr;
  }

  // Load bundled font from local directory (not flash0)
  // This ensures hardware icons are available and avoids PPSSPP flash0 issues
  font = intraFontLoad("ltn0.pgf", INTRAFONT_CACHE_LARGE);

  // Fallback to ltn8.pgf if ltn0 is missing
  if (!font) {
    font = intraFontLoad("ltn8.pgf", INTRAFONT_CACHE_LARGE);
  }

  if (!font) {
    return false;
  }

  intraFontSetEncoding(font, INTRAFONT_STRING_UTF8);
  intraFontSetStyle(font, fontScale, 0xFFFFFFFF, 0, 0.0f, 0);

  return true;
}

int TextRenderer::MeasureTextWidth(const char *text) {
  if (!font)
    return 0;

  intraFontSetStyle(font, fontScale, 0xFFFFFFFF, 0, 0.0f, 0);
  return (int)intraFontMeasureText(font, text);
}

void TextRenderer::RenderText(const char *text, int x, int y, uint32_t color) {
  if (!font)
    return;

  intraFontSetStyle(font, fontScale, color, 0, 0.0f, 0);
  intraFontPrint(font, (float)x, (float)y, text);
}
