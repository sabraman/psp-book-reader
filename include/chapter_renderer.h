#pragma once

#include <string>
#include <vector>

class ChapterRenderer {
public:
  ChapterRenderer();
  ~ChapterRenderer();

  // Load chapter HTML and extract text
  bool LoadChapterText(const char *htmlContent, size_t contentSize);

  // Layout text into pages based on screen dimensions
  void LayoutPages(int screenWidth, int screenHeight, int fontSize);

  // Get current page content
  const std::vector<std::string> &GetCurrentPage();

  // Navigation
  bool NextPage();
  bool PrevPage();
  int GetCurrentPageNumber() const { return currentPage + 1; }
  int GetTotalPages() const { return pages.size(); }

private:
  std::string ExtractTextFromHTML(const char *html, size_t size);
  void WrapText(const std::string &text, int maxWidth, int fontSize);

  std::string rawText;
  std::vector<std::vector<std::string>> pages; // Each page is a vector of lines
  int currentPage;
  int linesPerPage;
};
