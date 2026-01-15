# PSP-BookReader 📖

A modern, feature-rich EPUB reader for the Sony PlayStation Portable (PSP), built from scratch with C++ and SDL2.

![PSP-BookReader Library View](thumbnail.png)

## Features

-   **Modern UI**: Glassmorphic library view with cover art support and system status.
-   **Rotated "TATE" Mode**: Squeeze the most out of your screen by holding the PSP vertically (9:16 aspect ratio).
-   **Fancy Typography**: Uses high-quality fonts rendered with `SDL2_ttf` for crisp text.
-   **Fast Navigation**: Smart chapter menu with minimalist page indicators.
-   **Library Snapshot Caching**: Near-instant library scanning by caching book metadata.
-   **Deeply Optimized**: High-performance C++ engine featuring O(1) LRU caches, keyed rendering, and throttled background layout for a butter-smooth reading experience.
-   **Persistent Settings**: Customize your reading experience with adjustable **Font Size**, **Line Spacing**, **Margins**, and **Visual Themes** (Night, Sepia, Light).
-   **System Status**: Real-time clock and battery percentage available in both Library and Reader views.
-   **Wide Character Support**: Automatically switches to **Droid Sans Fallback** for CJK books while keeping **Inter** for Latin/Cyrillic.

## Controls

| Button | Action |
| :--- | :--- |
| **Cross (X)** | Select Book / Open Menu Item |
| **Circle (O)** | Rotate Screen (TATE Mode) |
| **Triangle** | Open/Close Chapter Menu |
| **Start** | Return to Library (from Reader) / Exit App (from Library) |
| **Select** | Open Settings Menu (Themes, Fonts, Spacing) |
| **L / R Triggers** | Fast Scroll (Library) / Previous/Next Page (Reader) |
| **D-Pad Up/Down** | Navigate Menu / Font Size (Reader) |
| **D-Pad Left/Right** | Navigate Library / Adjust Settings |

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

### 4. O(N) "Turbo Layout" Engine
We bypass complex line-breaking algorithms for speed.
-   **Caching**: We cache individual word widths and word lengths per font scale. This allows the layout engine to calculate line breaks arithmetically in O(N) time instead of re-measuring text or re-scanning strings.
-   **Smart Position Preservation**: When you change font sizes or rotate the screen, the engine tracks your reading position via "Anchors". It finds the first word currently on screen and ensures that word remains on screen after the layout is recalculated.
-   **LRU Texture Cache**: Rendered text lines are stored in an O(1) Least Recently Used (LRU) cache.
-   **Keyed Rendering**: Text hashes (FNV-1a) are pre-calculated during layout and stored in `LineInfo`, allowing the render loop to skip expensive string hashing on every frame.
-   **Dynamic Line Spacing**: Spacing is calculated based on font metrics (1.35x font height) rather than hardcoded constants, ensuring perfect readability at any zoom level.

### 5. Advanced System Tuning
To squeeze every bit of power from the PSP's hardware:
-   **Clock Speed**: We use `scePowerSetClockFrequency(333, 333, 166)` to lock the CPU at 333MHz during reader and layout operations.
-   **Memory Safety Guard**: Large high-resolution book covers are detected and rejected if they exceed memory safety limits (2MB uncompressed), preventing crashes on the 32MB PSP-1000.
-   **Metadata Caching**: Book metadata is cached in `library.cache`, avoiding expensive ZIP/XML parsing on subsequent launches.

### 6. Zero-Overhead Font Switching
To support CJK languages without destroying performance, we avoid per-character checks during the render loop.
-   **Method**: We detect the book's language (`<dc:language>`) upon load.
-   **Optimization**:
    -   If the book is **Chinese/Japanese/Korean**, we lock the renderer to *only* use `Droid Sans Fallback`.
    -   If the book is **English/Cyrillic**, we lock it to *only* use `Inter`.
-   **Result**: This allows us to support wide characters with **zero CPU overhead** during the critical render path, maintaining 60FPS.

### 6. Lazy Memory Management
To avoid crashing the 32MB/64MB PSP hardware with large libraries, we use a **Lazy Loading** strategy for UI assets.
-   **Thumbnails**: Instead of loading all book covers at startup, the app only loads the `SDL_Texture`s for the 4 books currently visible on screen.
-   **Unloading**: If the user scrolls far away, textures are destroyed to free VRAM/RAM for newly scrolled items.

## Contribution

Contributions are welcome!
1.  Fork the repository.
2.  Create a feature branch (`git checkout -b feature/amazing-feature`).
3.  Commit your changes.
4.  Open a specific Pull Request.

## License

This project is open source. Feel free to modify and distribute.
