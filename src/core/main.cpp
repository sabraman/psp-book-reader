#include "cover_renderer.h"
#include "debug_logger.h"
#include "epub_reader.h"
#include "html_text_extractor.h"
#include "input_handler.h"
#include "text_renderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
static float fontScale = 1.0f;
static bool isRotated = false;
static bool showChapterMenu = false;
static bool showStatusOverlay = false;
static int linesPerPage = 10;
static int menuSelection = 0;
static int menuScroll = 0;

static char *words[MAX_WORDS];
static TextStyle wordStyles[MAX_WORDS];
static char wordBuffer[WORD_BUFFER_SIZE];

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

void updateLayout(HtmlTextExtractor &extractor, EpubReader &reader,
                  TextRenderer &renderer, int chapterIndex) {
  if (chapterIndex < 0)
    return;
  DebugLogger::Log("Updating Layout: Ch %d, Rot: %d", chapterIndex, isRotated);
  uint8_t *raw_data = reader.LoadChapter(chapterIndex);
  if (!raw_data)
    return;

  int wordCount =
      extractor.ExtractWords((char *)raw_data, words, wordStyles, MAX_WORDS,
                             wordBuffer, WORD_BUFFER_SIZE);
  free(raw_data);

  totalLines = 0;
  memset(chapterLines, 0, sizeof(chapterLines));
  int maxWidth = isRotated ? (SCREEN_HEIGHT - 2 * LAYOUT_MARGIN)
                           : (SCREEN_WIDTH - 2 * LAYOUT_MARGIN);
  std::string currentLineStr = "";
  TextStyle currentLineStyle = TextStyle::NORMAL;
  const EpubMetadata &meta = reader.GetMetadata();

  auto pushLine = [&](const std::string &line, TextStyle style) {
    if (totalLines < MAX_CHAPTER_LINES) {
      if (line.empty() || (line.length() == 1 && line[0] == ' '))
        return;

      // Aggressive filtering of redundant metadata at the start of chapters
      if (totalLines < 20) {
        if (isRedundantMetadata(line.c_str(), meta)) {
          return;
        }
      }

      strncpy(chapterLines[totalLines].text, line.c_str(), MAX_LINE_LEN - 1);
      chapterLines[totalLines].text[MAX_LINE_LEN - 1] = '\0';
      chapterLines[totalLines].style = style;
      totalLines++;
    }
  };

  for (int i = 0; i < wordCount; i++) {
    if (words[i][0] == '\n') {
      if (!currentLineStr.empty())
        pushLine(currentLineStr, currentLineStyle);
      currentLineStr = "";
      continue;
    }
    if (wordStyles[i] != currentLineStyle && !currentLineStr.empty()) {
      pushLine(currentLineStr, currentLineStyle);
      currentLineStr = "";
    }
    currentLineStyle = wordStyles[i];
    std::string testLine =
        currentLineStr.empty() ? words[i] : (currentLineStr + " " + words[i]);
    int width = renderer.MeasureTextWidth(testLine.c_str(), currentLineStyle);
    if (width > maxWidth && !currentLineStr.empty()) {
      pushLine(currentLineStr, currentLineStyle);
      currentLineStr = words[i];
    } else {
      currentLineStr = testLine;
    }
    if (totalLines >= MAX_CHAPTER_LINES)
      break;
  }
  if (!currentLineStr.empty())
    pushLine(currentLineStr, currentLineStyle);

  // Smart skip for technical/dummy chapters
  if (chapterIndex <= 1 && totalLines < 5) {
    bool allMeta = true;
    for (int i = 0; i < totalLines; i++)
      if (!isRedundantMetadata(chapterLines[i].text, meta))
        allMeta = false;
    if (allMeta) {
      DebugLogger::Log("Auto-skipping noise chapter: %d", chapterIndex);
      totalLines = 0;
    }
  }

  int availableHeight =
      (isRotated ? SCREEN_WIDTH : SCREEN_HEIGHT) - LAYOUT_START_Y - 40;
  linesPerPage = availableHeight / (int)(LAYOUT_LINE_SPACE * fontScale);
  if (linesPerPage < 1)
    linesPerPage = 1;

  DebugLogger::Log("Layout Updated: %d lines", totalLines);
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

  EpubReader reader;
  reader.Open("epub-with-cyrillic.epub");

  CoverRenderer::ShowCover(sdlRenderer, reader);

  SDL_Event event;
  while (SDL_PollEvent(&event))
    ;

  TextRenderer renderer;
  renderer.Initialize(sdlRenderer);
  renderer.LoadFont(fontScale);

  const EpubMetadata &meta = reader.GetMetadata();
  HtmlTextExtractor htmlExtractor;
  int currentChapter = -1;

  updateLayout(htmlExtractor, reader, renderer, 0);

  InputHandler input;
  running = 1;
  uint32_t frameCount = 0;

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

    bool layoutNeedsUpdate = false;
    if (showChapterMenu) {
      int visibleMax = isRotated ? 15 : 10;
      if (input.UpPressed()) {
        menuSelection = std::max(0, menuSelection - 1);
        if (menuSelection < menuScroll)
          menuScroll = menuSelection;
      }
      if (input.DownPressed()) {
        menuSelection = std::min((int)meta.spine.size() - 1, menuSelection + 1);
        if (menuSelection >= menuScroll + visibleMax)
          menuScroll = menuSelection - visibleMax + 1;
      }
      if (input.CrossPressed()) {
        currentChapter = menuSelection;
        layoutNeedsUpdate = true;
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
          updateLayout(htmlExtractor, reader, renderer, 0);
          if (totalLines == 0) {
            currentChapter = 1;
            layoutNeedsUpdate = true;
          }
          currentLine = 0;
          renderer.ClearCache();
        } else if (currentLine + linesPerPage < totalLines) {
          currentLine += linesPerPage;
          renderer.ClearCache();
        } else if (currentChapter < (int)meta.spine.size() - 1) {
          currentChapter++;
          layoutNeedsUpdate = true;
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
          updateLayout(htmlExtractor, reader, renderer, currentChapter);
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
        layoutNeedsUpdate = true;
        renderer.ClearCache();
      }
      if (input.SelectPressed())
        showStatusOverlay = !showStatusOverlay;
      if (input.TrianglePressed()) {
        showChapterMenu = true;
        menuSelection = (currentChapter < 0) ? 0 : currentChapter;
        menuScroll = std::max(0, menuSelection - 3);
      }

      if (input.UpPressed()) {
        fontScale = std::min(3.0f, fontScale + 0.1f);
        renderer.LoadFont(fontScale);
        layoutNeedsUpdate = true;
      }
      if (input.DownPressed()) {
        fontScale = std::max(0.4f, fontScale - 0.1f);
        renderer.LoadFont(fontScale);
        layoutNeedsUpdate = true;
      }
    }

    if (layoutNeedsUpdate && currentChapter >= 0)
      updateLayout(htmlExtractor, reader, renderer, currentChapter);

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
        renderer.RenderTextCentered(meta.author, 160, 0xFFFFFFFF, TextStyle::H2,
                                    90.0f);
        renderer.RenderTextCentered(meta.title, 200, 0xFFFFFFFF,
                                    TextStyle::TITLE, 90.0f);
      } else {
        renderer.RenderTextCentered(meta.author, 80, 0xFFFFFFFF, TextStyle::H2,
                                    0.0f);
        renderer.RenderTextCentered(meta.title, 120, 0xFFFFFFFF,
                                    TextStyle::TITLE, 0.0f);
      }
      if (isRotated)
        renderer.RenderTextCentered("1", 450, 0xFF888888, TextStyle::SMALL,
                                    90.0f);
      else
        renderer.RenderTextCentered("1", 250, 0xFF888888, TextStyle::SMALL,
                                    0.0f);
    } else {
      // Show specific chapter title in the header
      const char *headerTitle = meta.spine[currentChapter].title;
      if (isRotated)
        renderer.RenderTextCentered(headerTitle, 10, 0xFF888888,
                                    TextStyle::SMALL, 90.0f);
      else
        renderer.RenderTextCentered(headerTitle, 10, 0xFF888888,
                                    TextStyle::SMALL, 0.0f);

      int stepY = (int)(LAYOUT_LINE_SPACE * fontScale);
      for (int i = 0; i < linesPerPage && (currentLine + i) < totalLines; i++) {
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
            renderer.RenderText(txt, LAYOUT_MARGIN, LAYOUT_START_Y + i * stepY,
                                0xFFFFFFFF, s, 0.0f);
        } else {
          if (isRotated)
            renderer.RenderTextCentered(txt, (LAYOUT_START_Y + i * stepY),
                                        0xFFFFFFFF, s, 90.0f);
          else
            renderer.RenderTextCentered(txt, LAYOUT_START_Y + i * stepY,
                                        0xFFFFFFFF, s, 0.0f);
        }
      }

      char pbuf[32];
      snprintf(pbuf, 32, "%d", (currentLine / linesPerPage) + 2);
      if (isRotated)
        renderer.RenderTextCentered(pbuf, 450, 0xFF888888, TextStyle::SMALL,
                                    90.0f);
      else
        renderer.RenderTextCentered(pbuf, 250, 0xFF888888, TextStyle::SMALL,
                                    0.0f);
    }

    if (showStatusOverlay) {
      ScePspDateTime ptime;
      sceRtcGetCurrentClockLocalTime(&ptime);
      int batt = scePowerGetBatteryLifePercent();
      char status[128];
      snprintf(status, 128, "%02d:%02d | Bat: %d%%", ptime.hour, ptime.minute,
               batt);
      drawUI(status, 16, isRotated ? 430 : 230, 0xFF00FF00, TextStyle::SMALL);
    }

    if (showChapterMenu) {
      SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 230);
      SDL_Rect overlay = {0, 0, 480, 272};
      SDL_RenderFillRect(sdlRenderer, &overlay);

      int menuX = isRotated ? 100 : 40;
      int menuY = isRotated ? 40 : 40;
      int menuWidth = isRotated ? 272 : 400;
      int visibleMax = isRotated ? 15 : 12;

      for (int i = 0;
           i < visibleMax && (menuScroll + i) < (int)meta.spine.size(); i++) {
        int idx = menuScroll + i;
        uint32_t color = (idx == menuSelection) ? 0xFFFFFFFF : 0xFF888888;

        if (idx == menuSelection) {
          float pulse = (sinf(frameCount * 0.15f) + 1.0f) * 0.5f;
          uint8_t alpha = 80 + (uint8_t)(pulse * 175);
          SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, alpha / 8);
          SDL_Rect selRect = {menuX - 10, menuY + i * 18 - 2, menuWidth + 20,
                              18};
          if (isRotated)
            selRect = {SCREEN_WIDTH - (menuY + i * 18 + 14), menuX - 10, 18,
                       menuWidth + 20};
          SDL_RenderFillRect(sdlRenderer, &selRect);
        }

        const char *title = meta.spine[idx].title;
        char trimTitle[64];
        strncpy(trimTitle, title, 63);
        trimTitle[63] = '\0';

        if (isRotated)
          renderer.RenderText(trimTitle, SCREEN_WIDTH - (menuY + i * 18), menuX,
                              color, TextStyle::NORMAL, 90.0f);
        else
          renderer.RenderText(trimTitle, menuX, menuY + i * 18, color,
                              TextStyle::NORMAL, 0.0f);
      }

      if (menuScroll > 0)
        drawUI("^", menuX + menuWidth / 2, menuY - 15, 0xFFFFFFFF);
      if (menuScroll + visibleMax < (int)meta.spine.size())
        drawUI("v", menuX + menuWidth / 2, menuY + visibleMax * 18, 0xFFFFFFFF);
    }

    SDL_RenderPresent(sdlRenderer);
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
