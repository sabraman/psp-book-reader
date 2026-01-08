#include "input_handler.h"
#include "debug_logger.h"
#include <cstring>

InputHandler::InputHandler()
    : currentButtons(0), previousButtons(0), pressedButtons(0) {}

InputHandler::~InputHandler() {}

void InputHandler::Update() {
  previousButtons = currentButtons;
  pressedButtons = 0; // Clear latches for the new frame
}

void InputHandler::ProcessEvent(SDL_Event &event) {
  uint32_t bit = 0;
  bool down = false;

  if (event.type == SDL_CONTROLLERBUTTONDOWN ||
      event.type == SDL_CONTROLLERBUTTONUP) {
    down = (event.type == SDL_CONTROLLERBUTTONDOWN);
    switch (event.cbutton.button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      bit = BTN_UP;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      bit = BTN_DOWN;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      bit = BTN_LEFT;
      break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      bit = BTN_RIGHT;
      break;
    case SDL_CONTROLLER_BUTTON_A:
      bit = BTN_CROSS;
      break;
    case SDL_CONTROLLER_BUTTON_B:
      bit = BTN_CIRCLE;
      break;
    case SDL_CONTROLLER_BUTTON_X:
      bit = BTN_SQUARE;
      break;
    case SDL_CONTROLLER_BUTTON_Y:
      bit = BTN_TRIANGLE;
      break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
      bit = BTN_L;
      break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
      bit = BTN_R;
      break;
    case SDL_CONTROLLER_BUTTON_START:
      bit = BTN_START;
      break;
    case SDL_CONTROLLER_BUTTON_BACK:
      bit = BTN_SELECT;
      break;
    }
  } else if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP) {
    down = (event.type == SDL_JOYBUTTONDOWN);
    switch (event.jbutton.button) {
    case 0:
      bit = BTN_TRIANGLE;
      break;
    case 1:
      bit = BTN_CIRCLE;
      break;
    case 2:
      bit = BTN_CROSS;
      break;
    case 3:
      bit = BTN_SQUARE;
      break;
    case 4:
      bit = BTN_L;
      break;
    case 5:
      bit = BTN_R;
      break;
    case 6:
      bit = BTN_DOWN;
      break;
    case 7:
      bit = BTN_LEFT;
      break;
    case 8:
      bit = BTN_UP;
      break;
    case 9:
      bit = BTN_RIGHT;
      break;
    case 10:
      bit = BTN_SELECT;
      break;
    case 11:
      bit = BTN_START;
      break;
    }
  } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
    down = (event.type == SDL_KEYDOWN);
    switch (event.key.keysym.sym) {
    case SDLK_UP:
      bit = BTN_UP;
      break;
    case SDLK_DOWN:
      bit = BTN_DOWN;
      break;
    case SDLK_LEFT:
      bit = BTN_LEFT;
      break;
    case SDLK_RIGHT:
      bit = BTN_RIGHT;
      break;
    case SDLK_RETURN:
      bit = BTN_START;
      break;
    case SDLK_ESCAPE:
      bit = BTN_SELECT;
      break;
    case SDLK_SPACE:
      bit = BTN_CROSS;
      break;
    case SDLK_q:
      bit = BTN_SQUARE;
      break;
    case SDLK_w:
      bit = BTN_TRIANGLE;
      break;
    case SDLK_e:
      bit = BTN_CIRCLE;
      break;
    case SDLK_a:
      bit = BTN_L;
      break;
    case SDLK_s:
      bit = BTN_R;
      break;
    }
  }

  if (bit) {
    if (down) {
      // If it was already down, it's a repeat
      if (!(currentButtons & bit)) {
        pressedButtons |= bit; // Latch the press event
      }
      currentButtons |= bit;
    } else {
      currentButtons &= ~bit;
    }
  }
}

bool InputHandler::IsPressed(uint32_t bit) {
  // Check latched press OR the state transition
  bool pressed = (pressedButtons & bit) ||
                 ((currentButtons & bit) && !(previousButtons & bit));
  if (pressed) {
    // Consume the latch so multiple calls in one frame don't double count
    // (though usually we only call once per logical check)
    pressedButtons &= ~bit;
  }
  return pressed;
}

bool InputHandler::NextPage() {
  return IsPressed(BTN_RIGHT) || IsPressed(BTN_CIRCLE) || IsPressed(BTN_R);
}
bool InputHandler::PrevPage() {
  return IsPressed(BTN_LEFT) || IsPressed(BTN_SQUARE) || IsPressed(BTN_L);
}
bool InputHandler::Exit() { return IsPressed(BTN_START); }
bool InputHandler::TrianglePressed() { return IsPressed(BTN_TRIANGLE); }
bool InputHandler::CirclePressed() { return IsPressed(BTN_CIRCLE); }
bool InputHandler::SelectPressed() { return IsPressed(BTN_SELECT); }
bool InputHandler::UpPressed() { return IsPressed(BTN_UP); }
bool InputHandler::DownPressed() { return IsPressed(BTN_DOWN); }
bool InputHandler::CrossPressed() { return IsPressed(BTN_CROSS); }
bool InputHandler::LTriggerPressed() { return IsPressed(BTN_L); }
bool InputHandler::RTriggerPressed() { return IsPressed(BTN_R); }
bool InputHandler::LeftPressed() { return IsPressed(BTN_LEFT); }
bool InputHandler::RightPressed() { return IsPressed(BTN_RIGHT); }
