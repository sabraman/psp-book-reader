#include "text_renderer.h"
#include "debug_logger.h"
#include <cstdio>
#include <cstring>

TextRenderer::TextRenderer()
    : renderer(nullptr), fontScale(1.0f), currentMode(FontMode::SMART) {}

TextRenderer::~TextRenderer() { Shutdown(); }

bool TextRenderer::Initialize(SDL_Renderer *sdlRenderer) {
  renderer = sdlRenderer;
  if (TTF_Init() == -1) {
    DebugLogger::Log("TTF_Init failed: %s", TTF_GetError());
    return false;
  }
  return true;
}

// Helper to check for CJK/Wide characters
static bool HasWideChars(const char *text) {
  if (!text)
    return false;
  // Simple check: if any byte is > 127, it's non-ASCII.
  // Generally, CJK are 3-byte UTF-8 sequences starting with 0xE...
  // Cyrillic is 2-byte starting with 0xD...
  // Inter covers standard Latin + Cyrillic. Droid covers CJK.
  // We want to fallback MAINLY for CJK.
  // Unicode blocks:
  // CJK Unified Ideographs: 4E00-9FFF (E4 B8 80 - E9 BF BF)
  // Kana/Hangul etc also high up.
  // Quick heuristic: If we find a 3-byte sequence (0xE0-0xEF), assume CJK
  // and use fallback.
  size_t len = strlen(text);
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)text[i];
    if (c >= 0xE0 && c <= 0xEF) {
      return true;
    }
  }
  return false;
}

void TextRenderer::Shutdown() {
  CleanupCache();
  for (auto &pair : fonts) {
    if (pair.second)
      TTF_CloseFont(pair.second);
  }
  for (auto &pair : fallbackFonts) {
    if (pair.second)
      TTF_CloseFont(pair.second);
  }
  fonts.clear();
  fallbackFonts.clear();
  TTF_Quit();
}

void TextRenderer::CleanupCache() {
  for (auto &pair : cache) {
    if (pair.second.texture) {
      SDL_DestroyTexture(pair.second.texture);
    }
  }
  cache.clear();
}

void TextRenderer::ClearCache() { CleanupCache(); }

void TextRenderer::ClearMetricsCache() { metricsCache.clear(); }

void TextRenderer::SetFontMode(FontMode mode) {
  if (currentMode != mode) {
    currentMode = mode;
    // Clearing cache is safer when switching modes to avoid mixing assets
    ClearCache();
    ClearMetricsCache();
  }
}

bool TextRenderer::LoadFont(float scale) {
  Shutdown(); // Clean full state
  if (TTF_Init() == -1)
    return false;

  fontScale = scale;
  const char *primaryPath = "fonts/Inter-Regular.ttf";
  const char *fallbackPath = "fonts/DroidSansFallback.ttf";

  auto loadOne = [&](TextStyle style, int baseSize) {
    int size = (int)(baseSize * fontScale);
    if (size < 8)
      size = 8;

    // Load Primary
    fonts[style] = TTF_OpenFont(primaryPath, size);
    if (!fonts[style]) {
      DebugLogger::Log("Failed loading primary font %d: %s", (int)style,
                       TTF_GetError());
    }

    // Load Fallback
    fallbackFonts[style] = TTF_OpenFont(fallbackPath, size);
    if (!fallbackFonts[style]) {
      // If fallback fails, just log it, don't crash, we just won't have CJK
      DebugLogger::Log("Failed loading fallback font: %s", TTF_GetError());
    }
  };

  loadOne(TextStyle::NORMAL, 18);
  loadOne(TextStyle::H1, 26);
  loadOne(TextStyle::H2, 22);
  loadOne(TextStyle::H3, 19);
  loadOne(TextStyle::TITLE, 34);
  loadOne(TextStyle::SMALL, 14);

  return !fonts.empty();
}

std::string TextRenderer::GetCacheKey(const char *text, TextStyle style) {
  char buf[512];
  int len = snprintf(buf, sizeof(buf), "%d_%d_%s", (int)style, (int)currentMode,
                     text);
  if (len >= (int)sizeof(buf)) {
    return std::to_string((int)style) + "_" + std::to_string((int)currentMode) +
           "_" + text;
  }
  return std::string(buf);
}

void TextRenderer::RenderText(const char *text, int x, int y, uint32_t color,
                              TextStyle style, float angle) {
  if (!renderer || !text || strlen(text) == 0)
    return;

  // Font Selection Logic
  TTF_Font *font = nullptr;

  if (currentMode == FontMode::INTER_ONLY) {
    font = fonts[style];
  } else if (currentMode == FontMode::FALLBACK_ONLY) {
    // If fallback fails, try primary
    font = fallbackFonts[style] ? fallbackFonts[style] : fonts[style];
  } else {
    // Smart Mode
    font = fonts[style];
    if (HasWideChars(text) && fallbackFonts[style]) {
      font = fallbackFonts[style];
    }
  }

  if (!font)
    return;

  std::string key = GetCacheKey(text, style);
  CachedTexture *cached = nullptr;

  auto it = cache.find(key);
  if (it != cache.end()) {
    cached = &it->second;
  } else {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, white);
    if (!surface)
      return;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
      SDL_FreeSurface(surface);
      return;
    }

    CachedTexture newEntry = {texture, surface->w, surface->h};
    cache[key] = newEntry;
    cached = &cache[key];
    SDL_FreeSurface(surface);
  }

  uint8_t r = (color >> 0) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = (color >> 16) & 0xFF;
  uint8_t a = (color >> 24) & 0xFF;

  SDL_SetTextureColorMod(cached->texture, r, g, b);
  SDL_SetTextureAlphaMod(cached->texture, a);

  SDL_Rect dstRect = {x, y, cached->w, cached->h};

  if (angle != 0.0f) {
    SDL_Point center = {0, 0};
    SDL_RenderCopyEx(renderer, cached->texture, NULL, &dstRect, (double)angle,
                     &center, SDL_FLIP_NONE);
  } else {
    SDL_RenderCopy(renderer, cached->texture, NULL, &dstRect);
  }
}

void TextRenderer::RenderTextCentered(const char *text, int y, uint32_t color,
                                      TextStyle style, float angle) {
  int width = MeasureTextWidth(text, style);

  if (angle != 0.0f) {
    int tx = (272 - width) / 2;
    RenderText(text, 480 - y, tx, color, style, angle);
  } else {
    int x = (480 - width) / 2;
    RenderText(text, x, y, color, style, 0.0f);
  }
}

int TextRenderer::MeasureTextWidth(const char *text, TextStyle style) {
  if (!text || text[0] == '\0')
    return 0;

  std::string key = GetCacheKey(text, style);
  auto it = metricsCache.find(key);
  if (it != metricsCache.end()) {
    return it->second;
  }

  TTF_Font *font = nullptr;
  if (currentMode == FontMode::INTER_ONLY) {
    font = fonts[style];
  } else if (currentMode == FontMode::FALLBACK_ONLY) {
    font = fallbackFonts[style] ? fallbackFonts[style] : fonts[style];
  } else {
    font = fonts[style];
    if (HasWideChars(text) && fallbackFonts[style]) {
      font = fallbackFonts[style];
    }
  }

  if (!font)
    return 0;

  int w, h;
  if (TTF_SizeUTF8(font, text, &w, &h) == 0) {
    metricsCache[key] = w;
    return w;
  }
  return 0;
}
