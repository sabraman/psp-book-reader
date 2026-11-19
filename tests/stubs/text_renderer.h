#pragma once

// Host-test stub of include/text_renderer.h. The real header needs
// SDL2; the extractor only uses the TextStyle enum, mirrored here
// with the exact same values and order.
enum class TextStyle {
  NORMAL,
  H1,    // Large header
  H2,    // Medium header
  H3,    // Small header
  TITLE, // Very large for title page
  SMALL  // For footer/status
};
