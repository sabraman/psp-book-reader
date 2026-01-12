#pragma once

#include <stdint.h>
#include <string>
#include <vector>

struct ChapterInfo {
  char id[64];
  char title[128];
  char href[128];
  uint32_t zipOffset;
  uint32_t compSize;
  uint32_t uncompSize;
};

struct EpubMetadata {
  char title[128];
  char author[128];
  char language[16];
  char coverHref[128];
  std::vector<ChapterInfo> spine;
};

// EPUB parser class
class EpubReader {
public:
  EpubReader();
  ~EpubReader();

  bool Open(const char *path);
  void Close();

  const EpubMetadata &GetMetadata() const { return metadata; }
  uint8_t *LoadChapter(int chapterIndex);
  uint8_t *LoadCover(size_t *outSize);

private:
  void *zipArchive;
  EpubMetadata metadata;

  bool ReadContainerXml(char *outPath);
  bool ParseContentOpf(const uint8_t *data, size_t size,
                       const std::string &rootDir);
};
