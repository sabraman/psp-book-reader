#include "epub_reader.h"
#include "miniz.h"
#include "pugixml.hpp"
#include <cstdlib>
#include <cstring>

EpubReader::EpubReader() : zipArchive(nullptr) {
  memset(&metadata, 0, sizeof(metadata));
}

EpubReader::~EpubReader() { Close(); }

bool EpubReader::Open(const char *path) {
  // Allocate ZIP archive
  zipArchive = malloc(sizeof(mz_zip_archive));
  if (!zipArchive)
    return false;

  mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
  memset(zip, 0, sizeof(mz_zip_archive));

  // Open EPUB (which is a ZIP file)
  if (!mz_zip_reader_init_file(zip, path, 0)) {
    free(zipArchive);
    zipArchive = nullptr;
    return false;
  }

  // Read container.xml to find content.opf path
  char opfPath[256];
  if (!ReadContainerXml(opfPath)) {
    Close();
    return false;
  }

  // Extract and parse content.opf
  size_t opfSize;
  void *opfData = mz_zip_reader_extract_file_to_heap(zip, opfPath, &opfSize, 0);
  if (!opfData) {
    Close();
    return false;
  }

  bool success = ParseContentOpf((const uint8_t *)opfData);
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

  // Extract META-INF/container.xml
  size_t containerSize;
  void *containerData = mz_zip_reader_extract_file_to_heap(
      zip, "META-INF/container.xml", &containerSize, 0);
  if (!containerData)
    return false;

  // Parse XML
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(containerData, containerSize);
  mz_free(containerData);

  if (!result)
    return false;

  // Find rootfile path
  pugi::xml_node rootfile =
      doc.child("container").child("rootfiles").child("rootfile");
  const char *fullPath = rootfile.attribute("full-path").value();

  if (!fullPath || strlen(fullPath) == 0)
    return false;

  strncpy(outPath, fullPath, 255);
  outPath[255] = '\0';

  return true;
}

bool EpubReader::ParseContentOpf(const uint8_t *data) {
  pugi::xml_document doc;
  pugi::xml_parse_result result =
      doc.load_buffer(data, strlen((const char *)data));

  if (!result)
    return false;

  pugi::xml_node package = doc.child("package");

  // Parse metadata
  pugi::xml_node metadataNode = package.child("metadata");
  strncpy(metadata.title, metadataNode.child("dc:title").text().as_string(),
          127);
  strncpy(metadata.author, metadataNode.child("dc:creator").text().as_string(),
          127);

  // Parse spine (reading order)
  pugi::xml_node spine = package.child("spine");
  pugi::xml_node manifest = package.child("manifest");

  for (pugi::xml_node itemref : spine.children("itemref")) {
    const char *idref = itemref.attribute("idref").value();

    // Find corresponding item in manifest
    for (pugi::xml_node item : manifest.children("item")) {
      if (strcmp(item.attribute("id").value(), idref) == 0) {
        ChapterInfo chapter;
        strncpy(chapter.id, idref, 63);
        strncpy(chapter.href, item.attribute("href").value(), 127);

        // Get file info from ZIP
        mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
        int fileIndex =
            mz_zip_reader_locate_file(zip, chapter.href, nullptr, 0);
        if (fileIndex >= 0) {
          mz_zip_archive_file_stat fileStat;
          if (mz_zip_reader_file_stat(zip, fileIndex, &fileStat)) {
            chapter.compSize = fileStat.m_comp_size;
            chapter.uncompSize = fileStat.m_uncomp_size;
          }
        }

        metadata.spine.push_back(chapter);
        break;
      }
    }
  }

  return metadata.spine.size() > 0;
}

uint8_t *EpubReader::LoadChapter(int chapterIndex) {
  if (chapterIndex < 0 || chapterIndex >= (int)metadata.spine.size()) {
    return nullptr;
  }

  mz_zip_archive *zip = (mz_zip_archive *)zipArchive;
  ChapterInfo &chapter = metadata.spine[chapterIndex];

  // Extract chapter XHTML
  size_t chapterSize;
  uint8_t *chapterData = (uint8_t *)mz_zip_reader_extract_file_to_heap(
      zip, chapter.href, &chapterSize, 0);

  return chapterData;
}
