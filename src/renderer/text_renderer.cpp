#include "text_renderer.h"
#include "debug_logger.h"
#include <cstdio>
#include <cstring>

TextRenderer::TextRenderer() : renderer(nullptr), fontScale(1.0f) {}

TextRenderer::~TextRenderer() { Shutdown(); }

bool TextRenderer::Initialize(SDL_Renderer *sdlRenderer) {
  renderer = sdlRenderer;
  if (TTF_Init() == -1) {
    DebugLogger::Log("TTF_Init failed: %s", TTF_GetError());
    return false;
  }
  return true;
}

void TextRenderer::Shutdown() {
  CleanupCache();
  for (auto &pair : fonts) {
    if (pair.second) {
      TTF_CloseFont(pair.second);
    }
  }
  fonts.clear();
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

bool TextRenderer::LoadFont(float scale) {
  for (auto &pair : fonts) {
    if (pair.second) {
      TTF_CloseFont(pair.second);
    }
  }
  fonts.clear();
  CleanupCache();

  fontScale = scale;
  const char *fontPath = "fonts/extras/ttf/Inter-Regular.ttf";

  auto loadOne = [&](TextStyle style, int baseSize) {
    int size = (int)(baseSize * fontScale);
    if (size < 8)
      size = 8;
    fonts[style] = TTF_OpenFont(fontPath, size);
    if (!fonts[style]) {
      DebugLogger::Log("TTF_OpenFont failed for style %d: %s", (int)style,
                       TTF_GetError());
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
  char buf[16];
  snprintf(buf, 16, "%d_", (int)style);
  return std::string(buf) + text;
}

void TextRenderer::RenderText(const char *text, int x, int y, uint32_t color,
                              TextStyle style, float angle) {
  TTF_Font *font = fonts[style];
  if (!font || !renderer || !text || strlen(text) == 0)
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
    // For rotated centering, we center relative to the 'vertical' height (272)
    // On PSP, screen dimensions are fixed.
    int tx = (272 - width) / 2;
    RenderText(text, 480 - y, tx, color, style, angle);
  } else {
    int x = (480 - width) / 2;
    RenderText(text, x, y, color, style, 0.0f);
  }
}

int TextRenderer::MeasureTextWidth(const char *text, TextStyle style) {
  TTF_Font *font = fonts[style];
  if (!font || !text || strlen(text) == 0)
    return 0;

  int w, h;
  if (TTF_SizeUTF8(font, text, &w, &h) == 0) {
    return w;
  }
  return 0;
}
