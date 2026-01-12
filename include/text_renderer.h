#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

enum class TextStyle {
  NORMAL,
  H1,    // Large header
  H2,    // Medium header
  H3,    // Small header
  TITLE, // Very large for title page
  SMALL  // For footer/status
};

enum class FontMode { SMART, INTER_ONLY, FALLBACK_ONLY };

class TextRenderer {
public:
  TextRenderer();
  ~TextRenderer();

  bool Initialize(SDL_Renderer *sdlRenderer);
  void Shutdown();

  bool LoadFont(float scale);

  void SetFontMode(FontMode mode);

  void RenderText(const char *text, int x, int y, uint32_t color,
                  TextStyle style = TextStyle::NORMAL, float angle = 0.0f);

  void RenderTextCentered(const char *text, int y, uint32_t color,
                          TextStyle style = TextStyle::NORMAL,
                          float angle = 0.0f);

  int MeasureTextWidth(const char *text, TextStyle style = TextStyle::NORMAL);
  int GetLineHeight(TextStyle style = TextStyle::NORMAL);

  float GetFontScale() const { return fontScale; }

  void ClearCache();
  void ClearMetricsCache();
  bool IsValid() const { return !fonts.empty(); }

private:
  SDL_Renderer *renderer;
  std::unordered_map<TextStyle, TTF_Font *> fonts;
  std::unordered_map<TextStyle, TTF_Font *> fallbackFonts;
  float fontScale;
  FontMode currentMode;

  struct CachedTexture {
    SDL_Texture *texture;
    int w, h;
  };

  // Cache key will now include the style
  // Cache key will now be a numeric hash
  std::unordered_map<uint64_t, CachedTexture> cache;
  // Word width cache
  std::unordered_map<uint64_t, int> metricsCache;

  void CleanupCache();
  void CloseFonts();
  // Use a combined hash of string + style for faster lookups
  uint64_t GetCacheKey(const char *text, TextStyle style);

  const size_t MAX_CACHE_SIZE = 120;
  std::vector<uint64_t> lruList;
};
