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

class TextRenderer {
public:
  TextRenderer();
  ~TextRenderer();

  bool Initialize(SDL_Renderer *sdlRenderer);
  void Shutdown();

  bool LoadFont(float scale);

  void RenderText(const char *text, int x, int y, uint32_t color,
                  TextStyle style = TextStyle::NORMAL, float angle = 0.0f);

  void RenderTextCentered(const char *text, int y, uint32_t color,
                          TextStyle style = TextStyle::NORMAL,
                          float angle = 0.0f);

  int MeasureTextWidth(const char *text, TextStyle style = TextStyle::NORMAL);

  float GetFontScale() const { return fontScale; }

  void ClearCache();

private:
  SDL_Renderer *renderer;
  std::unordered_map<TextStyle, TTF_Font *> fonts;
  float fontScale;

  struct CachedTexture {
    SDL_Texture *texture;
    int w, h;
  };

  // Cache key will now include the style
  std::unordered_map<std::string, CachedTexture> cache;

  void CleanupCache();
  std::string GetCacheKey(const char *text, TextStyle style);
};
