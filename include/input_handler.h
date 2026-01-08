#pragma once

#include <SDL2/SDL.h>

class InputHandler {
public:
  InputHandler();
  ~InputHandler();

  void Update();
  void ProcessEvent(SDL_Event &event);

  bool NextPage();
  bool PrevPage();
  bool Exit();
  bool TrianglePressed();
  bool CirclePressed();
  bool SelectPressed();
  bool UpPressed();
  bool DownPressed();
  bool CrossPressed();
  bool LTriggerPressed();
  bool RTriggerPressed();
  bool LeftPressed();
  bool RightPressed();

  // Manual mapping for certain SDL buttons to our bitmask
  enum {
    BTN_UP = 0x0001,
    BTN_DOWN = 0x0002,
    BTN_LEFT = 0x0004,
    BTN_RIGHT = 0x0008,
    BTN_TRIANGLE = 0x0010,
    BTN_CIRCLE = 0x0020,
    BTN_CROSS = 0x0040,
    BTN_SQUARE = 0x0080,
    BTN_L = 0x0100,
    BTN_R = 0x0200,
    BTN_START = 0x0400,
    BTN_SELECT = 0x0800
  };

private:
  uint32_t currentButtons;
  uint32_t previousButtons;
  uint32_t pressedButtons; // Latched bits for IsPressed

  bool IsPressed(uint32_t bit);
};
