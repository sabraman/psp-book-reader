#pragma once

#include "epub_reader.h"
#include <SDL2/SDL.h>

// SDL2-based cover rendering utility
class CoverRenderer {
public:
  CoverRenderer();
  ~CoverRenderer();

  bool ShowCover(SDL_Renderer *renderer, EpubReader &reader, int timeoutMs = 0);
  void ClearCache();

private:
  SDL_Texture *cachedTexture;
  std::string cachedHref;
};
