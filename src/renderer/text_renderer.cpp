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
  ClearMetricsCache();
  CloseFonts();
  TTF_Quit();
}

void TextRenderer::CloseFonts() {
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
}

void TextRenderer::CleanupCache() {
  for (auto &pair : cache) {
    if (pair.second.texture) {
      SDL_DestroyTexture(pair.second.texture);
    }
  }
  cache.clear();
  lruList.clear();
}

void TextRenderer::ClearCache() { CleanupCache(); }

void TextRenderer::ClearMetricsCache() {
  metricsCache.clear();
  metricsLruList.clear();
}

void TextRenderer::SetFontMode(FontMode mode) {
  if (currentMode != mode) {
    currentMode = mode;
    ClearCache();
    ClearMetricsCache();
  }
}

bool TextRenderer::LoadFont(float scale) {
  if (fontScale == scale && IsValid())
    return true;

  CloseFonts();
  ClearCache();
  ClearMetricsCache();

  fontScale = scale;
  const char *primaryPath = "fonts/Inter-Regular.ttf";
  const char *fallbackPath = "fonts/DroidSansFallback.ttf";

  auto loadOne = [&](TextStyle style, int baseSize) {
    int size = (int)(baseSize * fontScale);
    if (size < 8)
      size = 8;

    fonts[style] = TTF_OpenFont(primaryPath, size);
    if (!fonts[style]) {
      DebugLogger::Log("Failed loading primary font %d: %s", (int)style,
                       TTF_GetError());
    }

    fallbackFonts[style] = TTF_OpenFont(fallbackPath, size);
  };

  loadOne(TextStyle::NORMAL, 18);
  loadOne(TextStyle::H1, 26);
  loadOne(TextStyle::H2, 22);
  loadOne(TextStyle::H3, 19);
  loadOne(TextStyle::TITLE, 34);
  loadOne(TextStyle::SMALL, 14);

  return !fonts.empty();
}

uint64_t TextRenderer::GetCacheKey(const char *text, TextStyle style) {
  uint64_t hash = 14695981039346656037ULL;
  hash ^= (uint64_t)style;
  hash *= 1099511628211ULL;
  hash ^= (uint64_t)currentMode;
  hash *= 1099511628211ULL;
  const unsigned char *u = (const unsigned char *)text;
  while (*u) {
    hash ^= *u++;
    hash *= 1099511628211ULL;
  }
  return hash;
}

void TextRenderer::RenderText(const char *text, int x, int y, uint32_t color,
                              TextStyle style, float angle) {
  RenderTextWithKey(text, GetCacheKey(text, style), x, y, color, style, angle);
}

void TextRenderer::RenderTextWithKey(const char *text, uint64_t key, int x,
                                     int y, uint32_t color, TextStyle style,
                                     float angle) {
  if (!renderer || !text || text[0] == '\0')
    return;

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
    return;

  CachedTexture *cached = nullptr;

  auto it = cache.find(key);
  if (it != cache.end()) {
    cached = &it->second;
    // Update LRU: move to back
    lruList.erase(cached->lruIt);
    lruList.push_back(key);
    cached->lruIt = std::prev(lruList.end());
  } else {
    // Evict if cache full
    if (cache.size() >= MAX_CACHE_SIZE && !lruList.empty()) {
      uint64_t oldKey = lruList.front();
      lruList.pop_front();
      if (cache[oldKey].texture) {
        SDL_DestroyTexture(cache[oldKey].texture);
      }
      cache.erase(oldKey);
    }

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, white);
    if (!surface)
      return;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
      SDL_FreeSurface(surface);
      return;
    }

    lruList.push_back(key);
    CachedTexture newEntry = {texture, surface->w, surface->h,
                              std::prev(lruList.end())};
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
  RenderTextCenteredWithKey(text, GetCacheKey(text, style), y, color, style,
                            angle);
}

void TextRenderer::RenderTextCenteredWithKey(const char *text, uint64_t key,
                                             int y, uint32_t color,
                                             TextStyle style, float angle) {
  int width = MeasureTextWidthWithKey(text, key, style);
  if (angle != 0.0f) {
    int tx = (272 - width) / 2;
    RenderTextWithKey(text, key, 480 - y, tx, color, style, angle);
  } else {
    int x = (480 - width) / 2;
    RenderTextWithKey(text, key, x, y, color, style, 0.0f);
  }
}

int TextRenderer::MeasureTextWidth(const char *text, TextStyle style) {
  return MeasureTextWidthWithKey(text, GetCacheKey(text, style), style);
}

int TextRenderer::MeasureTextWidthWithKey(const char *text, uint64_t key,
                                          TextStyle style) {
  if (!text || text[0] == '\0')
    return 0;

  auto it = metricsCache.find(key);
  if (it != metricsCache.end()) {
    // Update metrics LRU: move to back
    metricsLruList.erase(it->second.lruIt);
    metricsLruList.push_back(key);
    it->second.lruIt = std::prev(metricsLruList.end());
    return it->second.width;
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
    // Evict if metrics cache full
    if (metricsCache.size() >= MAX_METRICS_CACHE_SIZE &&
        !metricsLruList.empty()) {
      uint64_t oldKey = metricsLruList.front();
      metricsLruList.pop_front();
      metricsCache.erase(oldKey);
    }

    metricsLruList.push_back(key);
    metricsCache[key] = {w, std::prev(metricsLruList.end())};
    return w;
  }
  return 0;
}

int TextRenderer::GetLineHeight(TextStyle style) {
  TTF_Font *font = fonts[style];
  if (!font)
    return 0;
  return TTF_FontHeight(font);
}
void TextRenderer::SetTheme(Theme theme) {
  switch (theme) {
  case Theme::SEPIA:
    themeColors = ThemeColors(0xFFCCE8FF, // Background: Cream/Sepia
                              0xFF202050, // Text: Dark Brown
                              0xFF101030, // Heading: Deep Brown
                              0xFF606090, // Dimmed
                              0xFF8080C0  // Selection
    );
    break;
  case Theme::LIGHT:
    themeColors = ThemeColors(0xFFFFFFFF, // Background: White
                              0xFF202020, // Text: Near Black
                              0xFF000000, // Heading: Black
                              0xFF808080, // Dimmed
                              0xFFDDDDDD  // Selection
    );
    break;
  case Theme::NIGHT:
  default:
    themeColors = ThemeColors(0xFF000000, // Background: Black
                              0xFFDDDDDD, // Text: Light Gray
                              0xFFFFFFFF, // Heading: White
                              0xFF888888, // Dimmed
                              0xFF00C8FF  // Selection: Cyan
    );
    break;
  }
}
