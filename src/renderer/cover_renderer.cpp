#include "cover_renderer.h"
#include "debug_logger.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <algorithm>

#include <string>

CoverRenderer::CoverRenderer() : cachedTexture(nullptr), cachedHref("") {}

CoverRenderer::~CoverRenderer() { ClearCache(); }

void CoverRenderer::ClearCache() {
  if (cachedTexture) {
    SDL_DestroyTexture(cachedTexture);
    cachedTexture = nullptr;
  }
  cachedHref = "";
}

bool CoverRenderer::ShowCover(SDL_Renderer *renderer, EpubReader &reader,
                              int timeoutMs) {
  if (!renderer)
    return false;

  const char *currentHref = reader.GetMetadata().coverHref;
  if (currentHref[0] == '\0') {
    ClearCache();
    DebugLogger::Log("No cover image found in metadata");
    return false;
  }

  if (cachedTexture && cachedHref == currentHref) {
    // Use cache
  } else {
    ClearCache();
    size_t coverSize = 0;
    uint8_t *coverData = reader.LoadCover(&coverSize);

    if (!coverData || coverSize == 0) {
      DebugLogger::Log("Failed to load cover data");
      return false;
    }

    // Load image from memory to surface first
    SDL_RWops *rw = SDL_RWFromMem(coverData, coverSize);
    SDL_Surface *surface = IMG_Load_RW(rw, 1);
    free(coverData);

    if (!surface) {
      DebugLogger::Log("IMG_Load_RW failed: %s", IMG_GetError());
      return false;
    }

    // Check for PSP 512x512 limit
    SDL_Surface *finalSurface = surface;
    if (surface->w > 512 || surface->h > 512) {
      float scale =
          std::min(512.0f / (float)surface->w, 512.0f / (float)surface->h);
      int sw = (int)(surface->w * scale);
      int sh = (int)(surface->h * scale);

      DebugLogger::Log(
          "Scaling cover from %dx%d down to %dx%d for PSP hardware limits",
          surface->w, surface->h, sw, sh);

      SDL_Surface *scaled = SDL_CreateRGBSurface(0, sw, sh, 32, 0, 0, 0, 0);
      SDL_BlitScaled(surface, NULL, scaled, NULL);
      SDL_FreeSurface(surface);
      finalSurface = scaled;
    }

    cachedTexture = SDL_CreateTextureFromSurface(renderer, finalSurface);
    SDL_FreeSurface(finalSurface);

    if (!cachedTexture) {
      DebugLogger::Log("SDL_CreateTextureFromSurface failed: %s",
                       SDL_GetError());
      return false;
    }
    cachedHref = currentHref;
  }

  int imgW, imgH;
  SDL_QueryTexture(cachedTexture, NULL, NULL, &imgW, &imgH);

  // Centering and Scaling
  float scale = std::min(480.0f / (float)imgW, 272.0f / (float)imgH);
  int targetW = (int)(imgW * scale);
  int targetH = (int)(imgH * scale);

  SDL_Rect dstRect = {(480 - targetW) / 2, (272 - targetH) / 2, targetW,
                      targetH};

  bool wait = true;
  SDL_Event event;
  uint32_t startTime = SDL_GetTicks();
  while (wait) {
    if (timeoutMs > 0 && (SDL_GetTicks() - startTime) >= (uint32_t)timeoutMs) {
      wait = false;
      break;
    }

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        wait = false;
      } else if (event.type == SDL_CONTROLLERBUTTONDOWN ||
                 event.type == SDL_JOYBUTTONDOWN || event.type == SDL_KEYDOWN) {
        wait = false;
      }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, cachedTexture, NULL, &dstRect);
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  return true;
}
