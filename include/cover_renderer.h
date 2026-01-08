#pragma once

#include "epub_reader.h"
#include <SDL2/SDL.h>

// SDL2-based cover rendering utility
class CoverRenderer {
public:
  static bool ShowCover(SDL_Renderer *renderer, EpubReader &reader);
};
