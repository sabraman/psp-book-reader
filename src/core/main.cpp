#include "cover_renderer.h"
#include "debug_logger.h"
#include "epub_reader.h"
#include "html_text_extractor.h"
#include "input_handler.h"
#include "library_manager.h"
#include "power_utils.h"
#include "settings_manager.h"
#include "text_renderer.h"
#include <SDL2/SDL.h>

static uint32_t lastInputTicks = 0;
static PowerMode currentPowerMode = POWER_MODE_BALANCED;
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

struct LineInfo {
  char text[MAX_LINE_LEN];
  TextStyle style;
  int startWordIdx;  // Used for anchor tracking during reflow
  uint64_t cacheKey; // Pre-calculated render key
};

static int layoutMargin = 24;
static int layoutStartY = 45;

static LineInfo chapterLines[MAX_CHAPTER_LINES];
static int totalLines = 0;
static int currentLine = 0;
static float readerFontScale = 1.0f;
static bool isRotated = false;
static bool showChapterMenu = false;
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
  int targetWordIdx = -1; // Resume at this word after reflow
  int anchorWordIdx = 0;  // First word of current page
} layoutState;

static std::vector<int> pageAnchors; // wordIndex for start of each page
static int currentPageIdx = 0;
static bool showStatusOverlay = false;
static CoverRenderer coverRenderer;

static char *words[MAX_WORDS];
static int wordLens[MAX_WORDS];
static TextStyle wordStyles[MAX_WORDS];
static int wordWidths[MAX_WORDS]; // Cached widths for O(N) layout
static char wordBuffer[WORD_BUFFER_SIZE];
static int cachedSpaceWidths[6]; // Cache space width per style
static bool spaceWidthsDirty = true;

enum AppState { STATE_LIBRARY, STATE_READER, STATE_SETTINGS };
static AppState currentState = STATE_LIBRARY;
static AppState previousState = STATE_LIBRARY; // To return from settings

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

struct MetadataCheck {
  bool isRedundant[15];
  int checkedCount;
} metadataCheck = {{false}, 0};

bool isRedundantMetadata(const char *text, const EpubMetadata &meta) {
  if (!text || text[0] == '\0')
    return false;
  if (findStringInsensitive(text, meta.title))
    return true;
  if (findStringInsensitive(text, meta.author))
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
  memset(words, 0, sizeof(words));
  memset(wordStyles, 0, sizeof(wordStyles));
  memset(wordLens, 0, sizeof(wordLens));

  extractor.ExtractWords((char *)raw_data, words, wordStyles, wordLens,
                         MAX_WORDS, wordBuffer, WORD_BUFFER_SIZE);
  free(raw_data);

  for (int i = 0; i < MAX_WORDS; i++) {
    wordWidths[i] = -1; // -1 means unmeasured
  }
  memset(wordWidths, -1, sizeof(wordWidths)); // Double ensure

  layoutState.chapterIndex = chapterIndex;
  layoutState.wordIdx = 0;
  layoutState.lineCount = 0;
  layoutState.complete = false;
  layoutState.needsReset = false;
  layoutState.targetWordIdx = -1;
  layoutState.anchorWordIdx = 0;

  totalLines = 0;
  currentLine = 0;
  currentPageIdx = 0;
  pageAnchors.clear();
  pageAnchors.reserve(512); // Pre-allocate to prevent heap churn
  pageAnchors.push_back(0); // Page 1 starts at word 0

  metadataCheck.checkedCount = 0;
  memset(metadataCheck.isRedundant, 0, sizeof(metadataCheck.isRedundant));

  DebugLogger::Log("Layout Reset for Ch %d", chapterIndex);
}

void reflowLayout() {
  if (layoutState.chapterIndex < 0)
    return;

  // Remember current position
  if (currentLine >= 0 && currentLine < totalLines) {
    layoutState.targetWordIdx = chapterLines[currentLine].startWordIdx;
  } else if (currentPageIdx >= 0 && currentPageIdx < (int)pageAnchors.size()) {
    layoutState.targetWordIdx = pageAnchors[currentPageIdx];
  } else {
    layoutState.targetWordIdx = 0;
  }

  memset(wordWidths, -1, sizeof(wordWidths));
  spaceWidthsDirty = true;

  layoutState.wordIdx = 0;
  layoutState.lineCount = 0;
  layoutState.complete = false;
  layoutState.needsReset = false;
  totalLines = 0;
  currentLine = 0;
  currentPageIdx = 0;
  pageAnchors.clear();
  pageAnchors.push_back(0);

  DebugLogger::Log("Reflow started: targetWord=%d", layoutState.targetWordIdx);
}

bool processLayout(EpubReader &reader, TextRenderer &renderer,
                   int maxWords = 200) {
  if (layoutState.complete || layoutState.chapterIndex < 0)
    return true;

  int maxWidth = isRotated ? (SCREEN_HEIGHT - 2 * layoutMargin)
                           : (SCREEN_WIDTH - 2 * layoutMargin);
  int availableHeight =
      (isRotated ? SCREEN_WIDTH : SCREEN_HEIGHT) - layoutStartY - 25;
  int baseHeight = renderer.GetLineHeight(TextStyle::NORMAL);

  float spacingMult = 1.35f;
  switch (SettingsManager::Get().GetSettings().spacing) {
  case SpacingPreset::TIGHT:
    spacingMult = 1.15f;
    break;
  case SpacingPreset::NORMAL:
    spacingMult = 1.35f;
    break;
  case SpacingPreset::LOOSE:
    spacingMult = 1.6f;
    break;
  }

  int stepY = (int)(baseHeight * spacingMult);
  linesPerPage = availableHeight / stepY;
  if (linesPerPage < 1)
    linesPerPage = 1;

  const EpubMetadata &meta = reader.GetMetadata();
  int wordsProcessed = 0;

  // Pre-cache space widths for common styles if needed
  if (spaceWidthsDirty) {
    for (int i = 0; i < 6; i++) {
      cachedSpaceWidths[i] = renderer.MeasureTextWidth(" ", (TextStyle)i);
    }
    spaceWidthsDirty = false;
  }

  while (layoutState.wordIdx < MAX_WORDS &&
         words[layoutState.wordIdx] != nullptr && wordsProcessed < maxWords) {
    if (words[layoutState.wordIdx][0] == '\n') {
      layoutState.wordIdx++;
      wordsProcessed++;
      continue;
    }

    int currentLineWidth = 0;
    int lineStartWordIdx = layoutState.wordIdx;
    TextStyle currentLineStyle = wordStyles[layoutState.wordIdx];

    while (layoutState.wordIdx < MAX_WORDS &&
           words[layoutState.wordIdx] != nullptr) {
      if (words[layoutState.wordIdx][0] == '\n')
        break;

      // O(N) Layout: Use cached word widths
      if (wordWidths[layoutState.wordIdx] == -1) {
        wordWidths[layoutState.wordIdx] = renderer.MeasureTextWidth(
            words[layoutState.wordIdx], wordStyles[layoutState.wordIdx]);
      }

      int wordW = wordWidths[layoutState.wordIdx];
      int spaceW =
          (currentLineWidth == 0)
              ? 0
              : cachedSpaceWidths[(int)wordStyles[layoutState.wordIdx]];

      if (currentLineWidth + spaceW + wordW > maxWidth && currentLineWidth > 0)
        break;

      currentLineWidth += spaceW + wordW;
      layoutState.wordIdx++;
      wordsProcessed++;
      if (wordsProcessed >= maxWords)
        break;
    }

    if (layoutState.wordIdx > lineStartWordIdx &&
        totalLines < MAX_CHAPTER_LINES) {
      // Reconstruct line string only once per line
      char *linePtr = chapterLines[totalLines].text;
      int lineLen = 0;
      for (int i = lineStartWordIdx; i < layoutState.wordIdx; i++) {
        int wlen = wordLens[i];
        if (lineLen + wlen + 2 < MAX_LINE_LEN) {
          if (i > lineStartWordIdx) {
            linePtr[lineLen++] = ' ';
          }
          if (words[i]) {
            memcpy(linePtr + lineLen, words[i], wlen);
            lineLen += wlen;
          }
        }
      }
      linePtr[lineLen] = '\0';

      bool redundant = false;
      if (totalLines < 15) {
        redundant = isRedundantMetadata(chapterLines[totalLines].text, meta);
        metadataCheck.isRedundant[totalLines] = redundant;
        metadataCheck.checkedCount = totalLines + 1;
      }

      if (redundant) {
        // Skip metadata noise
      } else {
        chapterLines[totalLines].style = currentLineStyle;
        chapterLines[totalLines].startWordIdx = lineStartWordIdx;
        chapterLines[totalLines].cacheKey =
            renderer.GetCacheKey(linePtr, currentLineStyle);

        // Position Recovery Logic
        if (layoutState.targetWordIdx >= 0 &&
            lineStartWordIdx <= layoutState.targetWordIdx &&
            layoutState.wordIdx > layoutState.targetWordIdx) {
          currentLine = totalLines;
          currentPageIdx = pageAnchors.size() - 1;
          layoutState.targetWordIdx = -1; // Position focused
        }

        totalLines++;
        layoutState.lineCount++;

        // Pagination Tracking
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
  }

  return layoutState.complete;
}

int main(int argc, char *argv[]) {
  printf("PSP-BookReader: main() starting...\n");
  DebugLogger::Init();
  DebugLogger::Log("App starting...");
  SettingsManager::Get().Load();
  DebugLogger::Log("Settings Loaded");

  AppSettings &settings = SettingsManager::Get().GetSettings();
  readerFontScale = settings.fontScale;
  showStatusOverlay = settings.showStatus;
  DebugLogger::Log("Font scale: %.1f, Themes: %d", readerFontScale,
                   (int)settings.theme);

  scePowerSetClockFrequency(333, 333, 166);
  printf("Initializing SDL...\n");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) <
      0) {
    printf("SDL_Init FAILED: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Joystick *joy = nullptr;
  if (SDL_NumJoysticks() > 0) {
    printf("Opening Joystick 0...\n");
    joy = SDL_JoystickOpen(0);
  }

  SDL_Window *window =
      SDL_CreateWindow("PSP-BookReader", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 480, 272, 0);
  printf("Creating Renderer...\n");
  SDL_Renderer *sdlRenderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  SDL_Event event;

  printf("Initializing TextRenderer...\n");
  TextRenderer renderer;
  renderer.Initialize(sdlRenderer);
  printf("Loading fonts...\n");
  if (!renderer.LoadFont(1.0f)) {
    printf("CRITICAL: Failed to load fonts!\n");
  }

  LibraryManager library;
  printf("Library Object Initialized (Deferred Scan)\n");

  EpubReader reader;
  HtmlTextExtractor htmlExtractor;
  int currentChapter = -1;

  InputHandler input;
  running = 1;

  int libSelection = 0;
  uint32_t frameCount = 0;
  bool isScanning = true;
  DebugLogger::Log("Entering main loop");

  int settingsSelection = 0;

  while (running) {
    frameCount++;
    input.Update();

    // --- Power Management Logic ---
    PowerMode targetMode = POWER_MODE_BALANCED;
    bool isIdle = (SDL_GetTicks() - lastInputTicks > 2000);

    if (isScanning) {
      targetMode = POWER_MODE_PERFORMANCE;
    } else if (isIdle) {
      targetMode = POWER_MODE_SAVING;
    }

    if (targetMode != currentPowerMode) {
      SetPowerMode(targetMode);
      currentPowerMode = targetMode;
      DebugLogger::Log("PowerMode changed: %d", (int)targetMode);
    }

    // --- Frame Throttling ---
    if (currentPowerMode == POWER_MODE_SAVING) {
      SDL_Delay(32); // ~30 FPS or less wakeups
    } else {
      SDL_Delay(1); // Standard yielding
    }
    const auto &books = library.GetBooks();
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = 0;
      input.ProcessEvent(event);
      lastInputTicks = SDL_GetTicks(); // Activity detected
    }

    if (input.HasActiveInput()) {
      lastInputTicks = SDL_GetTicks(); // Continuous activity
    }
    if (input.StartPressed()) {
      if (currentState == STATE_READER) {
        // Return to Library
        if (currentChapter >= 0 && !books.empty() && libSelection >= 0 &&
            libSelection < (int)books.size()) {
          int wordIdx = 0;
          if (currentLine >= 0 && currentLine < totalLines) {
            wordIdx = chapterLines[currentLine].startWordIdx;
          }
          SettingsManager::Get().SaveProgress(
              books[libSelection].filename.c_str(), currentChapter, wordIdx);
        }
        currentState = STATE_LIBRARY;
        renderer.SetFontMode(FontMode::SMART);
        renderer.LoadFont(1.0f);
        spaceWidthsDirty = true;
        renderer.ClearCache();
      } else if (currentState == STATE_SETTINGS) {
        currentState = previousState;
        SettingsManager::Get().Save();
      }
    }

    if (input.SelectPressed()) {
      if (currentState == STATE_READER) {
        DebugLogger::Log("Input: SELECT pressed in READER -> SETTINGS");
        previousState = STATE_READER;
        currentState = STATE_SETTINGS;
        settingsSelection = 0;
      } else if (currentState == STATE_SETTINGS) {
        DebugLogger::Log("Input: SELECT pressed in SETTINGS -> Return");
        currentState = previousState;
        SettingsManager::Get().Save();
      }
    }

    if (input.Exit()) {
      // Handled
    }

    const ThemeColors &themeColors = renderer.GetThemeColors();

    if (currentState == STATE_LIBRARY) {
      if (isScanning) {
        SDL_SetRenderDrawColor(sdlRenderer, 15, 15, 20, 255);
        SDL_RenderClear(sdlRenderer);
        renderer.RenderTextCentered("SCANNING LIBRARY...", 120, 0xFFFFFFFF,
                                    TextStyle::H2);
        SDL_RenderPresent(sdlRenderer);
        library.ScanDirectory("books");
        isScanning = false;
        DebugLogger::Log("Library scanned. Count: %d",
                         (int)library.GetBooks().size());
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
            renderer.SetTheme(SettingsManager::Get().GetSettings().theme);

            // Resume Logic
            const BookProgress &prog = SettingsManager::Get().GetProgress();
            if (strcmp(prog.path, books[libSelection].filename.c_str()) == 0) {
              currentChapter = prog.chapterIndex;
              if (currentChapter >= 0) {
                resetLayout(currentChapter, reader, htmlExtractor);
                layoutState.targetWordIdx = prog.wordIndex;
                processLayout(reader, renderer, 1000);
              }
            }
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
            // Show cover for 2 seconds
            coverRenderer.ShowCover(sdlRenderer, reader, 2000);
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

      // Library Status Header
      ScePspDateTime pspTime;
      sceRtcGetCurrentClockLocalTime(&pspTime);
      int battery = scePowerGetBatteryLifePercent();
      char statusBuf[64];
      snprintf(statusBuf, sizeof(statusBuf), "%02d:%02d  |  %d%%", pspTime.hour,
               pspTime.minute, battery);
      renderer.RenderText(statusBuf, 40, 20, 0xFF888888, TextStyle::SMALL);

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
    } else if (currentState == STATE_READER) {
      // --- READER LOGIC ---
      const EpubMetadata &meta = reader.GetMetadata();
      // Background layout processing
      if (!layoutState.complete) {
        processLayout(reader, renderer,
                      500); // Throttled to 500 words for better frame timing
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
          showChapterMenu = false;
        }
        if (input.TrianglePressed())
          showChapterMenu = false;

        // Return to Library with SELECT+START or similar?
        // User wants START for settings. Let's use TRIANGLE in Library or just
        // specific back button. For now, let's add a "Back to Library" option
        // in settings.
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
              currentPageIdx++;
            } else {
              // Speed up layout if user is waiting
              processLayout(reader, renderer, 1000);
            }
          } else if (currentChapter < (int)meta.spine.size() - 1) {
            currentChapter++;
            layoutNeedsReset = true;
            currentLine = 0;
          }
        }
        if (input.PrevPage()) {
          if (currentLine >= linesPerPage) {
            currentLine -= linesPerPage;
            if (currentPageIdx > 0)
              currentPageIdx--;
          } else if (currentChapter > 0) {
            currentChapter--;
            resetLayout(currentChapter, reader, htmlExtractor);
            processLayout(reader, renderer, 10000);
            if (currentChapter == 0 && totalLines == 0) {
              currentChapter = -1;
            } else {
              currentLine = ((totalLines - 1) / linesPerPage) * linesPerPage;
              if (currentLine < 0)
                currentLine = 0;
            }
          } else if (currentChapter == 0) {
            currentChapter = -1;
          }
        }
        if (input.CirclePressed()) {
          isRotated = !isRotated;
          reflowLayout();
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
          reflowLayout();
        }
        if (input.DownPressed()) {
          readerFontScale = std::max(0.4f, readerFontScale - 0.1f);
          renderer.LoadFont(readerFontScale);
          reflowLayout();
        }
      } // End if(!showChapterMenu)

      if (layoutNeedsReset && currentChapter >= 0) {
        resetLayout(currentChapter, reader, htmlExtractor);
        processLayout(reader, renderer, 500); // Instant first page
        layoutNeedsReset = false;
      }

      // --- READER RENDER ---
      SDL_SetRenderDrawColor(sdlRenderer, (themeColors.background >> 0) & 0xFF,
                             (themeColors.background >> 8) & 0xFF,
                             (themeColors.background >> 16) & 0xFF, 255);
      SDL_RenderClear(sdlRenderer);

      if (currentChapter == -1) {
        if (isRotated) {
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
          int authorY = isRotated ? 60 : 80;
          int titleY = authorY + (int)(40 * readerFontScale);
          renderer.RenderTextCentered(meta.author, authorY, 0xFFFFFFFF,
                                      TextStyle::H2, 0.0f);
          renderer.RenderTextCentered(meta.title, titleY, 0xFFFFFFFF,
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

        int baseHeight = renderer.GetLineHeight(TextStyle::NORMAL);
        float spacingMult = 1.35f;
        switch (SettingsManager::Get().GetSettings().spacing) {
        case SpacingPreset::TIGHT:
          spacingMult = 1.15f;
          break;
        case SpacingPreset::NORMAL:
          spacingMult = 1.35f;
          break;
        case SpacingPreset::LOOSE:
          spacingMult = 1.6f;
          break;
        }
        int stepY = (int)(baseHeight * spacingMult);

        for (int i = 0; i < linesPerPage && (currentLine + i) < totalLines;
             i++) {
          const LineInfo &li = chapterLines[currentLine + i];
          TextStyle s = li.style;
          const char *txt = li.text;
          uint64_t key = li.cacheKey;

          if (!txt || txt[0] == '\0')
            continue;
          if (s == TextStyle::NORMAL) {
            if (isRotated)
              renderer.RenderTextWithKey(
                  txt, key, SCREEN_WIDTH - (layoutStartY + i * stepY),
                  layoutMargin, themeColors.text, s, 90.0f);
            else
              renderer.RenderTextWithKey(txt, key, layoutMargin,
                                         layoutStartY + i * stepY,
                                         themeColors.text, s, 0.0f);
          } else {
            if (isRotated)
              renderer.RenderTextCenteredWithKey(txt, key,
                                                 (layoutStartY + i * stepY),
                                                 themeColors.heading, s, 90.0f);
            else
              renderer.RenderTextCenteredWithKey(txt, key,
                                                 layoutStartY + i * stepY,
                                                 themeColors.heading, s, 0.0f);
          }
        }
      }

      // Render Common Reader UI (Overlay & Counter)
      if (showStatusOverlay) {
        ScePspDateTime pspTime;
        sceRtcGetCurrentClockLocalTime(&pspTime);
        int battery = scePowerGetBatteryLifePercent();

        char statusBuf[64];
        snprintf(statusBuf, sizeof(statusBuf), "%02d:%02d  |  %d%%",
                 pspTime.hour, pspTime.minute, battery);

        SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 180);

        if (isRotated) {
          // SDL_Rect bg = {445, 0, 35, 272};
          // SDL_RenderFillRect(sdlRenderer, &bg);
          renderer.RenderText(statusBuf, 448, 10, themeColors.text,
                              TextStyle::SMALL, 90.0f);
        } else {
          // SDL_Rect bg = {0, 0, 480, 25};
          // SDL_RenderFillRect(sdlRenderer, &bg);
          renderer.RenderText(statusBuf, 10, 5, themeColors.text,
                              TextStyle::SMALL, 0.0f);
        }
      }

      if (currentChapter >= 0 && !showChapterMenu) {
        char pageBuf[16];
        snprintf(pageBuf, sizeof(pageBuf), "%d", currentPageIdx + 1);
        if (isRotated) {
          renderer.RenderTextCentered(pageBuf, 455, 0xFF888888,
                                      TextStyle::SMALL, 90.0f);
        } else {
          renderer.RenderTextCentered(pageBuf, 247, 0xFF888888,
                                      TextStyle::SMALL, 0.0f);
        }
      }

      if (showChapterMenu) {
        const EpubMetadata &meta = reader.GetMetadata();
        SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 230);
        SDL_Rect overlay = {0, 0, 480, 272};
        SDL_RenderFillRect(sdlRenderer, &overlay);

        int menuX = isRotated ? 10 : 40;
        int menuY = 40;
        int menuWidth = isRotated ? 250 : 400;
        int visibleItems = isRotated ? 22 : 12;

        for (int i = 0;
             i < visibleItems && (menuScroll + i) < (int)meta.spine.size();
             i++) {
          int idx = menuScroll + i;
          const char *title = meta.spine[idx].title;
          uint32_t color = (idx == menuSelection) ? 0xFFFFFFFF : 0xFF888888;

          if (idx == menuSelection) {
            SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 40);
            if (isRotated) {
              SDL_Rect selRect = {480 - (menuY + i * 18) - 22, menuX - 5, 24,
                                  menuWidth + 10};
              SDL_RenderFillRect(sdlRenderer, &selRect);
            } else {
              SDL_Rect selRect = {menuX - 5, menuY + i * 18, menuWidth + 10,
                                  18};
              SDL_RenderFillRect(sdlRenderer, &selRect);
            }
          }

          int textW = renderer.MeasureTextWidth(title, TextStyle::NORMAL);
          int offset = 0;
          bool clipped = false;

          if (idx == menuSelection && textW > menuWidth) {
            uint32_t ticks = SDL_GetTicks();
            offset = (ticks / 20) % (textW + 60);
            if (offset > textW + 20)
              offset = -20;
            if (offset < 0)
              offset = 0;
            clipped = true;
          }

          if (isRotated) {
            int visualY = menuY + i * 18;
            if (clipped) {
              SDL_Rect clip = {480 - visualY - 22, menuX, 24, menuWidth};
              SDL_RenderSetClipRect(sdlRenderer, &clip);
            }
            renderer.RenderText(title, 480 - visualY, menuX - offset, color,
                                TextStyle::NORMAL, 90.0f);
            if (clipped)
              SDL_RenderSetClipRect(sdlRenderer, NULL);
          } else {
            if (clipped) {
              SDL_Rect clip = {menuX, menuY + i * 18, menuWidth, 20};
              SDL_RenderSetClipRect(sdlRenderer, &clip);
            }
            renderer.RenderText(title, menuX - offset, menuY + i * 18, color,
                                TextStyle::NORMAL, 0.0f);
            if (clipped)
              SDL_RenderSetClipRect(sdlRenderer, NULL);
          }
        }
      }
    } else if (currentState == STATE_SETTINGS) {
      // --- SETTINGS LOGIC ---
      if (input.UpPressed())
        settingsSelection = std::max(0, settingsSelection - 1);
      if (input.DownPressed())
        settingsSelection = std::min(5, settingsSelection + 1);

      if (input.LeftPressed() || input.RightPressed() || input.CrossPressed()) {
        int dir = input.LeftPressed() ? -1 : 1;
        // Cross always advances like Right
        if (input.CrossPressed())
          dir = 1;

        AppSettings &s = SettingsManager::Get().GetSettings();
        switch (settingsSelection) {
        case 0: // Theme
          s.theme = (Theme)(((int)s.theme + dir + 3) % 3);
          renderer.SetTheme(s.theme);
          break;
        case 1: // Font Size
        {
          float f = s.fontScale + (0.2f * dir);
          if (f > 3.0f)
            f = 3.0f;
          if (f < 0.6f)
            f = 0.6f;
          s.fontScale = f;
          readerFontScale = f;
          renderer.LoadFont(readerFontScale);
          reflowLayout();
        } break;
        case 2: // Margins
          s.margin = (MarginPreset)(((int)s.margin + dir + 3) % 3);
          switch (s.margin) {
          case MarginPreset::NARROW:
            layoutMargin = 10;
            break;
          case MarginPreset::NORMAL:
            layoutMargin = 24;
            break;
          case MarginPreset::WIDE:
            layoutMargin = 40;
            break;
          }
          reflowLayout();
          break;
        case 3: // Spacing
          s.spacing = (SpacingPreset)(((int)s.spacing + dir + 3) % 3);
          reflowLayout();
          break;
        case 4: // Status Overlay
          s.showStatus = !s.showStatus;
          showStatusOverlay = s.showStatus;
          break;
        case 5: // Back to Library
          if (input.CrossPressed() || input.CirclePressed() ||
              input.RightPressed()) {
            currentState = STATE_LIBRARY;
            renderer.SetFontMode(FontMode::SMART);
            renderer.LoadFont(1.0f);
            spaceWidthsDirty = true;
            renderer.ClearCache();
          }
          break;
        }
      }

      // --- SETTINGS RENDER ---
      const ThemeColors &tc = renderer.GetThemeColors();
      SDL_SetRenderDrawColor(sdlRenderer, (tc.background >> 0) & 0xFF,
                             (tc.background >> 8) & 0xFF,
                             (tc.background >> 16) & 0xFF, 255);
      SDL_RenderClear(sdlRenderer);

      // renderer.RenderTextCentered("SETTINGS", 20, tc.heading, TextStyle::H1);

      const char *options[] = {"Theme",       "Font Size",
                               "Margins",     "Line Spacing",
                               "Show Status", "Back to Library"};
      char valBuf[64];
      AppSettings &s = SettingsManager::Get().GetSettings();

      for (int i = 0; i < 6; i++) {
        uint32_t color = (i == settingsSelection) ? tc.selection : tc.text;
        renderer.RenderText(options[i], 60, 60 + i * 25, color,
                            TextStyle::NORMAL);

        valBuf[0] = '\0';
        if (i == 0)
          snprintf(valBuf, 64, ": \u25C0 %s \u25BA",
                   s.theme == Theme::NIGHT
                       ? "Night"
                       : (s.theme == Theme::SEPIA ? "Sepia" : "Light"));
        if (i == 1)
          snprintf(valBuf, 64, ": \u25C0 %.1fx \u25BA", s.fontScale);
        if (i == 2)
          snprintf(
              valBuf, 64, ": \u25C0 %s \u25BA",
              s.margin == MarginPreset::NARROW
                  ? "Narrow"
                  : (s.margin == MarginPreset::NORMAL ? "Normal" : "Wide"));
        if (i == 3)
          snprintf(
              valBuf, 64, ": \u25C0 %s \u25BA",
              s.spacing == SpacingPreset::TIGHT
                  ? "Tight"
                  : (s.spacing == SpacingPreset::NORMAL ? "Normal" : "Loose"));
        if (i == 4)
          snprintf(valBuf, 64, ": \u25C0 %s \u25BA",
                   s.showStatus ? "ON" : "OFF");

        if (valBuf[0] != '\0') {
          renderer.RenderText(valBuf, 220, 60 + i * 25, color,
                              TextStyle::NORMAL);
        }
      } // End for loop

      renderer.RenderTextCentered("Press SELECT to return to book", 240,
                                  tc.dimmed, TextStyle::SMALL);
      // End STATE_SETTINGS

    } // End if/else if chain

    // --- COMMON PER-FRAME OUTPUT ---
    // Make sure we always present, regardless of state
    SDL_RenderPresent(sdlRenderer);
    sceDisplayWaitVblankStart();
    SDL_Delay(1);

  } // End while(running)

  DebugLogger::Log("App exiting, shutting down systems...");
  renderer.Shutdown();
  reader.Close();
  SettingsManager::Get().Save();
  if (joy)
    SDL_JoystickClose(joy);
  SDL_Quit();
  sceKernelExitGame();
  return 0;
}
