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

  bool ScanDirectory(SDL_Renderer *renderer, const std::string &path);
  const std::vector<BookEntry> &GetBooks() const { return books; }
  void Clear();

private:
  std::vector<BookEntry> books;
  SDL_Texture *CreateThumbnail(SDL_Renderer *renderer, EpubReader &reader);
};

#endif
