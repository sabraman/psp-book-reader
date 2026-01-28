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

### 2. State-Machine Based HTML Extraction
Traditional DOM parsers are too resource-intensive for the PSP. The reader uses a custom state-machine parser to extract text from EPUB chapters.
-   **Filtering**: It explicitly ignores `<script>` and `<style>` blocks and treats block-level tags as newline generators.
-   **Performance**: This allows for near-instant rendering of large chapters with minimal memory allocation.

### 3. Memory-Efficient Archive Handling
EPUBs are ZIP archives. We use `miniz` to extract only the current chapter into the heap.
-   **Memory Barrier**: To prevent fragmentation and exhaustion on 32MB hardware, the previous chapter's buffer is explicitly freed *immediately* before the new one is allocated.
-   **Result**: The application can handle documents larger than 50MB by never holding the entire decompressed book in memory.

### 4. TATE Mode Implementation
The PSP's 480x272 screen is optimized for horizontal use, which is cramped for reading.
-   **Rotation**: Text is rendered to textures as normal and then displayed using `SDL_RenderCopyEx` with a 90-degree rotation.
-   **Input Remapping**: The D-Pad coordinate system is swapped when in TATE mode (e.g., "Up" scrolls "Left") to keep navigation intuitive.

### 5. O(N) Layout Arithmetic
The reader avoids expensive re-measuring of text by caching word metrics.
-   **Metric Caching**: Word widths for each font scale are pre-calculated. This transforms line breaking into a simple arithmetic operation rather than a repeated rendering task.
-   **Anchor Points**: Reading positions are tracked via anchors (the first word on screen). When font sizes change or the screen rotates, the engine recalculates the layout and scrolls to ensure the anchor word remains visible.

### 6. Caching and Rendering 
-   **LRU Texture Cache**: Rendered text lines are stored in a Least Recently Used (LRU) cache.
-   **FNV-1a Hashing**: Text lines are hashed once during layout. The render loop uses these hashes to skip redundant string operations on every frame.
-   **Zero-Overhead Font Switching**: The book's language is detected from metadata. The renderer then locks to a specific font (e.g., `Droid Sans Fallback` for CJK or `Inter` for Latin) to avoid per-character font checks during the critical render path.

### 7. Lazy Asset Management
To prevent VRAM exhaustion, UI assets like book cover thumbnails are managed lazily. Only items within the immediate viewport are loaded into memory; textures are destroyed as they move out of scope.

## Contribution

Technical contributions are welcome via Pull Requests.

## License

This project is open source.
