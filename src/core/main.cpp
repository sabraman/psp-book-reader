#include "debug_logger.h"
#include "epub_reader.h"
#include "html_text_extractor.h"
#include "input_handler.h"
#include "text_renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// PSP Screen Dimensions
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
#define BUFFER_WIDTH 512

// PSP Button Glyphs
#define ICON_CROSS "\x7c"
#define ICON_CIRCLE "\x7d"
#define ICON_TRIANGLE "\x7e"
#define ICON_SQUARE "\x7f"
#define ICON_L "\xEF\x80\x80"
#define ICON_R "\xEF\x80\x81"
#define ICON_SELECT "\xEF\x80\x82"
#define ICON_START "\xEF\x80\x83"
#define ICON_DPAD "\xEF\x80\x84"
#define ICON_UP "\xEF\x80\x85"
#define ICON_DOWN "\xEF\x80\x86"
#define ICON_LEFT "\xEF\x80\x87"
#define ICON_RIGHT "\xEF\x80\x88"

// Reader Constraints
#define MAX_CHAPTER_LINES 5000
#define MAX_WORDS 20000
#define WORD_BUFFER_SIZE 262144
#define MAX_LINE_LEN 256

// Reader Layout Constants
#define LAYOUT_MARGIN 20
#define LAYOUT_START_Y 66
#define LAYOUT_LINE_SPACE 22.0f

static char chapterLines[MAX_CHAPTER_LINES][MAX_LINE_LEN];
static int totalLines = 0;
static int currentLine = 0;
static float fontScale = 1.0f;
static bool isRotated = false;
static bool showChapterMenu = false;
static bool showStatusOverlay = false;
static int linesPerPage = 12;

static char *words[MAX_WORDS];
static char wordBuffer[WORD_BUFFER_SIZE];

PSP_MODULE_INFO("PSP-BookReader", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

unsigned int __attribute__((aligned(16))) list[262144];
int running = 0;

int exit_callback(int arg1, int arg2, void *common) {
  running = 0;
  return 0;
}

int callback_thread(SceSize args, void *argp) {
  int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
  sceKernelRegisterExitCallback(cbid);
  sceKernelSleepThreadCB();
  return 0;
}

int setup_callbacks(void) {
  int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11,
                                   0xFA0, 0, 0);
  if (thid >= 0)
    sceKernelStartThread(thid, 0, 0);
  return thid;
}

void initGu() {
  sceGuInit();
  sceGuStart(GU_DIRECT, list);
  sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUFFER_WIDTH);
  sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, (void *)0x88000, BUFFER_WIDTH);
  sceGuDepthBuffer((void *)0x110000, BUFFER_WIDTH);
  sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
  sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
  sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
  sceGuEnable(GU_SCISSOR_TEST);
  sceGuDisable(GU_DEPTH_TEST);
  sceGuFinish();
  sceGuSync(0, 0);
  sceDisplayWaitVblankStart();
  sceGuDisplay(GU_TRUE);
}

void startFrame() {
  sceGuStart(GU_DIRECT, list);
  sceGuClearColor(0xFF000000);
  sceGuClear(GU_COLOR_BUFFER_BIT);
}

void endFrame() {
  sceGuFinish();
  sceGuSync(0, 0);
  sceDisplayWaitVblankStart();
  sceGuSwapBuffers();
}

void updateLayout(HtmlTextExtractor &extractor, EpubReader &reader,
                  TextRenderer &renderer, int chapterIndex) {
  uint8_t *data = reader.LoadChapter(chapterIndex);
  if (!data)
    return;

  int wordCount = extractor.ExtractWords((char *)data, words, MAX_WORDS,
                                         wordBuffer, WORD_BUFFER_SIZE);
  free(data);

  totalLines = 0;
  memset(chapterLines, 0, sizeof(chapterLines));

  int maxWidth = isRotated ? (SCREEN_HEIGHT - 2 * LAYOUT_MARGIN)
                           : (SCREEN_WIDTH - 2 * LAYOUT_MARGIN);
  std::string currentLineStr = "";

  auto pushLine = [&](const std::string &line) {
    if (totalLines < MAX_CHAPTER_LINES) {
      strncpy(chapterLines[totalLines], line.c_str(), MAX_LINE_LEN - 1);
      chapterLines[totalLines][MAX_LINE_LEN - 1] = '\0';
      totalLines++;
    }
  };

  for (int i = 0; i < wordCount; i++) {
    if (strcmp(words[i], "\n") == 0) {
      pushLine(currentLineStr);
      currentLineStr = "";
      continue;
    }

    std::string testLine =
        currentLineStr.empty() ? words[i] : (currentLineStr + " " + words[i]);
    int width = renderer.MeasureTextWidth(testLine.c_str());

    if (width > maxWidth && !currentLineStr.empty()) {
      pushLine(currentLineStr);
      currentLineStr = words[i];
    } else {
      currentLineStr = testLine;
    }
    if (totalLines >= MAX_CHAPTER_LINES)
      break;
  }
  if (!currentLineStr.empty())
    pushLine(currentLineStr);

  int lineHeight = (int)(LAYOUT_LINE_SPACE * fontScale);
  if (lineHeight < 10)
    lineHeight = 10;

  // Portrait Height is 480, Landscape is 272.
  // We start at LAYOUT_START_Y (66) and must leave room for footer at bottom.
  int availableHeight =
      (isRotated ? SCREEN_WIDTH : SCREEN_HEIGHT) - LAYOUT_START_Y - 45;
  linesPerPage = availableHeight / lineHeight;
  if (linesPerPage < 1)
    linesPerPage = 1;

  DebugLogger::Log(
      "Layout Updated: Chapter %d, %d lines, Rotated: %d, Scale: %.2f",
      chapterIndex, totalLines, isRotated, fontScale);
}

int main(int argc, char *argv[]) {
  DebugLogger::Init();
  setup_callbacks();
  initGu();

  TextRenderer renderer;
  renderer.Initialize();
  renderer.LoadFont(fontScale);

  EpubReader reader;
  reader.Open("epub-with-cyrillic.epub");
  const EpubMetadata &meta = reader.GetMetadata();

  HtmlTextExtractor htmlExtractor;
  int currentChapter = 0;
  updateLayout(htmlExtractor, reader, renderer, currentChapter);

  InputHandler input;
  running = 1;

  while (running) {
    input.Update();
    if (input.Exit())
      break;

    bool layoutNeedsUpdate = false;

    if (showChapterMenu) {
      static int menuSelection = 0;
      if (input.UpPressed())
        menuSelection = std::max(0, menuSelection - 1);
      if (input.DownPressed())
        menuSelection = std::min((int)meta.spine.size() - 1, menuSelection + 1);
      if (input.CrossPressed()) {
        currentChapter = menuSelection;
        layoutNeedsUpdate = true;
        currentLine = 0;
        showChapterMenu = false;
      }
      if (input.TrianglePressed())
        showChapterMenu = false;
    } else {
      if (input.NextPage()) {
        if (currentLine + linesPerPage < totalLines)
          currentLine += linesPerPage;
        else if (currentChapter < (int)meta.spine.size() - 1) {
          currentChapter++;
          layoutNeedsUpdate = true;
          currentLine = 0;
        }
      }
      if (input.PrevPage()) {
        if (currentLine >= linesPerPage)
          currentLine -= linesPerPage;
        else if (currentChapter > 0) {
          currentChapter--;
          updateLayout(htmlExtractor, reader, renderer, currentChapter);
          currentLine = ((totalLines - 1) / linesPerPage) * linesPerPage;
          if (currentLine < 0)
            currentLine = 0;
        }
      }
      if (input.CirclePressed()) {
        isRotated = !isRotated;
        layoutNeedsUpdate = true;
        currentLine = 0;
      }
      if (input.SelectPressed())
        showStatusOverlay = !showStatusOverlay;
      if (input.TrianglePressed())
        showChapterMenu = true;

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

    if (layoutNeedsUpdate)
      updateLayout(htmlExtractor, reader, renderer, currentChapter);

    startFrame();
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);

    auto drawUI = [&](const char *text, int tx, int ty, uint32_t color) {
      if (isRotated) {
        // Restore logic that "worked fine" before: (SCREEN_WIDTH - ty, tx)
        // This corresponds to a Clockwise rotation mapping.
        // Angle 90.0 maps text to run Top-to-Bottom in landscape =
        // Left-to-Right in portrait.
        intraFontSetStyle(renderer.GetFont(), fontScale, color, 0, 90.0f, 0);
        intraFontPrint(renderer.GetFont(), (float)(SCREEN_WIDTH - ty),
                       (float)tx, text);
      } else {
        intraFontSetStyle(renderer.GetFont(), fontScale, color, 0, 0.0f, 0);
        intraFontPrint(renderer.GetFont(), (float)tx, (float)ty, text);
      }
    };

    // Header
    char header[256];
    if (isRotated) {
      // Shorten for 272px width
      char shortTitle[32];
      strncpy(shortTitle, meta.title, 16);
      shortTitle[16] = '\0';
      snprintf(header, 256, "%s.. C%d/%d", shortTitle, currentChapter + 1,
               (int)meta.spine.size());
    } else {
      snprintf(header, 256, "%s - Ch %d/%d", meta.title, currentChapter + 1,
               (int)meta.spine.size());
    }
    drawUI(header, 16, 30, 0xFF00FFFF);

    // Content
    int stepY = (int)(LAYOUT_LINE_SPACE * fontScale);
    if (stepY < 10)
      stepY = 10;

    for (int i = 0; i < linesPerPage && (currentLine + i) < totalLines; i++) {
      drawUI(chapterLines[currentLine + i], LAYOUT_MARGIN,
             LAYOUT_START_Y + i * stepY, 0xFFFFFFFF);
    }

    // Footer
    char footer[256];
    int p = (currentLine / linesPerPage) + 1;
    int maxP = (totalLines + linesPerPage - 1) / linesPerPage;
    if (isRotated) {
      snprintf(footer, 256, "P %d/%d | S %.1f | " ICON_LEFT ICON_RIGHT, p,
               std::max(1, maxP), fontScale);
    } else {
      snprintf(footer, 256,
               "Page %d/%d | Scale %.1f | " ICON_LEFT ICON_RIGHT " Turn", p,
               std::max(1, maxP), fontScale);
    }
    drawUI(footer, 16, isRotated ? 464 : 260, 0xFF888888);

    if (showStatusOverlay) {
      ScePspDateTime ptime;
      sceRtcGetCurrentClockLocalTime(&ptime);
      int batt = scePowerGetBatteryLifePercent();
      char status[128];
      if (isRotated) {
        snprintf(status, 128, "%02d:%02d | %d%%", ptime.hour, ptime.minute,
                 batt);
      } else {
        snprintf(status, 128, "Time: %02d:%02d | Battery: %d%%", ptime.hour,
                 ptime.minute, batt);
      }
      drawUI(status, 16, isRotated ? 440 : 235, 0xFF00FF00);
      drawUI(ICON_SELECT " Status | " ICON_TRIANGLE " Menu | " ICON_CIRCLE
                         " Rotate",
             16, isRotated ? 420 : 215, 0xFF00AAAA);
    }

    if (showChapterMenu) {
      drawUI("=== CHAPTER SELECT ===", 40, 60, 0xFF00FFFF);
      for (int i = 0; i < (int)meta.spine.size() && i < 15; i++) {
        char displayId[32];
        if (isRotated) {
          strncpy(displayId, meta.spine[i].id, 16);
          displayId[16] = '\0';
          if (strlen(meta.spine[i].id) > 16)
            strcat(displayId, "..");
        } else {
          strncpy(displayId, meta.spine[i].id, 31);
          displayId[31] = '\0';
        }
        drawUI(displayId, 50, 85 + i * 18,
               (i == currentChapter) ? 0xFF00FF00 : 0xFFFFFFFF);
      }
      drawUI(ICON_CROSS " Select | " ICON_TRIANGLE " Back", 40,
             isRotated ? 400 : 200, 0xFF00AAAA);
    }

    endFrame();
  }

  renderer.Shutdown();
  reader.Close();
  DebugLogger::Close();
  sceGuTerm();
  sceKernelExitGame();
  return 0;
}
