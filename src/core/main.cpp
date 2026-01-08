#include "epub_reader.h"
#include "input_handler.h"
#include "text_renderer.h"
#include <cstdio>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>

PSP_MODULE_INFO("PSP-BookReader", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define BUFFER_WIDTH 512
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

// Display list for GU
unsigned int __attribute__((aligned(16))) list[262144];

// Exit callback setup
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
  sceGuDepthRange(65535, 0);
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
  sceGuClearColor(0xFF000000); // Black background
  sceGuClearDepth(0);
  sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
}

void endFrame() {
  sceGuFinish();
  sceGuSync(0, 0);
  sceDisplayWaitVblankStart();
  sceGuSwapBuffers();
}

int main(int argc, char *argv[]) {
  setup_callbacks();
  initGu();

  // Initialize text renderer with Inter font
  TextRenderer renderer;
  renderer.Initialize();
  bool fontLoaded = renderer.LoadFont(1.0f);

  // Load EPUB
  EpubReader reader;
  bool epubLoaded = reader.Open("epub-with-cyrillic.epub");

  InputHandler input;
  running = 1;

  int y = 10;

  while (running) {
    input.Update();

    if (input.Exit()) {
      break;
    }

    startFrame();

    // Enable GU for 2D rendering
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexMode(GU_PSM_T8, 0, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);

    // Render text
    y = 10;

    if (fontLoaded) {
      renderer.RenderText("PSP-BookReader v0.3", 10, y, 0xFFFFFFFF);
      y += 25;
      renderer.RenderText("Inter Font + Cyrillic!", 10, y, 0xFF00FFFF);
      y += 25;
      renderer.RenderText("======================", 10, y, 0xFFFFFFFF);
      y += 30;
    }

    if (epubLoaded) {
      const EpubMetadata &meta = reader.GetMetadata();

      if (fontLoaded) {
        renderer.RenderText("EPUB Loaded!", 10, y, 0xFF00FF00);
        y += 25;
        y += 10;

        // Display title with Cyrillic support
        char titleBuf[256];
        snprintf(titleBuf, sizeof(titleBuf), "Title: %s", meta.title);
        renderer.RenderText(titleBuf, 10, y, 0xFFFFFF00);
        y += 20;

        // Display author
        char authorBuf[256];
        snprintf(authorBuf, sizeof(authorBuf), "Author: %s", meta.author);
        renderer.RenderText(authorBuf, 10, y, 0xFFFFFF00);
        y += 20;

        // Display chapter count
        char chapterBuf[128];
        snprintf(chapterBuf, sizeof(chapterBuf), "Chapters: %d",
                 (int)meta.spine.size());
        renderer.RenderText(chapterBuf, 10, y, 0xFFFFFF00);
        y += 30;

        // Show first few chapters
        renderer.RenderText("First chapters:", 10, y, 0xFFFFFFFF);
        y += 20;

        int showCount = meta.spine.size() < 5 ? meta.spine.size() : 5;
        for (int i = 0; i < showCount; i++) {
          char chBuf[256];
          snprintf(chBuf, sizeof(chBuf), "  %d. %s", i + 1, meta.spine[i].href);
          renderer.RenderText(chBuf, 10, y, 0xFFCCCCCC);
          y += 18;
        }
      }
    }

    y = SCREEN_HEIGHT - 25;
    if (fontLoaded) {
      renderer.RenderText("Press START to exit", 10, y, 0xFF888888);
    }

    endFrame();
  }

  renderer.Shutdown();
  reader.Close();
  sceGuTerm();
  sceKernelExitGame();
  return 0;
}
