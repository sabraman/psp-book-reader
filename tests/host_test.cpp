// Host-side regression tests for HtmlTextExtractor.
//
// The extractor is a pure function over caller-owned buffers, so it
// compiles and runs on any host with no PSP toolchain: TextStyle is
// stubbed in tests/stubs/ (the real text_renderer.h needs SDL2) and
// DebugLogger is used as-is. Run with `make host-test`, which builds
// with ASan+UBSan and runs the assertions plus a seeded fuzz pass.
//
// Case groups:
//   1. the original self-closing-tag patch
//   2. lenient recovery for malformed markup (CrossPoint-style)
//   3. the extractor/layout newline-run contract (blank lines)
//   4. <title>/<script>/<style> skipping and word boundaries
//   5. heading style lifecycle
//   6. memory-safety edges (also covered by the --fuzz pass)

#include "html_text_extractor.h"
#include "debug_logger.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int passes = 0;
static int failures = 0;

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
  printf("fuzz done: %d iterations\n", iters);
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
  expect("unterminated CDATA stops cleanly", "A<![CDATA[ never closed", "A");
  expect("unterminated declaration stops cleanly",
         "A<!DOCTYPE html lang=\"en", "A");
  expect("recovered script with '/' in quote still skips",
         "<p>a</p><script src=\"foo/>var x=1;</script>after",
         "a|\\n|after");
  // Settled policy: an open quote swallows prose up to the tag's real
  // end, exactly like the HTML5 tokenizer (browsers do the same). The
  // loss is bounded to one tag span and is logged at runtime.
  expect("open quote span swallows prose (HTML5-accurate, logged)",
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
  // Browsers auto-close an unclosed heading at a block start.
  expectWordStyle("heading auto-closed at block open", "<h1>Title<p>para",
                  "Title", TextStyle::H1);
  expectWordStyle("block after unclosed heading is normal", "<h1>Title<p>para",
                  "para", TextStyle::NORMAL);

  // --- 6. Integration sanity ---
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

  DebugLogger::Close();
  remove("debug.log"); // don't litter the caller's directory

  return failures == 0 ? 0 : 1;
}
