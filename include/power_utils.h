#pragma once

#include <psppower.h>

enum PowerMode {
  POWER_MODE_PERFORMANCE, // 333/166
  POWER_MODE_BALANCED,    // 222/111
  POWER_MODE_SAVING       // 66/33
};

static inline void SetPowerMode(PowerMode mode) {
  switch (mode) {
  case POWER_MODE_PERFORMANCE:
    scePowerSetClockFrequency(333, 333, 166);
    break;
  case POWER_MODE_BALANCED:
    scePowerSetClockFrequency(222, 222, 111);
    break;
  case POWER_MODE_SAVING:
    scePowerSetClockFrequency(66, 66, 33);
    break;
  }
}
