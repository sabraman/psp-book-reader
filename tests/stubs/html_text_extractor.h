#pragma once

#include "text_renderer.h"

// Simple HTML-to-text extractor for EPUB chapters with style detection
// Optimized for PSP-1000 (32MB RAM)

class HtmlTextExtractor {
public:
  HtmlTextExtractor();
  ~HtmlTextExtractor();

  // Extract words from HTML with style flags.
  // Returns the number of words found.
  //
  // Contract with the layout (processLayout in src/core/main.cpp):
  // - words[] entries [0, return value) point into wordBuffer. A single
  //   "\n" word is a line terminator; a run of two or more consecutive
  //   "\n" words renders as exactly one visible blank line.
  // - When the return value equals maxWords, no nullptr sentinel is
  //   written (it would be out of bounds). Bound iteration by maxWords,
  //   never by a sentinel.
  int ExtractWords(const char *html, char **words, TextStyle *styles,
                   int *wordLens, int maxWords, char *wordBuffer,
                   int bufferSize);

private:
  bool IsWhitespace(char c);
};
