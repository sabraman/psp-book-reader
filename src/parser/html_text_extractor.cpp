#include "html_text_extractor.h"
#include "debug_logger.h"
#include <cctype>
#include <cstring>
#include <strings.h>

namespace {
  // 's' points just after "</".
  // Returns `true` if the tag name equals `name` (case-insensitive)
  // and ends at '>', '/', whitespace or end of string.
  bool ClosingTagIs(const char *s, const char *name) {
    size_t n = strlen(name);
    if (strncasecmp(s, name, n) != 0)
      return false;
    char e = s[n];
    return e == '>' || e == '/' || e == '\0' || isspace((unsigned char)e);
  }

  bool IsHeading(const char *name) {
    return name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && name[2] == '\0';
  }

  bool IsBlockTag(const char *name) {
    static const char *kBlock[] = {
        "p",       "div",     "li",     "ul",       "ol",     "dl",
        "dt",      "dd",      "tr",     "td",       "th",     "table",
        "thead",   "tbody",   "tfoot",  "caption",  "option", "section",
        "article", "aside",   "header", "footer",   "nav",    "blockquote",
        "pre",     "hr",      "hgroup", "figure",   "figcaption",
        "address", "body"
    };

    for (const char *b : kBlock)
      if (strcmp(name, b) == 0)
        return true;

    return false;
  }

} // namespace

HtmlTextExtractor::HtmlTextExtractor() {}

HtmlTextExtractor::~HtmlTextExtractor() {}

bool HtmlTextExtractor::IsWhitespace(char c) {
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

int HtmlTextExtractor::ExtractWords(const char *html, char **words,
                                    TextStyle *styles, int *wordLens,
                                    int maxWords, char *wordBuffer,
                                    int bufferSize) {
  if (!html || !words || !styles || !wordLens || maxWords == 0 || !wordBuffer ||
      bufferSize == 0)
    return 0;

  int wordCount = 0;
  int bufferPos = 0;
  TextStyle currentStyle = TextStyle::NORMAL;

  // When non-null we are inside <script>/<style>/<title>: everything is
  // ignored until the matching closing tag.
  const char *skipElem = nullptr;

  char currentWord[256];
  int currentWordLen = 0;

  auto commitWord = [&]() {
    if (currentWordLen > 0 && wordCount < maxWords) {
      currentWord[currentWordLen] = '\0';
      if (bufferPos + currentWordLen + 1 < bufferSize) {
        strcpy(wordBuffer + bufferPos, currentWord);
        words[wordCount] = wordBuffer + bufferPos;
        styles[wordCount] = currentStyle;
        wordLens[wordCount] = currentWordLen;
        wordCount++;
        bufferPos += currentWordLen + 1;
      }
      currentWordLen = 0;
    }
  };

  // force == false: skipped at the very start and if the previous word is
  // already a newline (so nested blocks / </p><p> give one break, not many).
  // force == true (<br>): always pushed; the layout renders a run of two
  // or more '\n' words as a visible blank line (so <br><br> makes one).
  auto pushNewline = [&](bool force) {
    if (!force &&
        (wordCount == 0 || (words[wordCount - 1] && words[wordCount - 1][0] == '\n')))
      return;
    if (wordCount < maxWords && bufferPos + 2 < bufferSize) {
      strcpy(wordBuffer + bufferPos, "\n");
      words[wordCount] = wordBuffer + bufferPos;
      styles[wordCount] = TextStyle::NORMAL; // Newlines are style-neutral
      wordLens[wordCount] = 1;
      wordCount++;
      bufferPos += 2;
    }
  };

  // Main loop through characters
  for (int i = 0; html[i] && wordCount < maxWords; i++) {
    char c = html[i];

    // Part 1: inside <script>/<style>/<title>: looking for a closing tag
    // with the same name. When found - skipping everything in between.
    if (skipElem) {
      if (c == '<' && html[i + 1] == '/' && ClosingTagIs(&html[i + 2], skipElem)) {
        const char *gt = strchr(&html[i], '>');
        if (!gt) {
          // The element never closes: the rest of the input is its
          // content, so there is nothing more to render.
          DebugLogger::Log("Unterminated <%s>: rest of chapter skipped",
                           skipElem);
          break;
        }
        i = (int)(gt - html);
        skipElem = nullptr;
      }
      continue;
    }

    // Part 2: markup and tags
    if (c == '<') {

      // Markup: skipping everything inside the markup constructions

      // Comment: skip to "-->"
      if (strncmp(&html[i], "<!--", 4) == 0) {
        const char *e = strstr(&html[i + 4], "-->");
        if (!e) {
          // Unterminated comment: it runs to the end of input (HTML5 and
          // CrossPoint's lexer agree) - nothing renderable follows it.
          DebugLogger::Log("Unterminated comment: rest of chapter skipped");
          break;
        }
        i = (int)(e - html) + 2;
        continue;
      }
      // CDATA: skip to "]]>"
      if (strncmp(&html[i], "<![CDATA[", 9) == 0) {
        const char *e = strstr(&html[i + 9], "]]>");
        if (!e) {
          // Unterminated CDATA: runs to the end of input, like a comment.
          DebugLogger::Log("Unterminated CDATA: rest of chapter skipped");
          break;
        }
        i = (int)(e - html) + 2;
        continue;
      }
      // <?xml ...?>, <!DOCTYPE ...>: skip to '>'
      if (html[i + 1] == '!' || html[i + 1] == '?') {
        const char *gt = strchr(&html[i], '>');
        if (!gt) {
          // Unterminated declaration: runs to the end of input.
          DebugLogger::Log("Unterminated declaration: rest of chapter skipped");
          break;
        }
        i = (int)(gt - html);
        continue;
      }

      // HTML tags:
      //  1. Checking if it's a closing tag
      //  2. Reading the name and finding the closing '>' of the tag
      //  3. Checking if it's a self closing tag and decide what to do
      //     with a tag according to its name and status (opening/closing/self-closing).
      const char *t = &html[i + 1];
      bool closingTag = (*t == '/');
      if (closingTag)
        t++;

      // Reading the tag name (lower-cased, max 15 chars).
      char tag_name[16];
      int nl = 0;
      bool tooLong = false;
      const char *p = t;

      while (*p && *p != '>' && *p != '/' && !isspace((unsigned char)*p)) {
        if (nl < 15)
          tag_name[nl++] = (char)tolower((unsigned char)*p);
        else
          tooLong = true;
        p++;
      }

      tag_name[nl] = '\0';
      if (tooLong)
        tag_name[0] = '\0';

      // Looking for the `>` that really ends the tag.
      // Finding just the first `>` isn't enough, because attribute values
      // of a tag can contain one:
      //
      //    <a title="a > b" href="x">link</a>
      //
      char quote = 0;  // when quote == 0 we are outside of quotes
      const char *q = p;

      while (*q) {
        if (quote) {
          if (*q == quote)
            quote = 0;
        } else if (*q == '"' || *q == '\'') {
          quote = *q;
        } else if (*q == '>') {
          break;
        }
        q++;
      }
      // *q can point either on > or \0 (bc it reached the end of the
      // string without finding a closing >)
      bool recovered = false;
      if (!*q) {
        // Unterminated tag (e.g. an unclosed attribute quote). Lenient
        // fallback, after the CrossPoint firmware pattern: one malformed
        // tag must not cost the reader the rest of the chapter, so resume
        // at the first '>' and keep rendering the text that follows.
        q = strchr(p, '>');
        if (!q) {
          DebugLogger::Log("Unterminated tag: rest of chapter skipped");
          break; // no '>' at all: the tag runs to the end of input
        }
        recovered = true;
        DebugLogger::Log("Malformed tag: recovering at the first '>'");
      }

      // A recovered tag is malformed by definition: a '/' just before the
      // recovered '>' may be a quote fragment, not a self-closing marker.
      // Treat it as a normal tag so <script src="foo/> (missing closing
      // quote) still enters the skipElem path instead of leaking its body.
      bool selfClosingTag = !recovered && q > p && q[-1] == '/';

      if (!closingTag) {
        if (strcmp(tag_name, "script") == 0) {
          if (!selfClosingTag) {
            commitWord(); // don't glue the words around the skipped element
            skipElem = "script";
          }
        } else if (strcmp(tag_name, "style") == 0) {
          if (!selfClosingTag) {
            commitWord();
            skipElem = "style";
          }
        } else if (strcmp(tag_name, "title") == 0) {
          // <title> is the only text-bearing <head> element in practice;
          // skip to </title> so the chapter title doesn't leak as line 1.
          if (!selfClosingTag) {
            commitWord();
            skipElem = "title";
          }
        } else if (strcmp(tag_name, "br") == 0) {
          commitWord();
          pushNewline(true);
        } else if (IsHeading(tag_name)) {
          commitWord();
          if (!selfClosingTag) { // it's a rare but valid case for XHTML
            if (tag_name[1] == '1')
              currentStyle = TextStyle::H1;
            else if (tag_name[1] == '2')
              currentStyle = TextStyle::H2;
            else if (tag_name[1] == '3')
              currentStyle = TextStyle::H3;
            else
              currentStyle = TextStyle::NORMAL;
          }
          pushNewline(false);
        } else if (IsBlockTag(tag_name)) {
          commitWord();
          pushNewline(false);
        }
        // anything else (span, i, b, a, img, ...) is inline: ignored,
        // and does not break the current word.
      } else {
        if (strcmp(tag_name, "br") == 0) {
          // Browsers treat a stray </br> as <br>.
          commitWord();
          pushNewline(true);
        } else if (IsHeading(tag_name)) {
          commitWord(); // commit with the heading's style first
          currentStyle = TextStyle::NORMAL;
          pushNewline(false);
        } else if (IsBlockTag(tag_name)) {
          commitWord();
          pushNewline(false);
        }
      }

      i = (int)(q - html); // now on '>', loop increment moves past it
      continue;
    }

    unsigned char uc = (unsigned char)c;
    // Check for CJK start byte (roughly 0xE0 - 0xEF for common CJK)
    if (uc >= 0xE0 && uc <= 0xEF) {
      // Commit pending word first
      commitWord();

      // Capture the 3-byte char as a standalone "word"
      if (html[i + 1] && html[i + 2]) {
        currentWord[0] = c;
        currentWord[1] = html[i + 1];
        currentWord[2] = html[i + 2];
        currentWordLen = 3;
        commitWord(); // Commit this character immediately
        i += 2;       // Skip next 2 bytes (loop adds 1 more)
      }
    } else if (IsWhitespace(c)) {
      commitWord();
    } else if (currentWordLen < 255) {
      currentWord[currentWordLen++] = c;
    }
  }

  commitWord();
  if (wordCount < maxWords) {
    words[wordCount] = nullptr;
  }
  DebugLogger::Log("Extracted %d words", wordCount);
  return wordCount;
}
