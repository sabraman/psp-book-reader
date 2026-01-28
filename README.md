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
-   **Frame Throttling**: The main loop reduces poll frequency when idle, significantly lowering CPU utilization during passive reading while maintaining responsiveness to any button press.

### 2. Memory Safety Guards
On the PSP-1000, 32MB of RAM is extremely restrictive.
-   **Cover Guard**: The `EpubReader` explicitly rejects any book cover larger than 2MB uncompressed. This prevents "OOM" (Out of Memory) crashes caused by high-resolution images.
-   **Lazy Viewport Management**: UI assets and cover textures are only loaded when they enter the immediate viewport and are destroyed as soon as they scroll out of scope.

### 3. ZIP-to-Memory Extraction
To avoid the overhead of temporary files on the slow Memory Stick I/O:
-   **RWops Integration**: We use `miniz` to extract image data directly to a heap buffer, then use `SDL_RWFromMem` to feed it directly into the `SDL2_image` decoder. This bypasses the disk entirely after the initial library scan.
-   **Streaming Chapter Buffers**: Only the active chapter is held in memory. The layout engine uses pre-calculated spine metadata (sizes/offsets) to allocate exactly the required buffer size before decompression.

### 4. Optimized Font Pipeline
`SDL2_ttf` rendering is CPU-bound and slow on the PSP's specialized architecture.
-   **Dual-Tier Caching**:
    -   **Texture Cache (LRU)**: Rendered text lines are stored as hardware-accelerated textures.
    -   **Metrics Cache**: Theoretical widths of all text segments are cached using FNV-1a hashes. This makes the wrap-around layout engine an O(N) arithmetic task rather than a measurement-heavy one.
-   **Zero-Check CJK Fallback**: We detect the book language from OPF metadata and switch the primary font at the renderer level. This avoids expensive per-character Unicode block checks during the 60FPS render loop.

### 5. Render Loop Optimizations
-   **Keyed Line Rendering**: Layout positions and text line hashes are cached. The main loop only re-sets texture color/alpha modulators when themes change, rather than re-calculating drawing targets.
-   **TATE Mode Coordinates**: Screen rotation is achieved via `SDL_RenderCopyEx` at 90 degrees. To maintain intuitive controls, the input handler uses a specialized remap matrix for the D-Pad and Analog stick axes.

## Contribution

Technical contributions are welcome via Pull Requests.

## License

This project is open source.
