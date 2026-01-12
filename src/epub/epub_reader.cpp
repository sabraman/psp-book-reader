#include "epub_reader.h"
#include "debug_logger.h"
#include "miniz.h"
#include "pugixml.hpp"
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

EpubReader::EpubReader() : zipArchive(nullptr) {
  // Clear metadata
  metadata.title[0] = '\0';
  metadata.author[0] = '\0';
  metadata.language[0] = '\0';
  metadata.coverHref[0] = '\0';
  metadata.spine.clear();
}

EpubReader::~EpubReader() { Close(); }

bool EpubReader::Open(const char *path) {
  Close(); // Ensure any previous file is closed
  zipArchive = malloc(sizeof(mz_zip_archive));
  if (!zipArchive)
    return false;

  mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
  memset(zip, 0, sizeof(mz_zip_archive));

  if (!mz_zip_reader_init_file(zip, path, 0)) {
    free(zipArchive);
    zipArchive = nullptr;
    return false;
  }

  char opfPath[256];
  if (!ReadContainerXml(opfPath)) {
    Close();
    return false;
  }

  size_t opfSize;
  void *opfData = mz_zip_reader_extract_file_to_heap(zip, opfPath, &opfSize, 0);
  if (!opfData) {
    Close();
    return false;
  }

  std::string rootDir = "";
  std::string opfPathStr = opfPath;
  size_t lastSlash = opfPathStr.find_last_of("\\/");
  if (lastSlash != std::string::npos) {
    rootDir = opfPathStr.substr(0, lastSlash + 1);
  }

  bool success = ParseContentOpf((const uint8_t *)opfData, rootDir);
  mz_free(opfData);

  return success;
}

void EpubReader::Close() {
  if (zipArchive) {
    mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
    mz_zip_reader_end(zip);
    free(zipArchive);
    zipArchive = nullptr;
  }
  metadata.spine.clear();
}

bool EpubReader::ReadContainerXml(char *outPath) {
  mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
  size_t containerSize;
  void *containerData = mz_zip_reader_extract_file_to_heap(
      zip, "META-INF/container.xml", &containerSize, 0);
  if (!containerData)
    return false;

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(containerData, containerSize);
  mz_free(containerData);
  if (!result)
    return false;

  pugi::xml_node rootfile =
      doc.child("container").child("rootfiles").child("rootfile");
  const char *fullPath = rootfile.attribute("full-path").value();
  if (!fullPath || strlen(fullPath) == 0)
    return false;

  strncpy(outPath, fullPath, 255);
  outPath[255] = '\0';
  return true;
}

// Recursive helper for NCX parsing
void RecursiveParseNcx(pugi::xml_node parent, const std::string &rootDir,
                       std::map<std::string, std::string> &hrefToTitle) {
  for (pugi::xml_node navPoint : parent.children("navPoint")) {
    const char *label =
        navPoint.child("navLabel").child("text").text().as_string();
    const char *src = navPoint.child("content").attribute("src").value();

    if (label && src) {
      std::string srcStr = src;
      size_t hash = srcStr.find('#');
      if (hash != std::string::npos)
        srcStr = srcStr.substr(0, hash);

      // Resolve relative path
      std::string fullHref = rootDir + srcStr;

      // Only use the first title encountered for a href to avoid overwriting
      // main chapters with sub-sections
      if (hrefToTitle.find(fullHref) == hrefToTitle.end()) {
        hrefToTitle[fullHref] = label;
        DebugLogger::Log("NCX Match: %s -> %s", fullHref.c_str(), label);
      }
    }

    // Recurse into sub-points
    RecursiveParseNcx(navPoint, rootDir, hrefToTitle);
  }
}

bool EpubReader::ParseContentOpf(const uint8_t *data,
                                 const std::string &rootDir) {
  pugi::xml_document doc;
  pugi::xml_parse_result result =
      doc.load_buffer(data, strlen((const char *)data));
  if (!result)
    return false;

  pugi::xml_node package = doc.child("package");
  pugi::xml_node metadataNode = package.child("metadata");

  strncpy(metadata.title, metadataNode.child("dc:title").text().as_string(),
          127);
  strncpy(metadata.author, metadataNode.child("dc:creator").text().as_string(),
          127);
  strncpy(metadata.language,
          metadataNode.child("dc:language").text().as_string(), 15);

  // EPUB 2 cover detection
  std::string coverId = "";
  for (pugi::xml_node meta : metadataNode.children("meta")) {
    if (strcmp(meta.attribute("name").value(), "cover") == 0) {
      coverId = meta.attribute("content").value();
      break;
    }
  }

  const char *ncxId = package.child("spine").attribute("toc").value();
  std::map<std::string, std::string> manifestHrefs;
  std::string ncxHref = "";

  pugi::xml_node manifest = package.child("manifest");
  for (pugi::xml_node item : manifest.children("item")) {
    const char *itemId = item.attribute("id").value();
    const char *itemHref = item.attribute("href").value();
    const char *itemProps = item.attribute("properties").value();

    std::string fullHref = rootDir + itemHref;
    manifestHrefs[itemId] = fullHref;

    // Detect cover (EPUB 2 ID match or EPUB 3 property)
    if ((!coverId.empty() && strcmp(itemId, coverId.c_str()) == 0) ||
        (itemProps && strstr(itemProps, "cover-image"))) {
      strncpy(metadata.coverHref, fullHref.c_str(), 127);
      DebugLogger::Log("Cover Detected: %s", metadata.coverHref);
    }

    if (ncxId && strcmp(itemId, ncxId) == 0) {
      ncxHref = fullHref;
    }
  }

  // Recursive NCX parsing for all sub-chapters
  std::map<std::string, std::string> hrefToTitle;
  if (!ncxHref.empty()) {
    mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
    size_t ncxSize;
    void *ncxData =
        mz_zip_reader_extract_file_to_heap(zip, ncxHref.c_str(), &ncxSize, 0);
    if (ncxData) {
      pugi::xml_document ncxDoc;
      if (ncxDoc.load_buffer(ncxData, ncxSize)) {
        RecursiveParseNcx(ncxDoc.child("ncx").child("navMap"), rootDir,
                          hrefToTitle);
      }
      mz_free(ncxData);
    }
  }

  pugi::xml_node spine = package.child("spine");
  int chapterIdx = 1;
  for (pugi::xml_node itemref : spine.children("itemref")) {
    const char *idref = itemref.attribute("idref").value();
    if (manifestHrefs.count(idref)) {
      ChapterInfo chapter;
      strncpy(chapter.id, idref, 63);
      strncpy(chapter.href, manifestHrefs[idref].c_str(), 127);

      if (hrefToTitle.count(chapter.href)) {
        strncpy(chapter.title, hrefToTitle[chapter.href].c_str(), 127);
      } else {
        snprintf(chapter.title, 127, "Chapter %d", chapterIdx);
      }
      chapterIdx++;

      mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
      int fileIndex = mz_zip_reader_locate_file(zip, chapter.href, nullptr, 0);
      if (fileIndex >= 0) {
        mz_zip_archive_file_stat fileStat;
        if (mz_zip_reader_file_stat(zip, fileIndex, &fileStat)) {
          chapter.compSize = fileStat.m_comp_size;
          chapter.uncompSize = fileStat.m_uncomp_size;
        }
      }
      metadata.spine.push_back(chapter);
    }
  }

  return metadata.spine.size() > 0;
}

uint8_t *EpubReader::LoadChapter(int chapterIndex) {
  if (chapterIndex < 0 || chapterIndex >= (int)metadata.spine.size())
    return nullptr;
  mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
  ChapterInfo &chapter = metadata.spine[chapterIndex];
  size_t chapterSize;
  uint8_t *chapterData = (uint8_t *)mz_zip_reader_extract_file_to_heap(
      zip, chapter.href, &chapterSize, 0);
  if (chapterData) {
    uint8_t *terminated = (uint8_t *)realloc(chapterData, chapterSize + 1);
    if (terminated) {
      terminated[chapterSize] = '\0';
      return terminated;
    }
    return chapterData;
  }
  return nullptr;
}

uint8_t *EpubReader::LoadCover(size_t *outSize) {
  if (!zipArchive || strlen(metadata.coverHref) == 0)
    return nullptr;
  mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
  return (uint8_t *)mz_zip_reader_extract_file_to_heap(zip, metadata.coverHref,
                                                       outSize, 0);
}
