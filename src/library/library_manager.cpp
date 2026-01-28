#include "library_manager.h"
#include "debug_logger.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <sstream>
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

bool LibraryManager::ScanDirectory(const std::string &path) {
  Clear();
  std::string cachePath = path + "/library.cache";

  // Load existing metadata from cache for faster scanning
  std::map<std::string, std::pair<std::string, std::string>> cacheMap;
  std::ifstream cacheFile(cachePath);
  if (cacheFile.is_open()) {
    std::string line;
    while (std::getline(cacheFile, line)) {
      std::stringstream ss(line);
      std::string fname, title, author;
      if (std::getline(ss, fname, '|') && std::getline(ss, title, '|') &&
          std::getline(ss, author)) {
        cacheMap[fname] = {title, author};
      }
    }
    cacheFile.close();
  }

  DIR *dir = opendir(path.c_str());
  if (!dir) {
    DebugLogger::Log("Failed to open directory: %s", path.c_str());
    return false;
  }

  bool cacheDirty = false;
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.')
      continue;

    const char *ext = strrchr(entry->d_name, '.');
    if (ext && strcasecmp(ext, ".epub") == 0) {
      std::string fullPath = path + "/" + entry->d_name;

      if (cacheMap.count(fullPath)) {
        BookEntry book;
        book.filename = fullPath;
        book.title = cacheMap[fullPath].first;
        book.author = cacheMap[fullPath].second;
        books.push_back(book);
      } else {
        static EpubReader sharedReader;
        if (sharedReader.Open(fullPath.c_str())) {
          BookEntry book;
          book.filename = fullPath;
          book.title = sharedReader.GetMetadata().title;
          book.author = sharedReader.GetMetadata().author;

          // Fallback if metadata is empty
          if (book.title.empty())
            book.title = entry->d_name;
          if (book.author.empty())
            book.author = "Unknown";

          books.push_back(book);

          cacheMap[fullPath] = {book.title, book.author};
          cacheDirty = true;
          // DebugLogger::Log("Library found (new): %s", book.title.c_str());
        } else {
          // Fallback: Add file even if parsing fails (so it shows up)
          DebugLogger::Log("Failed to parse EPUB: %s. Adding fallback.",
                           entry->d_name);
          BookEntry book;
          book.filename = fullPath;
          book.title = entry->d_name; // Use filename as title
          book.author = "Unknown (Parse Error)";
          books.push_back(book);

          // Add to cache so we don't retry parsing every boot (unless user
          // deletes cache)
          cacheMap[fullPath] = {book.title, book.author};
          cacheDirty = true;
        }
      }
    }
  }
  closedir(dir);

  if (cacheDirty) {
    std::ofstream outFile(cachePath);
    if (outFile.is_open()) {
      for (const auto &book : books) {
        outFile << book.filename << "|" << book.title << "|" << book.author
                << "\n";
      }
      outFile.close();
      // DebugLogger::Log("Library cache updated: %s", cachePath.c_str());
    }
  }

  return !books.empty();
}

void LibraryManager::LoadThumbnail(SDL_Renderer *renderer, int index) {
  if (index < 0 || index >= (int)books.size() || books[index].thumbnail)
    return;

  // DebugLogger::Log("Loading thumbnail for: %s",
  // books[index].filename.c_str());
  EpubReader reader;
  if (reader.Open(books[index].filename.c_str())) {
    books[index].thumbnail = CreateThumbnail(renderer, reader);
    if (books[index].thumbnail) {
      SDL_QueryTexture(books[index].thumbnail, nullptr, nullptr,
                       &books[index].thumbW, &books[index].thumbH);
    } else {
      DebugLogger::Log("Thumbnail creation failed for: %s",
                       books[index].filename.c_str());
    }
  } else {
    DebugLogger::Log("Failed to open ebook for thumbnail: %s",
                     books[index].filename.c_str());
  }
}

void LibraryManager::UnloadThumbnail(int index) {
  if (index < 0 || index >= (int)books.size() || !books[index].thumbnail)
    return;

  SDL_DestroyTexture(books[index].thumbnail);
  books[index].thumbnail = nullptr;
  books[index].thumbW = 0;
  books[index].thumbH = 0;
}

SDL_Texture *LibraryManager::CreateThumbnail(SDL_Renderer *renderer,
                                             EpubReader &reader) {
  size_t size = 0;
  uint8_t *data = reader.LoadCover(&size);
  if (!data || size == 0)
    return nullptr;

  SDL_RWops *rw = SDL_RWFromMem(data, size);
  if (!rw) {
    DebugLogger::Log("Failed to create RWops for cover (%zu bytes)", size);
    free(data);
    return nullptr;
  }
  // DebugLogger::Log("CreateThumbnail: Loading image...");
  SDL_Surface *surface = IMG_Load_RW(rw, 1);
  free(data);

  if (!surface) {
    DebugLogger::Log("IMG_Load_RW error: %s", IMG_GetError());
    return nullptr;
  }
  // DebugLogger::Log("CreateThumbnail: Decoded %dx%d", surface->w, surface->h);

  // Target thumb size: ~100x150
  int tw = 100;
  int th = 150;
  float scale = std::min((float)tw / surface->w, (float)th / surface->h);
  int finalW = (int)(surface->w * scale);
  int finalH = (int)(surface->h * scale);

  // DebugLogger::Log("CreateThumbnail: Scaling to %dx%d", finalW, finalH);
  SDL_Surface *scaled = SDL_CreateRGBSurface(0, finalW, finalH, 32, 0, 0, 0, 0);
  if (!scaled) {
    DebugLogger::Log("SDL_CreateRGBSurface FAILED!");
    SDL_FreeSurface(surface);
    return nullptr;
  }
  SDL_BlitScaled(surface, nullptr, scaled, nullptr);
  SDL_FreeSurface(surface);

  // DebugLogger::Log("CreateThumbnail: Creating texture...");
  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, scaled);
  SDL_FreeSurface(scaled);

  if (tex) {
    // DebugLogger::Log("CreateThumbnail: SUCCESS");
  } else {
    DebugLogger::Log("SDL_CreateTextureFromSurface FAILED!");
  }

  return tex;
}
