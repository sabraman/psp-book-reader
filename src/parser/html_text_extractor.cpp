#include "html_text_extractor.h"
#include <cctype>
#include <cstring>
#include <strings.h>

HtmlTextExtractor::HtmlTextExtractor() {}

HtmlTextExtractor::~HtmlTextExtractor() {}

bool HtmlTextExtractor::IsWhitespace(char c) {
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

int HtmlTextExtractor::ExtractWords(const char *html, char **words,
                                    int maxWords, char *wordBuffer,
                                    int bufferSize) {
  if (!html || !words || maxWords == 0 || !wordBuffer || bufferSize == 0)
    return 0;

  int wordCount = 0;
  int bufferPos = 0;
  bool inTag = false;
  bool inScript = false;
  bool inStyle = false;

  char currentWord[256];
  int currentWordLen = 0;

  for (int i = 0; html[i] && wordCount < maxWords; i++) {
    char c = html[i];

    if (c == '<') {
      if (currentWordLen > 0) {
        currentWord[currentWordLen] = '\0';
        if (bufferPos + currentWordLen + 1 < bufferSize) {
          strcpy(wordBuffer + bufferPos, currentWord);
          words[wordCount++] = wordBuffer + bufferPos;
          bufferPos += currentWordLen + 1;
        }
        currentWordLen = 0;
      }
      inTag = true;
      if (strncasecmp(&html[i], "<script", 7) == 0)
        inScript = true;
      else if (strncasecmp(&html[i], "<style", 6) == 0)
        inStyle = true;
      else if (strncasecmp(&html[i], "</script", 8) == 0)
        inScript = false;
      else if (strncasecmp(&html[i], "</style", 7) == 0)
        inStyle = false;
      else if (strncasecmp(&html[i], "<p", 2) == 0 ||
               strncasecmp(&html[i], "<br", 3) == 0 ||
               strncasecmp(&html[i], "<div", 4) == 0) {
        if (wordCount < maxWords && bufferPos + 2 < bufferSize) {
          strcpy(wordBuffer + bufferPos, "\n");
          words[wordCount++] = wordBuffer + bufferPos;
          bufferPos += 2;
        }
      }
    } else if (c == '>') {
      inTag = false;
    } else if (!inTag && !inScript && !inStyle) {
      if (IsWhitespace(c)) {
        if (currentWordLen > 0) {
          currentWord[currentWordLen] = '\0';
          if (bufferPos + currentWordLen + 1 < bufferSize) {
            strcpy(wordBuffer + bufferPos, currentWord);
            words[wordCount++] = wordBuffer + bufferPos;
            bufferPos += currentWordLen + 1;
          }
          currentWordLen = 0;
        }
      } else if (currentWordLen < 255) {
        currentWord[currentWordLen++] = c;
      }
    }
  }

  if (currentWordLen > 0 && wordCount < maxWords) {
    currentWord[currentWordLen] = '\0';
    if (bufferPos + currentWordLen + 1 < bufferSize) {
      strcpy(wordBuffer + bufferPos, currentWord);
      words[wordCount++] = wordBuffer + bufferPos;
    }
  }

  return wordCount;
}
