#include "library_manager.h"
#include "debug_logger.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

LibraryManager::LibraryManager() {}

LibraryManager::~LibraryManager() { Clear(); }

void LibraryManager::Clear() {
  for (auto &book : books) {
    if (book.thumbnail) {
      SDL_DestroyTexture(book.thumbnail);
    }
  }
  books.clear();
}

bool LibraryManager::ScanDirectory(SDL_Renderer *renderer,
                                   const std::string &path) {
  Clear();
  DIR *dir = opendir(path.c_str());
  if (!dir) {
    DebugLogger::Log("Failed to open directory: %s", path.c_str());
    // Try fallback to standard PSP path
    dir = opendir("ms0:/PSP/GAME/pspepub/books/");
    if (!dir)
      return false;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.')
      continue;

    const char *ext = strrchr(entry->d_name, '.');
    if (ext && strcasecmp(ext, ".epub") == 0) {
      std::string fullPath = path + "/" + entry->d_name;
      EpubReader reader;
      if (reader.Open(fullPath.c_str())) {
        BookEntry book;
        book.filename = fullPath;
        book.title = reader.GetMetadata().title;
        book.author = reader.GetMetadata().author;
        book.thumbnail = CreateThumbnail(renderer, reader);

        // Get thumb dimensions
        if (book.thumbnail) {
          SDL_QueryTexture(book.thumbnail, nullptr, nullptr, &book.thumbW,
                           &book.thumbH);
        }

        books.push_back(book);
        DebugLogger::Log("Library found: %s", book.title.c_str());
      }
    }
  }
  closedir(dir);
  return !books.empty();
}

SDL_Texture *LibraryManager::CreateThumbnail(SDL_Renderer *renderer,
                                             EpubReader &reader) {
  size_t size = 0;
  uint8_t *data = reader.LoadCover(&size);
  if (!data || size == 0)
    return nullptr;

  SDL_RWops *rw = SDL_RWFromMem(data, size);
  SDL_Surface *surface = IMG_Load_RW(rw, 1);
  free(data);

  if (!surface)
    return nullptr;

  // Target thumb size: ~100x150
  int tw = 100;
  int th = 150;
  float scale = std::min((float)tw / surface->w, (float)th / surface->h);
  int finalW = (int)(surface->w * scale);
  int finalH = (int)(surface->h * scale);

  SDL_Surface *scaled = SDL_CreateRGBSurface(0, finalW, finalH, 32, 0, 0, 0, 0);
  SDL_BlitScaled(surface, nullptr, scaled, nullptr);
  SDL_FreeSurface(surface);

  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, scaled);
  SDL_FreeSurface(scaled);

  return tex;
}
