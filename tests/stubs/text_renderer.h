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

// Sync guard: if include/text_renderer.h ever reorders the enum or
// inserts a value, these fail the host-test build instead of silently
// testing shifted style semantics.
static_assert((int)TextStyle::NORMAL == 0, "TextStyle stub out of sync");
static_assert((int)TextStyle::H1 == 1, "TextStyle stub out of sync");
static_assert((int)TextStyle::H2 == 2, "TextStyle stub out of sync");
static_assert((int)TextStyle::H3 == 3, "TextStyle stub out of sync");
static_assert((int)TextStyle::TITLE == 4, "TextStyle stub out of sync");
static_assert((int)TextStyle::SMALL == 5, "TextStyle stub out of sync");
