#pragma once

// Pure decision logic for turning a run of '\n' words into a blank
// chapter line. Extracted from processLayout (src/core/main.cpp) so the
// host tests can pin it: main.cpp needs SDL/PSP headers and cannot be
// compiled on a host, but this header can. processLayout owns the run
// scan and applies the side effects; this decides and writes the slot.
//
// SlotT is the LineInfo shape from main.cpp (text[], style,
// startWordIdx, cacheKey, isBlank); the host tests mirror it.

#include "text_renderer.h"

struct NewlineRunResult {
  bool emitted = false;       // a blank slot was written
  int emittedSlotIndex = -1; // slot the caller should focus on
  int newTotalLines = 0;      // totalLines after this run
  bool focusTarget = false;   // position recovery fired for targetWordIdx
  bool pushAnchor = false;    // pagination: push wordIdxAfter as anchor
};

// A run of `newlines` newline words was consumed starting at `runStart`;
// the caller's word cursor is now `wordIdxAfter`. Two or more newlines
// render as exactly one blank line. `prevBlank` (the previously emitted
// slot is itself a blank) dedups runs that the per-frame budget split
// across chunks, so consecutive blanks can never be emitted.
// `linesPerPage` is clamped to >= 1 internally, so a standalone caller
// cannot hit a division by zero (processLayout already clamps).
template <typename SlotT>
NewlineRunResult LayoutNewlineRun(int runStart, int newlines,
                                 int totalLines, int maxLines,
                                 int linesPerPage, bool prevBlank,
                                 int targetWordIdx, int wordIdxAfter,
                                 SlotT *slots) {
  NewlineRunResult r;
  r.newTotalLines = totalLines;

  if (linesPerPage < 1)
    linesPerPage = 1;

  if (newlines < 2 || prevBlank || totalLines >= maxLines)
    return r; // a single '\n' is just a line terminator; blanks never stack

  slots[totalLines].text[0] = '\0';
  slots[totalLines].style = TextStyle::NORMAL;
  slots[totalLines].startWordIdx = runStart;
  slots[totalLines].cacheKey = 0; // blank lines are never rendered
  slots[totalLines].isBlank = true;

  r.emitted = true;
  r.emittedSlotIndex = totalLines;
  r.focusTarget = targetWordIdx >= 0 && runStart <= targetWordIdx &&
                  wordIdxAfter > targetWordIdx;
  r.newTotalLines = totalLines + 1;
  r.pushAnchor = r.newTotalLines % linesPerPage == 0;
  return r;
}
