#include "cover_renderer.h"
#include "debug_logger.h"
#include "epub_reader.h"
#include "html_text_extractor.h"
#include "input_handler.h"
#include "library_manager.h"
#include "text_renderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <string>
#include <vector>

// PSP Screen Dimensions
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

// Reader Constraints
#define MAX_CHAPTER_LINES 5000
#define MAX_WORDS 20000
#define WORD_BUFFER_SIZE 262144
#define MAX_LINE_LEN 256

// Reader Layout Constants
#define LAYOUT_MARGIN 24
#define LAYOUT_START_Y 45
#define LAYOUT_LINE_SPACE 24.0f

struct LineInfo {
  char text[MAX_LINE_LEN];
  TextStyle style;
};

static LineInfo chapterLines[MAX_CHAPTER_LINES];
static int totalLines = 0;
static int currentLine = 0;
static float readerFontScale = 1.0f;
static bool isRotated = false;
static bool showChapterMenu = false;
static bool showStatusOverlay = false;
static int linesPerPage = 10;
static int menuSelection = 0;
static int menuScroll = 0;

// Background Layout State
struct LayoutState {
  int chapterIndex = -1;
  int wordIdx = 0;
  int lineCount = 0;
  bool complete = true;
  bool needsReset = false;
} layoutState;

static std::vector<int> pageAnchors; // wordIndex for start of each page
static int currentPageIdx = 0;

static char *words[MAX_WORDS];
static TextStyle wordStyles[MAX_WORDS];
static char wordBuffer[WORD_BUFFER_SIZE];

enum AppState { STATE_LIBRARY, STATE_READER };
static AppState currentState = STATE_LIBRARY;

int running = 0;

const char *findStringInsensitive(const char *haystack, const char *needle) {
  if (!haystack || !needle || !*needle)
    return NULL;
  int nlen = strlen(needle);
  int hlen = strlen(haystack);
  for (int i = 0; i <= hlen - nlen; i++) {
    if (strncasecmp(&haystack[i], needle, nlen) == 0)
      return &haystack[i];
  }
  return NULL;
}

bool isRedundantMetadata(const char *text, const EpubMetadata &meta) {
  if (!text || strlen(text) < 3)
    return false;

  // Strict match for title/author
  if (strcasecmp(text, meta.title) == 0)
    return true;
  if (strcasecmp(text, meta.author) == 0)
    return true;

  // Partially redundant book title/author at the top of pages (e.g. "Моби Дик"
  // in "Моби Дик, или Белый Кит")
  if (findStringInsensitive(text, meta.title) ||
      findStringInsensitive(text, meta.author) ||
      findStringInsensitive(
          meta.title, text)) { // Also check if text is a substring of title
    int textLen = strlen(text);
    if (textLen < (int)strlen(meta.title) + 15 ||
        textLen < (int)strlen(meta.author) + 15) {
      return true;
    }
  }

  if (findStringInsensitive(text, "Cover of"))
    return true;
  return false;
}

void resetLayout(int chapterIndex, EpubReader &reader,
                 HtmlTextExtractor &extractor) {
  if (chapterIndex < 0)
    return;

  uint8_t *raw_data = reader.LoadChapter(chapterIndex);
  if (!raw_data)
    return;

  memset(wordBuffer, 0, WORD_BUFFER_SIZE);
  extractor.ExtractWords((char *)raw_data, words, wordStyles, MAX_WORDS,
                         wordBuffer, WORD_BUFFER_SIZE);
  free(raw_data);

  layoutState.chapterIndex = chapterIndex;
  layoutState.wordIdx = 0;
  layoutState.lineCount = 0;
  layoutState.complete = false;
  layoutState.needsReset = false;

  totalLines = 0;
  currentLine = 0;
  currentPageIdx = 0;
  pageAnchors.clear();
  pageAnchors.push_back(0); // Page 1 starts at word 0

  DebugLogger::Log("Layout Reset for Ch %d", chapterIndex);
}

bool processLayout(EpubReader &reader, TextRenderer &renderer,
                   int maxWords = 200) {
  if (layoutState.complete || layoutState.chapterIndex < 0)
    return true;

  int maxWidth = isRotated ? (SCREEN_HEIGHT - 2 * LAYOUT_MARGIN)
                           : (SCREEN_WIDTH - 2 * LAYOUT_MARGIN);
  int availableHeight =
      (isRotated ? SCREEN_WIDTH : SCREEN_HEIGHT) - LAYOUT_START_Y - 40;
  linesPerPage = availableHeight / (int)(LAYOUT_LINE_SPACE * readerFontScale);
  if (linesPerPage < 1)
    linesPerPage = 1;

  const EpubMetadata &meta = reader.GetMetadata();
  int wordsProcessed = 0;

  while (layoutState.wordIdx < MAX_WORDS &&
         words[layoutState.wordIdx] != nullptr && wordsProcessed < maxWords) {
    if (words[layoutState.wordIdx][0] == '\n') {
      layoutState.wordIdx++;
      wordsProcessed++;
      continue;
    }

    std::string currentLineStr = "";
    TextStyle currentLineStyle = wordStyles[layoutState.wordIdx];

    while (layoutState.wordIdx < MAX_WORDS &&
           words[layoutState.wordIdx] != nullptr) {
      if (words[layoutState.wordIdx][0] == '\n')
        break;

      std::string testLine =
          currentLineStr.empty()
              ? words[layoutState.wordIdx]
              : (currentLineStr + " " + words[layoutState.wordIdx]);
      // DebugLogger::Log("Measuring: %s", testLine.c_str());
      int width = renderer.MeasureTextWidth(testLine.c_str(),
                                            wordStyles[layoutState.wordIdx]);

      if (width > maxWidth && !currentLineStr.empty())
        break;

      currentLineStr = testLine;
      currentLineStyle = wordStyles[layoutState.wordIdx];
      layoutState.wordIdx++;
      wordsProcessed++;
      if (wordsProcessed >= maxWords)
        break;
    }

    if (!currentLineStr.empty() && totalLines < MAX_CHAPTER_LINES) {
      if (totalLines < 15 &&
          isRedundantMetadata(currentLineStr.c_str(), meta)) {
        // Skip metadata noise
      } else {
        strncpy(chapterLines[totalLines].text, currentLineStr.c_str(),
                MAX_LINE_LEN - 1);
        chapterLines[totalLines].text[MAX_LINE_LEN - 1] = '\0';
        chapterLines[totalLines].style = currentLineStyle;
        totalLines++;

        if (totalLines > 0 && totalLines % linesPerPage == 0) {
          pageAnchors.push_back(layoutState.wordIdx);
        }
      }
    }

    if (wordsProcessed >= maxWords)
      break;
  }

  if (layoutState.wordIdx >= MAX_WORDS ||
      words[layoutState.wordIdx] == nullptr) {
    layoutState.complete = true;
    DebugLogger::Log("Layout Complete: %d lines", totalLines);

    if (layoutState.chapterIndex <= 1 && totalLines < 5) {
      bool allMeta = true;
      for (int i = 0; i < totalLines; i++)
        if (!isRedundantMetadata(chapterLines[i].text, meta))
          allMeta = false;
      if (allMeta)
        totalLines = 0;
    }
  }

  return layoutState.complete;
}

int main(int argc, char *argv[]) {
  DebugLogger::Init();
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) <
      0)
    return 1;

  SDL_Joystick *joy = nullptr;
  if (SDL_NumJoysticks() > 0)
    joy = SDL_JoystickOpen(0);

  SDL_Window *window =
      SDL_CreateWindow("PSP-BookReader", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 480, 272, 0);
  SDL_Renderer *sdlRenderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  SDL_Event event;

  TextRenderer renderer;
  renderer.Initialize(sdlRenderer);
  renderer.LoadFont(1.0f);

  LibraryManager library;
  library.ScanDirectory("books");

  EpubReader reader;
  HtmlTextExtractor htmlExtractor;
  int currentChapter = -1;

  InputHandler input;
  running = 1;

  int libSelection = 0;
  uint32_t frameCount = 0;
  bool isScanning = true;

  while (running) {
    frameCount++;
    input.Update();
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = 0;
      input.ProcessEvent(event);
    }
    if (input.Exit())
      break;

    if (currentState == STATE_LIBRARY) {
      if (isScanning) {
        SDL_SetRenderDrawColor(sdlRenderer, 15, 15, 20, 255);
        SDL_RenderClear(sdlRenderer);
        renderer.RenderTextCentered("SCANNING LIBRARY...", 120, 0xFFFFFFFF,
                                    TextStyle::H2);
        SDL_RenderPresent(sdlRenderer);
        library.ScanDirectory("books");
        isScanning = false;
        continue;
      }

      // --- LIBRARY LOGIC ---
      const auto &books = library.GetBooks();
      if (!books.empty()) {
        if (input.LeftPressed())
          libSelection = std::max(0, libSelection - 1);
        if (input.RightPressed())
          libSelection = std::min((int)books.size() - 1, libSelection + 1);
        if (input.LTriggerPressed())
          libSelection = std::max(0, libSelection - 4);
        if (input.RTriggerPressed())
          libSelection = std::min((int)books.size() - 1, libSelection + 4);

        if (input.CrossPressed()) {
          // DebugLogger::Log("Opening book: %s",
          //                  books[libSelection].filename.c_str());
          if (reader.Open(books[libSelection].filename.c_str())) {
            // DebugLogger::Log("Book opened successfully");
            currentState = STATE_READER;
            currentChapter = -1;
            layoutState.complete = true;
            layoutState.chapterIndex = -1;
            renderer.LoadFont(readerFontScale);

            // Select Font Mode based on Language
            const char *lang = reader.GetMetadata().language;
            if (lang &&
                (strncmp(lang, "zh", 2) == 0 || strncmp(lang, "ja", 2) == 0 ||
                 strncmp(lang, "ko", 2) == 0)) {
              renderer.SetFontMode(FontMode::FALLBACK_ONLY);
              DebugLogger::Log("Language: %s -> Mode: FALLBACK_ONLY", lang);
            } else {
              renderer.SetFontMode(FontMode::INTER_ONLY);
              DebugLogger::Log("Language: %s -> Mode: INTER_ONLY",
                               lang ? lang : "none");
            }

            if (!renderer.IsValid()) {
              DebugLogger::Log("ERROR: Fonts failed to load!");
            }
            // DebugLogger::Log("Fonts loaded. Clearing cache...");
            renderer.ClearCache();
            // DebugLogger::Log("Ready to render");
          } else {
            // DebugLogger::Log("Failed to open book");
          }
        }
      }

      // --- LIBRARY RENDER ---
      // Background Gradient Simulate
      for (int i = 0; i < 272; i++) {
        uint8_t r = 10 + (i * 20 / 272);
        uint8_t g = 10 + (i * 20 / 272);
        uint8_t b = 25 + (i * 30 / 272);
        SDL_SetRenderDrawColor(sdlRenderer, r, g, b, 255);
        SDL_RenderDrawLine(sdlRenderer, 0, i, 480, i);
      }

      // Bookshelf lines
      SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 40);
      SDL_RenderDrawLine(sdlRenderer, 20, 205, 460, 205);

      renderer.RenderText("PICK A BOOK", 40, 20, 0xFFFFFFFF, TextStyle::SMALL);

      char countBuf[32];
      snprintf(countBuf, 32, "%d BOOKS", (int)books.size());
      renderer.RenderText(countBuf, 380, 20, 0xFF888888, TextStyle::SMALL);

      if (books.empty()) {
        renderer.RenderTextCentered("No books found in /books/", 120,
                                    0xFF888888);
      } else {
        int startX = 40;
        int spacing = 110;
        // Simple slider animation logic (scroll logic)
        // Lazy loading/unloading logic
        int scrollOffset = (libSelection > 3) ? (libSelection - 3) : 0;
        for (int i = 0; i < (int)books.size(); i++) {
          if (i >= scrollOffset && i < scrollOffset + 4) {
            library.LoadThumbnail(sdlRenderer, i);
          } else {
            // Unload if far away (e.g. > 10 items away) to avoid thrashing
            if (abs(i - libSelection) > 10) {
              library.UnloadThumbnail(i);
            }
          }
        }

        for (int i = 0; i < 4 && (scrollOffset + i) < (int)books.size(); i++) {
          int idx = scrollOffset + i;
          const auto &book = books[idx];
          int bx = startX + i * spacing;
          int by = 50;

          // Determine dimensions
          int w = book.thumbW;
          int h = book.thumbH;
          // Fallback dimensions if 0
          if (w == 0 || h == 0) {
            w = 100;
            h = 150;
          }

          // Shadow (Always render shadow)
          SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 100);
          SDL_Rect shadow = {bx + 4, by + 4, w, h};
          SDL_RenderFillRect(sdlRenderer, &shadow);

          // Content (Thumbnail or Placeholder)
          SDL_Rect dst = {bx, by, w, h};
          if (book.thumbnail) {
            SDL_RenderCopy(sdlRenderer, book.thumbnail, nullptr, &dst);
          } else {
            // Placeholder: Slate/Grey rectangle
            SDL_SetRenderDrawColor(sdlRenderer, 60, 70, 80, 255);
            SDL_RenderFillRect(sdlRenderer, &dst);

            // Placeholder text lines effect
            SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 50);
            for (int k = 0; k < 3; k++) {
              SDL_Rect line = {bx + 10, by + 30 + (k * 20), w - 20, 10};
              SDL_RenderFillRect(sdlRenderer, &line);
            }
          }

          // Selection Highlight
          if (idx == libSelection) {
            float pulse = (sinf(frameCount * 0.2f) + 1.0f) * 0.5f;
            SDL_SetRenderDrawColor(sdlRenderer, 0, 200, 255,
                                   150 + (int)(pulse * 105));
            for (int t = 0; t < 3; t++) {
              SDL_Rect border = {bx - t, by - t, w + 2 * t, h + 2 * t};
              SDL_RenderDrawRect(sdlRenderer, &border);
            }
          }
        }

        // Glassmorphic Selection Detail
        SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 20);
        SDL_Rect glass = {0, 215, 480, 57};
        SDL_RenderFillRect(sdlRenderer, &glass);

        const auto &sel = books[libSelection];
        // Title with shadow for readability
        renderer.RenderText(sel.title.c_str(), 42, 222, 0xFF000000,
                            TextStyle::NORMAL); // shadow
        renderer.RenderText(sel.title.c_str(), 40, 220, 0xFFFFFFFF,
                            TextStyle::NORMAL);
        renderer.RenderText(sel.author.c_str(), 40, 242, 0xFFAAAAAA,
                            TextStyle::SMALL);

        // Selection indicator (dots)
        for (int i = 0; i < (int)books.size(); i++) {
          int dotX = 240 - (books.size() * 10 / 2) + i * 10;
          if (i == libSelection)
            SDL_SetRenderDrawColor(sdlRenderer, 0, 200, 255, 255);
          else
            SDL_SetRenderDrawColor(sdlRenderer, 150, 150, 150, 150);
          SDL_Rect dot = {dotX, 10, 6, 6};
          SDL_RenderFillRect(sdlRenderer, &dot);
        }
      }
    } else {
      // --- READER LOGIC ---
      const EpubMetadata &meta = reader.GetMetadata();
      // Background layout processing
      if (!layoutState.complete) {
        processLayout(reader, renderer, 200); // Process 200 words per frame
      }

      bool layoutNeedsReset = false;
      if (showChapterMenu) {
        int visibleMax = isRotated ? 22 : 10;
        if (input.UpPressed()) {
          menuSelection = std::max(0, menuSelection - 1);
          if (menuSelection < menuScroll)
            menuScroll = menuSelection;
        }
        if (input.DownPressed()) {
          menuSelection =
              std::min((int)meta.spine.size() - 1, menuSelection + 1);
          if (menuSelection >= menuScroll + visibleMax)
            menuScroll = menuSelection - visibleMax + 1;
        }
        if (input.CrossPressed()) {
          currentChapter = menuSelection;
          layoutNeedsReset = true;
          currentLine = 0;
          showChapterMenu = false;
          renderer.ClearCache();
        }
        if (input.TrianglePressed())
          showChapterMenu = false;
      } else {
        if (input.NextPage()) {
          if (currentChapter == -1) {
            currentChapter = 0;
            resetLayout(currentChapter, reader, htmlExtractor);
            processLayout(reader, renderer, 500); // Immediate feel
            if (totalLines == 0) {
              currentChapter = 1;
              layoutNeedsReset = true;
            }
            currentLine = 0;
            renderer.ClearCache();
          } else if (currentLine + linesPerPage < totalLines ||
                     !layoutState.complete) {
            if (currentLine + linesPerPage < totalLines) {
              currentLine += linesPerPage;
              renderer.ClearCache();
            } else {
              // Speed up layout if user is waiting
              processLayout(reader, renderer, 1000);
            }
          } else if (currentChapter < (int)meta.spine.size() - 1) {
            currentChapter++;
            layoutNeedsReset = true;
            currentLine = 0;
            renderer.ClearCache();
          }
        }
        if (input.PrevPage()) {
          if (currentLine >= linesPerPage) {
            currentLine -= linesPerPage;
            renderer.ClearCache();
          } else if (currentChapter > 0) {
            currentChapter--;
            resetLayout(currentChapter, reader, htmlExtractor);
            processLayout(reader, renderer, 10000);
            if (currentChapter == 0 && totalLines == 0) {
              currentChapter = -1;
              renderer.ClearCache();
            } else {
              currentLine = ((totalLines - 1) / linesPerPage) * linesPerPage;
              if (currentLine < 0)
                currentLine = 0;
              renderer.ClearCache();
            }
          } else if (currentChapter == 0) {
            currentChapter = -1;
            renderer.ClearCache();
          }
        }
        if (input.CirclePressed()) {
          isRotated = !isRotated;
          layoutNeedsReset = true;
          renderer.ClearCache();
        }
        if (input.SelectPressed()) {
          currentState = STATE_LIBRARY;
          renderer.SetFontMode(FontMode::SMART);
          renderer.LoadFont(1.0f);
          renderer.ClearCache();
        }
        if (input.TrianglePressed()) {
          showChapterMenu = true;
          menuSelection = (currentChapter < 0) ? 0 : currentChapter;
          menuScroll = std::max(0, menuSelection - 3);
        }

        if (input.UpPressed()) {
          readerFontScale = std::min(3.0f, readerFontScale + 0.1f);
          renderer.LoadFont(readerFontScale);
          layoutNeedsReset = true;
        }
        if (input.DownPressed()) {
          readerFontScale = std::max(0.4f, readerFontScale - 0.1f);
          renderer.LoadFont(readerFontScale);
          layoutNeedsReset = true;
        }
      }

      if (layoutNeedsReset && currentChapter >= 0) {
        resetLayout(currentChapter, reader, htmlExtractor);
        processLayout(reader, renderer, 500); // Instant first page
      }

      // --- READER RENDER ---
      SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
      SDL_RenderClear(sdlRenderer);

      auto drawUI = [&](const char *text, int tx, int ty, uint32_t color,
                        TextStyle style = TextStyle::NORMAL) {
        if (isRotated)
          renderer.RenderText(text, SCREEN_WIDTH - ty, tx, color, style, 90.0f);
        else
          renderer.RenderText(text, tx, ty, color, style, 0.0f);
      };

      if (currentChapter == -1) {
        if (isRotated) {
          renderer.RenderTextCentered(meta.author, 160, 0xFFFFFFFF,
                                      TextStyle::H2, 90.0f);
          renderer.RenderTextCentered(meta.author, 160, 0xFFFFFFFF,
                                      TextStyle::H2, 90.0f);

          int titleW = renderer.MeasureTextWidth(meta.title, TextStyle::TITLE);
          if (titleW > 260) {
            std::string t1 = meta.title;
            std::string t2 = "";
            size_t mid = t1.length() / 2;
            size_t split = t1.find(' ', mid); // Try forward first
            if (split == std::string::npos)
              split = t1.find_last_of(' ', mid); // Then backward

            if (split != std::string::npos) {
              t2 = t1.substr(split + 1);
              t1 = t1.substr(0, split);
              renderer.RenderTextCentered(t1.c_str(), 200, 0xFFFFFFFF,
                                          TextStyle::TITLE, 90.0f);
              renderer.RenderTextCentered(t2.c_str(), 240, 0xFFFFFFFF,
                                          TextStyle::TITLE, 90.0f);
            } else {
              // No spaces, force scaling down? Or just render as is (fallback)
              renderer.RenderTextCentered(meta.title, 200, 0xFFFFFFFF,
                                          TextStyle::TITLE, 90.0f);
            }
          } else {
            renderer.RenderTextCentered(meta.title, 200, 0xFFFFFFFF,
                                        TextStyle::TITLE, 90.0f);
          }
        } else {
          renderer.RenderTextCentered(meta.author, 80, 0xFFFFFFFF,
                                      TextStyle::H2, 0.0f);
          renderer.RenderTextCentered(meta.title, 120, 0xFFFFFFFF,
                                      TextStyle::TITLE, 0.0f);
        }
      } else {
        const char *headerTitle = meta.spine[currentChapter].title;
        if (isRotated)
          renderer.RenderTextCentered(headerTitle, 10, 0xFF888888,
                                      TextStyle::SMALL, 90.0f);
        else
          renderer.RenderTextCentered(headerTitle, 10, 0xFF888888,
                                      TextStyle::SMALL, 0.0f);

        int stepY = (int)(LAYOUT_LINE_SPACE * readerFontScale);
        for (int i = 0; i < linesPerPage && (currentLine + i) < totalLines;
             i++) {
          TextStyle s = chapterLines[currentLine + i].style;
          const char *txt = chapterLines[currentLine + i].text;
          if (!txt || txt[0] == '\0')
            continue;
          if (s == TextStyle::NORMAL) {
            if (isRotated)
              renderer.RenderText(txt,
                                  SCREEN_WIDTH - (LAYOUT_START_Y + i * stepY),
                                  LAYOUT_MARGIN, 0xFFFFFFFF, s, 90.0f);
            else
              renderer.RenderText(txt, LAYOUT_MARGIN,
                                  LAYOUT_START_Y + i * stepY, 0xFFFFFFFF, s,
                                  0.0f);
          } else {
            if (isRotated)
              renderer.RenderTextCentered(txt, (LAYOUT_START_Y + i * stepY),
                                          0xFFFFFFFF, s, 90.0f);
            else
              renderer.RenderTextCentered(txt, LAYOUT_START_Y + i * stepY,
                                          0xFFFFFFFF, s, 0.0f);
          }
        }
      }

      if (showChapterMenu) {
        SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 230);
        SDL_Rect overlay = {0, 0, 480, 272};
        SDL_RenderFillRect(sdlRenderer, &overlay);
        int menuX = isRotated ? 10 : 40;
        int menuY = isRotated ? 40 : 40;
        int menuWidth = isRotated ? 250 : 400;
        int visibleItems = isRotated ? 22 : 12;
        for (int i = 0;
             i < visibleItems && (menuScroll + i) < (int)meta.spine.size();
             i++) {
          int idx = menuScroll + i;
          std::string title = meta.spine[idx].title;
          uint32_t color = (idx == menuSelection) ? 0xFFFFFFFF : 0xFF888888;
          // Loop Body
          if (idx == menuSelection) {
            float pulse = (sinf(frameCount * 0.15f) + 1.0f) * 0.5f;
            SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255,
                                   20 + (int)(pulse * 30));
            if (isRotated) {
              // Visual Rec for selection
              // X (visual) = menuX
              // Y (visual) = menuY + i*18
              // W (visual) = menuWidth
              // H (visual) = 18

              // Rotated Rect:
              // Physical X = 480 - (menuY + i*18) - 18 (height becomes width)
              // => 480 - visualY - 18 Physical Y = menuX (indent) Physical W =
              // 18 (height) Physical H = menuWidth (width)

              int visualY = menuY + i * 18;
              SDL_Rect selRect = {480 - visualY - 18 - 4, menuX - 5, 24,
                                  menuWidth + 10};
              SDL_RenderFillRect(sdlRenderer, &selRect);
            } else {
              SDL_Rect selRect = {menuX - 5, menuY + i * 18, menuWidth + 10,
                                  18};
              SDL_RenderFillRect(sdlRenderer, &selRect);
            }
          }
          if (isRotated) {
            int visualItemY = menuY + i * 18;
            // For text scrolling on selected item
            int textX = visualItemY;
            int textY = menuX;

            if (idx == menuSelection) {
              // Measure and scroll
              int w =
                  renderer.MeasureTextWidth(title.c_str(), TextStyle::NORMAL);
              int maxVisible = menuWidth;
              if (w > maxVisible) {
                int scrollSpeed = 2; // px per frame
                int scrollPeriod =
                    w + 100; // total scroll distance including pause
                int offset =
                    (frameCount * 2) %
                    scrollPeriod; // Fixed scroll, or use shift logic below

                int shift = (frameCount % (w + 50));
                int effectiveX = menuX - shift;

                SDL_Rect pClip;
                pClip.x = 480 - (menuY + visibleItems * 18);
                pClip.y = menuX;
                pClip.w =
                    visibleItems * 18; // Logic might be rough but safe enough
                pClip.h = menuWidth;

                SDL_RenderSetClipRect(sdlRenderer, &pClip);

                renderer.RenderText(title.c_str(), 480 - visualItemY,
                                    menuX - shift, color, TextStyle::NORMAL,
                                    90.0f);

                if (shift > 0) {
                  renderer.RenderText(title.c_str(), 480 - visualItemY,
                                      menuX - shift + w + 50, color,
                                      TextStyle::NORMAL, 90.0f);
                }

                SDL_RenderSetClipRect(sdlRenderer, NULL);
              } else {
                renderer.RenderText(title.c_str(), 480 - visualItemY, menuX,
                                    color, TextStyle::NORMAL, 90.0f);
              }
            } else {
              renderer.RenderText(title.c_str(), 480 - visualItemY, menuX,
                                  color, TextStyle::NORMAL, 90.0f);
            }
          } else {
            // Landscape Mode
            if (idx == menuSelection) {
              int w =
                  renderer.MeasureTextWidth(title.c_str(), TextStyle::NORMAL);
              int maxVisible = menuWidth;
              if (w > maxVisible) {
                int shift = (frameCount % (w + 50));
                SDL_Rect clip = {menuX, menuY + i * 18, menuWidth, 18};
                SDL_RenderSetClipRect(sdlRenderer, &clip);
                renderer.RenderText(title.c_str(), menuX - shift,
                                    menuY + i * 18, color);
                if (shift > 0) {
                  renderer.RenderText(title.c_str(), menuX - shift + w + 50,
                                      menuY + i * 18, color);
                }
                SDL_RenderSetClipRect(sdlRenderer, NULL);
              } else {
                renderer.RenderText(title.c_str(), menuX, menuY + i * 18,
                                    color);
              }
            } else {
              renderer.RenderText(title.c_str(), menuX, menuY + i * 18, color);
            }
          }
        }
      }
    }

    SDL_RenderPresent(sdlRenderer);
    // Sync to VBlank for smooth 60fps on real hardware
    sceDisplayWaitVblankStart();
    SDL_Delay(16);
  }

  renderer.Shutdown();
  reader.Close();
  if (joy)
    SDL_JoystickClose(joy);
  SDL_Quit();
  sceKernelExitGame();
  return 0;
}
