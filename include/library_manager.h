#ifndef LIBRARY_MANAGER_H
#define LIBRARY_MANAGER_H

#include "epub_reader.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>

struct BookEntry {
  std::string filename;
  std::string title;
  std::string author;
  SDL_Texture *thumbnail;
  int thumbW, thumbH;

  BookEntry() : thumbnail(nullptr), thumbW(0), thumbH(0) {}
};

class LibraryManager {
public:
  LibraryManager();
  ~LibraryManager();

  bool ScanDirectory(const std::string &path);
  void Clear();

  void LoadThumbnail(SDL_Renderer *renderer, int index);
  void UnloadThumbnail(int index);

  const std::vector<BookEntry> &GetBooks() const { return books; }

private:
  std::vector<BookEntry> books;
  SDL_Texture *CreateThumbnail(SDL_Renderer *renderer, EpubReader &reader);
};

#endif
