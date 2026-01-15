#pragma once

#include "common_types.h"
#include <cstdio>
#include <cstring>
#include <string>

struct AppSettings {
  float fontScale = 1.0f;
  Theme theme = Theme::NIGHT;
  MarginPreset margin = MarginPreset::NORMAL;
  SpacingPreset spacing = SpacingPreset::NORMAL;
  bool showStatus = false;
};

struct BookProgress {
  char path[256];
  int chapterIndex;
  int wordIndex;
};

class SettingsManager {
public:
  static SettingsManager &Get() {
    static SettingsManager instance;
    return instance;
  }

  void Load() {
    FILE *f = fopen("config.bin", "rb");
    if (f) {
      fread(&settings, sizeof(AppSettings), 1, f);
      fclose(f);
    }

    f = fopen("progress.bin", "rb");
    if (f) {
      fread(&progress, sizeof(BookProgress), 1, f);
      fclose(f);
    }
  }

  void Save() {
    FILE *f = fopen("config.bin", "wb");
    if (f) {
      fwrite(&settings, sizeof(AppSettings), 1, f);
      fclose(f);
    }

    f = fopen("progress.bin", "wb");
    if (f) {
      fwrite(&progress, sizeof(BookProgress), 1, f);
      fclose(f);
    }
  }

  void SaveProgress(const char *path, int chapter, int word) {
    if (!path)
      return;
    strncpy(progress.path, path, 255);
    progress.path[255] = '\0';
    progress.chapterIndex = chapter;
    progress.wordIndex = word;
    Save();
  }

  AppSettings &GetSettings() { return settings; }
  BookProgress &GetProgress() { return progress; }

private:
  SettingsManager() {
    memset(&progress, 0, sizeof(BookProgress));
    progress.chapterIndex = -1;
  }
  AppSettings settings;
  BookProgress progress;
};
