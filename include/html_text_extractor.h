#pragma once

// Simple HTML-to-text extractor for EPUB chapters
// Optimized for PSP-1000 (32MB RAM)

class HtmlTextExtractor {
public:
  HtmlTextExtractor();
  ~HtmlTextExtractor();

  // Extract words from HTML
  // Returns number of words found.
  int ExtractWords(const char *html, char **words, int maxWords,
                   char *wordBuffer, int bufferSize);

private:
  bool IsWhitespace(char c);
  void AddWord(const char *word, int wordLen, char *outputLines,
               int &currentLine, int &linePos, int maxLines, int maxLineLen);
};
