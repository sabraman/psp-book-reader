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
2.  Install dependencies via the toolchain package manager.
3.  Execute `make` in the repository root.

The resulting `EBOOT.PBP` will be located in the root directory.

## Technical Implementation Details

### 1. Dynamic Power Management (DFS)
To maximize battery life, the application implements event-driven frequency scaling:
-   **Performance Mode (333MHz/166MHz)**: Active during library scans or intensive layout operations.
-   **Balanced Mode (222MHz/111MHz)**: Standard operating frequency during UI interaction.
-   **Saving Mode (66MHz/33MHz)**: Activated after 2 seconds of user inactivity. 
-   **Frame Throttling**: The main loop reduces the poll frequency when idle, significantly lowering CPU utilization during passive reading.

### 2. HTML Processing
The reader utilizes a state-machine based HTML extractor rather than a full DOM parser to reduce memory footprint and increase processing speed for large chapters. It filters out irrelevant tags and styles while maintaining document structure.

### 3. Memory Management
EPUB archives are processed in memory using a streaming approach. Only the active chapter is extracted at any time, allowing the application to handle documents significantly larger than the PSP's available RAM (32MB/64MB).

### 4. Layout Engine
The layout engine uses a metrics-based approach with O(1) word width lookups. Reading positions are tracked via anchors to maintain consistency across orientation changes or font size adjustments.

### 5. Caching Systems
-   **Metadata Caches**: Indexing results are stored in `library.cache`.
-   **Render Caches**: Text lines are stored in an LRU texture cache using FNV-1a hashes to avoid redundant rendering operations.
-   **Lazy Loading**: UI assets and book covers are loaded on-demand and evicted when no longer in the viewport.

## Contribution

Technical contributions are welcome via Pull Requests.

## License

This project is open source.
