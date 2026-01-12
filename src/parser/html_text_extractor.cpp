#include "html_text_extractor.h"
#include "debug_logger.h"
#include <cctype>
#include <cstring>
#include <strings.h>

HtmlTextExtractor::HtmlTextExtractor() {}

HtmlTextExtractor::~HtmlTextExtractor() {}

bool HtmlTextExtractor::IsWhitespace(char c) {
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

int HtmlTextExtractor::ExtractWords(const char *html, char **words,
                                    TextStyle *styles, int maxWords,
                                    char *wordBuffer, int bufferSize) {
  if (!html || !words || !styles || maxWords == 0 || !wordBuffer ||
      bufferSize == 0)
    return 0;

  int wordCount = 0;
  int bufferPos = 0;
  bool inTag = false;
  bool inScript = false;
  bool inStyle = false;
  TextStyle currentStyle = TextStyle::NORMAL;

  char currentWord[256];
  int currentWordLen = 0;

  auto commitWord = [&]() {
    if (currentWordLen > 0 && wordCount < maxWords) {
      currentWord[currentWordLen] = '\0';
      if (bufferPos + currentWordLen + 1 < bufferSize) {
        strcpy(wordBuffer + bufferPos, currentWord);
        words[wordCount] = wordBuffer + bufferPos;
        styles[wordCount] = currentStyle;
        wordCount++;
        bufferPos += currentWordLen + 1;
      }
      currentWordLen = 0;
    }
  };

  auto pushNewline = [&]() {
    if (wordCount < maxWords && bufferPos + 2 < bufferSize) {
      strcpy(wordBuffer + bufferPos, "\n");
      words[wordCount] = wordBuffer + bufferPos;
      styles[wordCount] = TextStyle::NORMAL; // Newlines are style-neutral
      wordCount++;
      bufferPos += 2;
    }
  };

  for (int i = 0; html[i] && wordCount < maxWords; i++) {
    char c = html[i];

    if (c == '<') {
      commitWord();
      inTag = true;

      const char *t = &html[i + 1];
      if (*t == '/') {
        // Closing tags
        t++;
        if (strncasecmp(t, "h1", 2) == 0 || strncasecmp(t, "h2", 2) == 0 ||
            strncasecmp(t, "h3", 2) == 0) {
          currentStyle = TextStyle::NORMAL;
          pushNewline();
        } else if (strncasecmp(t, "script", 6) == 0) {
          inScript = false;
        } else if (strncasecmp(t, "style", 5) == 0) {
          inStyle = false;
        }
      } else {
        // Opening tags
        if (strncasecmp(t, "h1", 2) == 0) {
          currentStyle = TextStyle::H1;
          pushNewline();
        } else if (strncasecmp(t, "h2", 2) == 0) {
          currentStyle = TextStyle::H2;
          pushNewline();
        } else if (strncasecmp(t, "h3", 2) == 0) {
          currentStyle = TextStyle::H3;
          pushNewline();
        } else if (strncasecmp(t, "p", 1) == 0 ||
                   strncasecmp(t, "br", 2) == 0 ||
                   strncasecmp(t, "div", 3) == 0) {
          pushNewline();
        } else if (strncasecmp(t, "script", 6) == 0) {
          inScript = true;
        } else if (strncasecmp(t, "style", 5) == 0) {
          inStyle = true;
        }
      }
    } else if (c == '>') {
      inTag = false;
    } else if (!inTag && !inScript && !inStyle) {
      unsigned char uc = (unsigned char)c;
      // Check for CJK start byte (roughly 0xE0 - 0xEF for common CJK)
      if (uc >= 0xE0 && uc <= 0xEF) {
        // Commit pending word first
        commitWord();

        // Capture the 3-byte char as a standalone "word"
        if (html[i + 1] && html[i + 2]) {
          currentWord[0] = c;
          currentWord[1] = html[i + 1];
          currentWord[2] = html[i + 2];
          currentWordLen = 3;
          commitWord(); // Commit this character immediately
          i += 2;       // Skip next 2 bytes (loop adds 1 more)
        }
      } else if (IsWhitespace(c)) {
        commitWord();
      } else if (currentWordLen < 255) {
        currentWord[currentWordLen++] = c;
      }
    }
  }

  commitWord();
  if (wordCount < maxWords) {
    words[wordCount] = nullptr;
  }
  DebugLogger::Log("Extracted %d words", wordCount);
  return wordCount;
}
