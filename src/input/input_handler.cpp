#include "input_handler.h"
#include <cstring>

InputHandler::InputHandler() {
  memset(&currentPad, 0, sizeof(currentPad));
  memset(&previousPad, 0, sizeof(previousPad));
}

InputHandler::~InputHandler() {}

void InputHandler::Update() {
  // Save previous state
  previousPad = currentPad;

  // Read current state
  sceCtrlReadBufferPositive(&currentPad, 1);
}

bool InputHandler::IsPressed(unsigned int button) {
  // Button just pressed (not held from previous frame)
  return (currentPad.Buttons & button) && !(previousPad.Buttons & button);
}

bool InputHandler::IsHeld(unsigned int button) {
  // Button currently held
  return (currentPad.Buttons & button);
}

bool InputHandler::IsReleased(unsigned int button) {
  // Button just released
  return !(currentPad.Buttons & button) && (previousPad.Buttons & button);
}

bool InputHandler::NextPage() {
  // Right D-pad or Circle to go forward
  return IsPressed(PSP_CTRL_RIGHT) || IsPressed(PSP_CTRL_CIRCLE);
}

bool InputHandler::PrevPage() {
  // Left D-pad or Square to go back
  return IsPressed(PSP_CTRL_LEFT) || IsPressed(PSP_CTRL_SQUARE);
}

bool InputHandler::Exit() {
  // Start button to exit
  return IsPressed(PSP_CTRL_START);
}
