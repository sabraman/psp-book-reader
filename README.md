# PSP-BookReader 📖

A modern, feature-rich EPUB reader for the Sony PlayStation Portable (PSP), built from scratch with C++ and SDL2.

![PSP-BookReader Library View](thumbnail.png)

## Features

-   **Modern UI**: Glassmorphic library view with cover art support.
-   **Rotated "TATE" Mode**: Squeeze the most out of your screen by holding the PSP vertically (9:16 aspect ratio).
-   **Fancy Typography**: Uses high-quality fonts rendered with `SDL2_ttf` for crisp text.
-   **Fast Navigation**: Smart chapter menu with marquee scrolling for long titles.
-   **Performance**: Optimized C++ engine using SDL2 for hardware acceleration on the PSP.
-   **EPUB Support**: Native parsing of EPUB containers, spines, and extracting text from HTML chapters.

## Controls

| Button | Action |
| :--- | :--- |
| **Cross (X)** | Select Book / Open Menu Item |
| **Circle (O)** | Rotate Screen (TATE Mode) |
| **Triangle** | Open/Close Chapter Menu |
| **Select** | Return to Library |
| **L / R Triggers** | Fast Scroll (Library) / Previous/Next Page (Reader) |
| **D-Pad Up/Down** | Font Size (Reader) / Navigate Menu |
| **D-Pad Left/Right** | Navigate Library |

## Build Instructions

### Dependencies

This project relies on the following libraries, which must be installed in your PSP toolchain:

-   **SDL2**: Core hardware abstraction and rendering.
-   **SDL2_ttf**: Font rendering (FreeType wrapper).
-   **SDL2_image**: Image loading (Covers, UI assets).
-   **pugixml**: XML parsing for `.opf` and `.ncx` files.
-   **miniz**: Lightweight ZIP extraction for `.epub` archives.

**Note**: `harfbuzz` and `freetype` are dependencies of `SDL2_ttf`, but you don't need to link them manually if your SDL2 configuration is correct.

### Building

1.  Ensure you have the **PSPdev** toolchain installed and in your `PATH`.
2.  Install the required libraries (usually via `psp-pacman` or building manually).
3.  Run `make` in the project root:

```bash
make
```

4.  The output `EBOOT.PBP` will be generated in the root directory.

## How We Made This ("Tips & Hacks")

Developing a modern reader for a 2004 handheld with 32MB (or 64MB) of RAM required several optimizations and specific architectural choices:

### 1. Custom HTML "Text Extraction"
We do not use a full web browser engine (like WebKit) or even a heavy DOM parser. Instead, we wrote a **custom, state-machine based HTML stripper** (`ChapterRenderer::ExtractTextFromHTML`).
-   **Why?** Parsing a full DOM tree for a large book chapter consumes too much RAM and CPU.
-   **How?** We scan the raw buffer, ignoring `<script>` and `<style>` blocks specifically, and treating block-level tags as newline generators. This allows us to "render" a chapter almost instantly with minimal memory overhead.

### 2. Memory Optimization with `miniz`
EPUBs are just ZIP files. We use `miniz` to extract *only* the current chapter into the heap.
-   **The Trick**: When the user changes chapters, we `free()` the previous chapter's raw text immediately before allocating the new one.
-   **Result**: We never hold the entire book in memory. This allows opening massive EPUBs (50MB+) on the PSP hardware without crashing.

### 3. "TATE" (Vertical) Mode
The PSP screen is 480x272. Reading text horizontally is cramped.
-   **Implementation**: We render text to textures as normal, but we use `SDL_RenderCopyEx` with a 90-degree rotation angle.
-   **Input Mapping**: When in TATE mode, we swap D-Pad controls so "Up" effectively scrolls "Left" (visually Up), keeping controls intuitive regardless of orientation.

### 4. Text Layout Engine
We bypass complex line-breaking algorithms (like ICU) for speed.
-   **Approximation**: We use a greedy algorithm for word wrapping (`ChapterRenderer::WrapText`), estimating character widths.
-   **Caching**: Rendered text lines are cached in `SDL_Texture`s to prevent re-rendering every frame. The cache is aggressively cleared (`renderer.ClearCache()`) whenever the page changes to free VRAM.

## Contribution

Contributions are welcome!
1.  Fork the repository.
2.  Create a feature branch (`git checkout -b feature/amazing-feature`).
3.  Commit your changes.
4.  Open a specific Pull Request.

## License

This project is open source. Feel free to modify and distribute.
