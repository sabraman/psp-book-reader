# PSP-BookReader

A C++/SDL2 EPUB reader for the Sony PlayStation Portable (PSP).

![PSP-BookReader Library View](thumbnail.png)

## Features

-   **Interface**: Library view with cover art support and system status overlays.
-   **Orientation**: Supporting vertical (TATE) mode for a 9:16 reading aspect ratio.
-   **Typography**: TrueType font rendering with character cache.
-   **Navigation**: Chapter-based navigation and persistent page tracking.
-   **Library Management**: Metadata extraction and persistent caching for library indexing.
-   **Resource Efficiency**: LRU texture caching and incremental background layout.
-   **Customization**: User-selectable themes (Night, Sepia, Light), font scale, and margin adjustments.
-   **System Integration**: Real-time clock and battery percentage monitoring.
-   **Wide Character Support**: Conditional font switching for CJK (Chinese, Japanese, Korean) and Latin/Cyrillic scripts.
-   **Power Management**: Dynamic frequency scaling and inactivity-based frame throttling.

## Controls

| Button | Action |
| :--- | :--- |
| **Cross (X)** | Select Book / Open Menu Item |
| **Circle (O)** | Rotate Screen (TATE Mode) |
| **Triangle** | Open/Close Chapter Menu |
| **Start** | Return to Library (from Reader) / Exit App (from Library) |
| **Select** | Open Settings Menu |
| **L / R Triggers** | Fast Scroll (Library) / Page Turn (Reader) |
| **D-Pad Up/Down** | Navigate Menu / Font Size Adjustment |
| **D-Pad Left/Right** | Navigate Library / Adjust Settings |

## Build Instructions

### Dependencies

The project requires the following libraries in the PSP toolchain:

-   **SDL2**: System abstraction and rendering.
-   **SDL2_ttf**: FreeType-based font rendering.
-   **SDL2_image**: Image decoding (PNG/JPG).
-   **pugixml**: XML parsing for metadata and navigation.
-   **miniz**: ZIP extraction for EPUB data.

### Building

1.  Configure the **PSPdev** toolchain.
2.  Install dependencies via the toolchain package manager (e.g., `psp-pacman`).
3.  Execute `make` in the repository root.

The resulting `EBOOT.PBP` will be located in the root directory.

## Technical Implementation Details ("Development Hacks")

Developing a modern reader for hardware with 32MB of RAM required several specific architectural optimizations and workarounds.

### 1. Dynamic Power Management (DFS)
To maximize battery life, the application implements event-driven frequency scaling:
-   **Automated Scaling**: The CPU drops to **66MHz** after 2 seconds of inactivity, scaling back to **222MHz** for UI interaction and **333MHz** for intensive tasks like library scanning.
-   **Frame Throttling**: The main loop reduces poll frequency when idle, significantly lowering CPU utilization during passive reading.

### 2. The CJK Tokenizer Hack
Without a heavy dictionary-based tokenizer, supporting CJK line-breaking is difficult.
-   **Byte-Level Detection**: The extractor identifies 3-byte UTF-8 sequences (typical for CJK ideographs) and treats each as a standalone "word."
-   **Arithmetic Wrapping**: This allows the standard whitespace-based layout engine to wrap CJK text correctly without needing dedicated script-aware logic.

### 3. Adaptive Incremental Layout
To prevent UI stutter upon loading large book chapters, we avoid blocking the main thread.
-   **Frame Throttling**: The layout engine processes 500 words per frame. This budget is dynamically doubled to 1000 words if the user is actively waiting (e.g., pressing "Next Page").
-   **Position Anchors**: Reading positions are tracked via word indices ("Anchors"). When the user changes font size or rotates the screen, the engine reflows the text and instantly scrolls to maintain the exact word position.

### 4. Fast Library Scanning
Parsing dozens of EPUB files (ZIP/XML) on every launch is too slow for handheld use.
-   **Metadata Caching**: The application maintains a `library.cache` file. On boot, the scanner performs a fast directory listing and only opens EPUBs that are not already present in the cache.

### 5. Memory Safety & I/O Optimization
-   **Cover Guard**: To prevent crashes on the PSP-1000, uncompressed images are limited to 2MB. Larger covers are automatically rejected.
-   **RWops Extraction**: Data is decompressed via `miniz` directly to memory and fed to SDL via `SDL_RWFromMem`. This bypasses the slow Memory Stick I/O for all image decoding.
-   **Lazy Viewport Management**: UI textures are loaded only when items enter the viewport and are evicted immediately when scrolled out of scope.

### 6. Layout Hygiene & Caching
-   **Redundant Metadata Filtering**: Some EPUBs repeat the book title or author at the start of every chapter. The reader uses an insensitive heuristic check to detect and strip this noise.
-   **FNV-1a Metric Hashing**: Theoretical word widths and rendered textures are stored in independent LRU caches. Composite keys (String + Style + FontMode) are hashed once to ensure O(1) retrieval.

### 7. Coordinate Remap Engine
-   **TATE Mode Logic**: While `SDL_RenderCopyEx` handles the 90-degree visual rotation, we use a custom translation matrix for the D-Pad and Analog stick axes to ensure controls remain intuitive in vertical mode.
-   **Input Event Latching**: Atomic state transitions in the input handler prevent "double-triggering" across multiple logical states in a single frame.

## Contribution

Technical contributions are welcome via Pull Requests.

## License

This project is open source.
