#pragma once

#include <pspctrl.h>

class InputHandler {
public:
  InputHandler();
  ~InputHandler();

  void Update();

  // Button state queries
  bool IsPressed(unsigned int button);
  bool IsHeld(unsigned int button);
  bool IsReleased(unsigned int button);

  // Common actions
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

private:
  SceCtrlData currentPad;
  SceCtrlData previousPad;
};
