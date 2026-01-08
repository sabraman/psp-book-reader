#pragma once

#include "text_renderer.h"

// Simple HTML-to-text extractor for EPUB chapters with style detection
// Optimized for PSP-1000 (32MB RAM)

class HtmlTextExtractor {
public:
  HtmlTextExtractor();
  ~HtmlTextExtractor();

  // Extract words from HTML with style flags
  // Returns number of words found.
  int ExtractWords(const char *html, char **words, TextStyle *styles,
                   int maxWords, char *wordBuffer, int bufferSize);

private:
  bool IsWhitespace(char c);
};
