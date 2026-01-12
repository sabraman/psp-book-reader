#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iterator>
#include <list>
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
  void RenderTextWithKey(const char *text, uint64_t key, int x, int y,
                         uint32_t color, TextStyle style = TextStyle::NORMAL,
                         float angle = 0.0f);

  void RenderTextCentered(const char *text, int y, uint32_t color,
                          TextStyle style = TextStyle::NORMAL,
                          float angle = 0.0f);
  void RenderTextCenteredWithKey(const char *text, uint64_t key, int y,
                                 uint32_t color,
                                 TextStyle style = TextStyle::NORMAL,
                                 float angle = 0.0f);

  int MeasureTextWidth(const char *text, TextStyle style = TextStyle::NORMAL);
  int MeasureTextWidthWithKey(const char *text, uint64_t key, TextStyle style);
  int GetLineHeight(TextStyle style = TextStyle::NORMAL);

  uint64_t GetCacheKey(const char *text, TextStyle style);

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
    std::list<uint64_t>::iterator lruIt;
  };

  // Cache key will now include the style
  // Cache key will now be a numeric hash
  std::unordered_map<uint64_t, CachedTexture> cache;
  struct MetricsEntry {
    int width;
    std::list<uint64_t>::iterator lruIt;
  };
  std::unordered_map<uint64_t, MetricsEntry> metricsCache;
  std::list<uint64_t> metricsLruList;

  void CleanupCache();
  void CloseFonts();
  // Use a combined hash of string + style for faster lookups
  // uint64_t GetCacheKey(const char *text, TextStyle style); // Moved to public

  const size_t MAX_CACHE_SIZE = 120;
  const size_t MAX_METRICS_CACHE_SIZE = 1000;
  std::list<uint64_t> lruList;
};
