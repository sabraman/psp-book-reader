#include "chapter_renderer.h"
#include "text_renderer.h"
#include <cctype>
#include <cstring>

ChapterRenderer::ChapterRenderer() : currentPage(0), linesPerPage(12) {}

ChapterRenderer::~ChapterRenderer() {}

std::string ChapterRenderer::ExtractTextFromHTML(const char *html,
                                                 size_t size) {
  std::string result;
  result.reserve(size);

  bool inTag = false;
  bool inScript = false;
  bool inStyle = false;

  for (size_t i = 0; i < size; i++) {
    char c = html[i];

    // Track script/style tags
    if (c == '<') {
      inTag = true;
      // Check for script/style opening
      if (i + 7 < size && strncmp(&html[i], "<script", 7) == 0) {
        inScript = true;
      }
      if (i + 6 < size && strncmp(&html[i], "<style", 6) == 0) {
        inStyle = true;
      }
    } else if (c == '>') {
      // Check for script/style closing
      if (inScript && i >= 8 && strncmp(&html[i - 8], "</script", 8) == 0) {
        inScript = false;
      }
      if (inStyle && i >= 7 && strncmp(&html[i - 7], "</style", 7) == 0) {
        inStyle = false;
      }
      inTag = false;
      continue;
    }

    // Only add text when not in tags/script/style
    if (!inTag && !inScript && !inStyle) {
      // Convert multiple spaces/newlines to single space
      if (isspace(c)) {
        if (!result.empty() && result.back() != ' ') {
          result += ' ';
        }
      } else {
        result += c;
      }
    }
  }

  return result;
}

bool ChapterRenderer::LoadChapterText(const char *htmlContent,
                                      size_t contentSize) {
  rawText = ExtractTextFromHTML(htmlContent, contentSize);
  currentPage = 0;
  pages.clear();
  return !rawText.empty();
}

void ChapterRenderer::WrapText(const std::string &text, int maxWidth,
                               int fontSize) {
  pages.clear();
  std::vector<std::string> currentPageLines;

  // Simple word wrapping
  std::string currentLine;
  std::string word;

  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];

    if (c == ' ' || c == '\n') {
      if (!word.empty()) {
        // Check if adding word exceeds line width
        std::string testLine =
            currentLine.empty() ? word : currentLine + " " + word;

        // Rough estimate: ~8 pixels per character at font size 1.0
        if (testLine.length() * 8 > maxWidth) {
          // Start new line
          if (!currentLine.empty()) {
            currentPageLines.push_back(currentLine);

            // Check if page is full
            if (currentPageLines.size() >= linesPerPage) {
              pages.push_back(currentPageLines);
              currentPageLines.clear();
            }
          }
          currentLine = word;
        } else {
          currentLine = testLine;
        }
        word.clear();
      }
    } else {
      word += c;
    }
  }

  // Add remaining word and line
  if (!word.empty()) {
    if (!currentLine.empty())
      currentLine += " ";
    currentLine += word;
  }
  if (!currentLine.empty()) {
    currentPageLines.push_back(currentLine);
  }

  // Add final page if not empty
  if (!currentPageLines.empty()) {
    pages.push_back(currentPageLines);
  }

  currentPage = 0;
}

void ChapterRenderer::LayoutPages(int screenWidth, int screenHeight,
                                  int fontSize) {
  linesPerPage = (screenHeight - 40) / 16; // Rough line height
  WrapText(rawText, screenWidth - 20, fontSize);
}

const std::vector<std::string> &ChapterRenderer::GetCurrentPage() {
  static std::vector<std::string> empty;
  if (currentPage >= 0 && currentPage < (int)pages.size()) {
    return pages[currentPage];
  }
  return empty;
}

bool ChapterRenderer::NextPage() {
  if (currentPage < (int)pages.size() - 1) {
    currentPage++;
    return true;
  }
  return false;
}

bool ChapterRenderer::PrevPage() {
  if (currentPage > 0) {
    currentPage--;
    return true;
  }
  return false;
}
