// Host-side regression tests for HtmlTextExtractor and the shared
// blank-line layout logic.
//
// The extractor is a pure function over caller-owned buffers and the
// layout decision lives in src/core/blank_line_layout.h, so both run on
// any host with no PSP toolchain: TextStyle and DebugLogger are stubbed
// in tests/stubs/. Run with `make host-test`, which builds with
// ASan+UBSan and runs the assertions plus a seeded fuzz pass.
//
// Case groups:
//   1. the original self-closing-tag patch
//   2. lenient recovery for malformed markup (CrossPoint-style)
//   3. the extractor/layout newline-run contract (blank lines)
//   4. <title>/<script>/<style> skipping and word boundaries
//   5. heading style lifecycle
//   6. the layout-side blank-line rules (shared blank_line_layout.h)
//   7. diagnostics logs for malformed markup
//   8. memory-safety edges (also covered by the --fuzz pass)

#include "html_text_extractor.h"
#include "debug_logger.h"
#include "blank_line_layout.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// --- DebugLogger stub implementation (see tests/stubs/debug_logger.h) ---
int DebugLogger::logsEmitted = 0;
char DebugLogger::lastLog[256];
bool DebugLogger::quiet = false;
bool DebugLogger::capturing = false;
char DebugLogger::logBuffer[8192];

void DebugLogger::Init() {}
void DebugLogger::Close() {}

void DebugLogger::Log(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(lastLog, sizeof(lastLog), format, args);
  va_end(args);
  logsEmitted++;
  if (capturing) {
    size_t used = strlen(logBuffer);
    if (used + 1 < sizeof(logBuffer)) {
      strncat(logBuffer, lastLog, sizeof(logBuffer) - used - 1);
      strncat(logBuffer, "\n", sizeof(logBuffer) - strlen(logBuffer) - 1);
    }
  }
  if (!quiet)
    printf("[log] %s\n", lastLog);
}

static int passes = 0;
static int failures = 0;

static void check(const char *name, bool cond) {
  if (cond) {
    passes++;
    printf("PASS %s\n", name);
  } else {
    failures++;
    printf("FAIL %s\n", name);
  }
}

static std::vector<std::string> extract(const char *html) {
  HtmlTextExtractor ex;
  static char *words[2048];
  static TextStyle styles[2048];
  static int lens[2048];
  static char buf[262144];
  int n = ex.ExtractWords(html, words, styles, lens, 2048, buf,
                          (int)sizeof(buf));
  std::vector<std::string> out;
  for (int i = 0; i < n; i++)
    out.push_back(std::string(words[i], lens[i]));
  return out;
}

static std::string join(const std::vector<std::string> &v) {
  std::string s;
  for (size_t i = 0; i < v.size(); i++) {
    if (i)
      s += "|";
    s += v[i] == "\n" ? "\\n" : v[i];
  }
  return s;
}

static void expect(const char *name, const char *html, const char *want) {
  std::string s = join(extract(html));
  if (s == want) {
    passes++;
    printf("PASS %s\n", name);
  } else {
    failures++;
    printf("FAIL %s\n  got:  %s\n  want: %s\n", name, s.c_str(), want);
  }
}

// Asserts that extracting `html` emits a log line containing `needle`
// somewhere during the call (the trailing "Extracted N words" line is
// part of the capture too, so mid-run messages are found).
static void expectLogFires(const char *name, const char *html,
                           const char *needle) {
  DebugLogger::logBuffer[0] = '\0';
  DebugLogger::capturing = true;
  extract(html);
  DebugLogger::capturing = false;
  check(name, strstr(DebugLogger::logBuffer, needle) != nullptr);
}

static std::vector<TextStyle> extractStyles(const char *html) {
  HtmlTextExtractor ex;
  static char *words[2048];
  static TextStyle styles[2048];
  static int lens[2048];
  static char buf[262144];
  int n = ex.ExtractWords(html, words, styles, lens, 2048, buf,
                          (int)sizeof(buf));
  std::vector<TextStyle> out;
  for (int i = 0; i < n; i++)
    out.push_back(styles[i]);
  (void)words;
  return out;
}

static void expectWordStyle(const char *name, const char *html,
                            const char *word, TextStyle want) {
  auto got = extract(html);
  auto st = extractStyles(html);
  bool found = false;
  TextStyle gotStyle = TextStyle::NORMAL;
  for (size_t i = 0; i < got.size(); i++) {
    if (got[i] == word) {
      found = true;
      gotStyle = st[i];
      break;
    }
  }
  if (found && gotStyle == want) {
    passes++;
    printf("PASS %s (style %d)\n", name, (int)want);
  } else {
    failures++;
    printf("FAIL %s: word %s %s, style %d (want %d)\n", name, word,
           found ? "found" : "MISSING", found ? (int)gotStyle : -1,
           (int)want);
  }
}

// Fixed-seed tag-soup fuzz: shakes out memory-safety bugs in the
// hand-rolled scanner (meaningful under the ASan+UBSan build).
static void fuzz(int iters) {
  const char *alpha = "<>/=\"'!-? abcdpdivscriptstyletitletbodyh1c\n\t";
  const int n = (int)strlen(alpha);
  unsigned seed = 12345; // fixed seed: reproducible failures
  HtmlTextExtractor ex;
  static char *words[2048];
  static TextStyle styles[2048];
  static int lens[2048];
  static char buf[262144];

  DebugLogger::quiet = true; // keep the pass output readable
  for (int it = 0; it < iters; it++) {
    seed = seed * 1103515245 + 12345;
    int len = (seed >> 16) % 512;
    char html[513];
    for (int j = 0; j < len; j++) {
      seed = seed * 1103515245 + 12345;
      html[j] = alpha[(seed >> 16) % n];
    }
    html[len] = '\0';

    // Periodically exercise the tiny-buffer / small-budget paths too.
    int bufSize = (it % 4 == 0) ? 64 : (int)sizeof(buf);
    int maxWords = (it % 7 == 0) ? 3 : 2048;

    ex.ExtractWords(html, words, styles, lens, maxWords, buf, bufSize);
  }
  DebugLogger::quiet = false;
  printf("fuzz done: %d iterations\n", iters);
}

// Host-side mirror of main.cpp's LineInfo shape, for driving the shared
// blank-line logic in src/core/blank_line_layout.h.
struct TestLine {
  char text[256];
  TextStyle style;
  int startWordIdx;
  unsigned long long cacheKey;
  bool isBlank;
};

static void layoutTests() {
  printf("--- layout newline-run rules (shared blank_line_layout.h) ---\n");

  // A fresh run of 2 emits exactly one blank slot with the contract fields.
  {
    TestLine slots[4] = {};
    auto r = LayoutNewlineRun(7, 2, 0, 4, 10, false, -1, 9, slots);
    check("run of 2 emits one blank", r.emitted && r.newTotalLines == 1);
    check("blank slot has empty text and isBlank set",
          slots[0].text[0] == '\0' && slots[0].isBlank);
    check("blank slot style/start/cache contract",
          slots[0].style == TextStyle::NORMAL && slots[0].startWordIdx == 7 &&
              slots[0].cacheKey == 0);
    check("no anchor before a full page",
          !r.pushAnchor && r.emittedSlotIndex == 0 && !r.focusTarget);
  }

  // A single newline is only a line terminator.
  {
    TestLine slots[4] = {};
    auto r = LayoutNewlineRun(0, 1, 0, 4, 10, false, -1, 1, slots);
    check("single newline emits nothing",
          !r.emitted && r.newTotalLines == 0 && !slots[0].isBlank);
  }

  // THE chunk-split regression pin: a run of 4 split 2+2 across
  // per-frame budgets must yield exactly one blank slot, not two.
  {
    TestLine slots[4] = {};
    int blanks = 0, totalLines = 0;
    for (int chunk = 0; chunk < 2; chunk++) {
      bool prevBlank = totalLines > 0 && slots[totalLines - 1].isBlank;
      auto r = LayoutNewlineRun(chunk * 2, 2, totalLines, 4, 10, prevBlank,
                                -1, chunk * 2 + 2, slots);
      totalLines = r.newTotalLines;
      if (r.emitted)
        blanks++;
    }
    check("chunk-split run (2+2) emits exactly one blank", blanks == 1 &&
          totalLines == 1 && slots[0].isBlank && !slots[1].isBlank);
  }

  // An intact run of 4 also collapses to one blank.
  {
    TestLine slots[4] = {};
    auto r = LayoutNewlineRun(0, 4, 0, 4, 10, false, -1, 4, slots);
    check("intact run of 4 emits exactly one blank",
          r.emitted && r.newTotalLines == 1);
  }

  // Capacity: no slot beyond maxLines.
  {
    TestLine slots[4] = {};
    auto r = LayoutNewlineRun(0, 2, 4, 4, 10, false, -1, 2, slots);
    check("run at capacity emits nothing", !r.emitted);
  }

  // Position recovery fires only for targets inside the run.
  {
    TestLine slots[4] = {};
    check("target inside run focuses",
          LayoutNewlineRun(0, 2, 0, 4, 10, false, 1, 2, slots).focusTarget);
    check("target before run does not focus",
          !LayoutNewlineRun(2, 2, 0, 4, 10, false, 1, 4, slots).focusTarget);
    check("target at run end does not focus",
          !LayoutNewlineRun(0, 2, 0, 4, 10, false, 2, 2, slots).focusTarget);
    check("negative target never focuses",
          !LayoutNewlineRun(0, 2, 0, 4, 10, false, -1, 2, slots).focusTarget);
  }

  // Pagination: anchor lands exactly on page boundaries.
  {
    TestLine slots[20] = {};
    check("anchor pushed when the blank fills a page",
          LayoutNewlineRun(0, 2, 9, 20, 10, false, -1, 2, slots).pushAnchor);
    check("no anchor mid-page",
          !LayoutNewlineRun(0, 2, 0, 20, 10, false, -1, 2, slots).pushAnchor);
  }
}

int main(int argc, char **argv) {
  DebugLogger::Init();

  // --- 1. Self-closing tags (the original patch) ---
  expect("self-closing script doesn't swallow chapter",
         "<script src=\"x.js\"/><p>visible</p>", "visible|\\n");
  expect("quote-aware '>' inside attribute",
         "<a title=\"a > b\" href=\"x\">link</a>", "link");
  expect("li breaks lines", "<ul><li>one</li><li>two</li></ul>",
         "one|\\n|two|\\n");

  // --- 2. Lenient recovery for malformed markup ---
  expect("unterminated quote recovers tail",
         "<p>good</p><a title=\"unclosed>text more words here</a>",
         "good|\\n|text|more|words|here");
  expect("unterminated tag, no '>' at all", "<p>a</p><img src=\"x", "a|\\n");
  expect("unterminated comment stops cleanly", "A<!-- never closed", "A");
  expect("unterminated script stops cleanly", "<p>x</p><script>var x=1;",
         "x|\\n");
  expect("unterminated </script at EOF stops cleanly",
         "<p>x</p><script>a</", "x|\\n");
  expect("closing tag with unterminated quote recovers (skipElem)",
         "<p>x</p><script>a</script \">after", "x|\\n|after");
  expect("unterminated CDATA stops cleanly", "A<![CDATA[ never closed", "A");
  expect("unterminated declaration stops cleanly",
         "A<!DOCTYPE html lang=\"en", "A");
  expect("recovered script with '/' in quote still skips",
         "<p>a</p><script src=\"foo/>var x=1;</script>after",
         "a|\\n|after");
  // Settled policy: a quote that spans a '>' makes the span part of the
  // attribute value, exactly like the HTML5 tokenizer; the loss is
  // bounded to one tag span and the pattern is logged.
  expect("quote spanning '>' swallows prose (HTML5-accurate, logged)",
         "<a href=\"x>foo <i>y</i> \">text</a>", "text");

  // --- 3. Newline-run contract with the layout (blank lines) ---
  expect("<br><br> emits two newline words", "para one<br/><br/>para two",
         "para|one|\\n|\\n|para|two");
  expect("</br> treated as <br>", "one</br>two", "one|\\n|two");
  expect("single block boundary is one newline", "<p>a</p><p>b</p>",
         "a|\\n|b|\\n");

  // --- 4. Skipping elements; word boundaries around skips ---
  expect("title skipped",
         "<head><title>Chapter 3: The Beginning</title></head><p>Body "
         "text</p>",
         "Body|text|\\n");
  expect("self-closing title harmless", "<title/><p>Body</p>", "Body|\\n");
  expect("title mixed case skipped", "<TITLE>Leak</TITLE><p>ok</p>",
         "ok|\\n");
  // A missing </title> must resume at the next tag instead of eating
  // the chapter body (titles never contain other tags).
  expect("unclosed title resumes at next tag",
         "<html><head><title>Broken</head><body><p>text</p></body></html>",
         "text|\\n");
  expect("no glue around script", "before<script>var x=1;</script>after",
         "before|after");
  expect("no glue around style", "a<style>p{}</style>b", "a|b");
  expect("comment breaks words", "<p>foo<!-- c -->bar</p>", "foo|bar|\\n");
  expect("incorrectly-closed comment (--!>) breaks words",
         "a<!-- x --!>b<p>c</p>", "a|b|\\n|c|\\n");
  expect("CDATA breaks words", "foo<![CDATA[x]]>bar", "foo|bar");
  expect("declaration breaks words", "foo<?xml v?>bar", "foo|bar");
  expect("quoted '>' in DOCTYPE skipped",
         "<!DOCTYPE html PUBLIC \"a>b\"><p>Body</p>", "Body|\\n");
  expect("closing tag with quoted '>' skipped cleanly",
         "<p>x</p><script>a</script b=\"c>d\">after", "x|\\n|after");
  expect("abrupt comment close '<!-->'", "<p>a</p><!--><p>b</p>",
         "a|\\n|b|\\n");
  expect("abrupt comment close '<!--->", "<p>a</p><!---><p>b</p>",
         "a|\\n|b|\\n");
  expect("abrupt comment close mid-word", "foo<!-->bar", "foo|bar");
  expect("td cells are separate words",
         "<table><tr><td>cell1</td><td>cell2</td></tr></table>",
         "cell1|\\n|cell2|\\n");

  // --- 5. Heading style lifecycle ---
  expectWordStyle("h1 styled", "<h1>Big</h1>", "Big", TextStyle::H1);
  expectWordStyle("h2 text styled", "<h2>Title Here</h2>after", "Title",
                  TextStyle::H2);
  expectWordStyle("text after </h2> back to normal", "<h2>T</h2>after",
                  "after", TextStyle::NORMAL);
  expectWordStyle("self-closing h3 keeps style", "<h3/>after", "after",
                  TextStyle::NORMAL);
  // Browsers auto-close an unclosed heading at a block start (open or
  // stray closing tag alike).
  expectWordStyle("heading auto-closed at block open", "<h1>Title<p>para",
                  "Title", TextStyle::H1);
  expectWordStyle("block after unclosed heading is normal", "<h1>Title<p>para",
                  "para", TextStyle::NORMAL);
  expectWordStyle("heading auto-closed at stray </p>", "<h1>T</p>rest",
                  "T", TextStyle::H1);
  expectWordStyle("block after stray </p> is normal", "<h1>T</p>rest",
                  "rest", TextStyle::NORMAL);

  // --- 6. Layout-side blank-line rules (the shared logic) ---
  layoutTests();

  // --- 7. Diagnostics logs for malformed markup ---
  expectLogFires("quote spanning '>' logs the pattern",
                 "<a href=\"x>foo <i>y</i> \">text</a>",
                 "Attribute value spans a '>'");
  expectLogFires("valid '>' in attribute logs the same pattern",
                 "<a title=\"a > b\" href=\"x\">link</a>",
                 "Attribute value spans a '>'");
  expectLogFires("truly unterminated tag logs the skip",
                 "<p>a</p><img src=\"x", "Unterminated tag");
  expectLogFires("unterminated comment logs the skip",
                 "A<!-- never closed", "Unterminated comment");

  // --- 8. Integration sanity ---
  expect("realistic chapter",
         "<?xml version=\"1.0\"?><!DOCTYPE html>"
         "<html><head><title>Ch 1</title><style>p{}</style></head>"
         "<body><h1>One</h1><p>First para.</p><!-- note --><p>Second</p>"
         "<script>if(1)</script><p>End.</p></body></html>",
         "One|\\n|First|para.|\\n|Second|\\n|End.|\\n");
  expect("CJK char is standalone word", "ab\xE4\xB8\xAD" "cd",
         "ab|\xE4\xB8\xAD|cd");

  printf("\n%d passed, %d failed\n", passes, failures);

  bool doFuzz = argc > 1 && strcmp(argv[1], "--fuzz") == 0;
  if (doFuzz)
    fuzz(argc > 2 ? atoi(argv[2]) : 20000);

  return failures == 0 ? 0 : 1;
}
