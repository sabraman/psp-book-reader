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
-   **Arithmetic Wrapping**: This allows the standard whitespace-based layout engine to wrap CJK text correctly without dedicated script-aware logic.

### 3. Adaptive Incremental Layout
To prevent UI stutter upon loading large book chapters, we avoid blocking the main thread.
-   **Frame Throttling**: The layout engine processes 500 words per frame. This budget is dynamically doubled if the user is actively waiting (e.g., pressing "Next Page").
-   **Position Anchors**: Reading positions are tracked via word indices. When the user changes font size or rotates the screen, the engine reflows the text and instantly scrolls to maintain the exact word position.

### 4. Hardware-Specific Memory Guards
On the PSP-1000, 32MB of RAM is extremely restrictive.
-   **Cover Guard**: Uncompressed images are limited to 2MB. Larger covers are rejected to prevent OOM (Out of Memory) crashes.
-   **GE Texture Limit**: The PSP Graphics Engine has a 512x512 texture size limit. The app detects oversized covers and re-samples them locally to stay within hardware bounds.

### 5. Disk I/O & Serialization Hacks
-   **RWops Buffering**: Data is decompressed via `miniz` directly to memory and fed to SDL via `SDL_RWFromMem`. This bypasses the slow Memory Stick I/O for all image decoding.
-   **Zero-Overhead Serialization**: App settings and reading progress are stored as a raw binary dump of the struct (`config.bin`). This is significantly faster than JSON/XML parsing on handheld hardware.
-   **Metadata Caching**: A `library.cache` file stores book info, avoiding expensive ZIP/XML parsing on every launch.

### 6. Optimized Font Pipeline
-   **Dual-Tier Caching**: Uses a combined hardware texture cache and a theoretical metrics cache (FNV-1a hashed) to make layout an O(N) arithmetic task.
-   **Zero-Check Font Switching**: Detects book language from OPF metadata and locks the renderer to a specific font (Droid Sans Fallback vs Inter) to avoid per-character Unicode checks during the render loop.

### 7. TATE Coordinate Engine
-   **Input Remapping**: While `SDL_RenderCopyEx` handles the visual 90-degree rotation, the input handler uses a specialized remap matrix for the D-Pad and Analog axes to keep navigation intuitive.
-   **Event Latching**: Atomic state transitions in the input handler prevent "double-triggering" across multiple logical states in a single frame.

## Contribution

Technical contributions are welcome via Pull Requests.

## License

This project is open source.
