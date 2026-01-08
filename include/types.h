#pragma once

#include <stdint.h>

// Data types from implementation plan
typedef uint32_t DocOffset;

struct GlyphKey {
  uint32_t codepoint;
  uint8_t fontId;
  uint8_t size;
  uint8_t padding[2];

  bool operator==(const GlyphKey &other) const {
    return codepoint == other.codepoint && fontId == other.fontId &&
           size == other.size;
  }
};

struct GlyphEntry {
  float u, v;
  float u2, v2;
  int16_t width;
  int16_t height;
  int16_t bearingX;
  int16_t bearingY;
  int16_t advance;
};

struct GlyphCache {
  static const int CAPACITY = 512;
  GlyphKey keys[CAPACITY];
  GlyphEntry entries[CAPACITY];
  bool occupied[CAPACITY];

  void *vramPointer;
  int currentX;
  int currentY;
  int maxHeightRow;
};

enum CommandType : uint8_t {
  CMD_DRAW_GLYPH,
  CMD_SET_COLOR,
  CMD_DRAW_RECT,
  CMD_END_PAGE
};

struct RenderCommand {
  CommandType type;
  uint8_t fontId;
  union {
    struct {
      int16_t x, y;
      uint32_t codepoint;
    } text;
    struct {
      uint32_t color;
    } color;
    struct {
      int16_t x, y, w, h;
    } rect;
  };
};

struct PageDisplayList {
  static const int MAX_CMDS = 2048;
  RenderCommand commands[MAX_CMDS];
  int count;
};

struct PageContext {
  DocOffset startOffset;
  DocOffset endOffset;
  uint8_t currentStyles;
  uint32_t lastColor;
  int16_t contentHeight;
  uint16_t sentenceCount;
};
