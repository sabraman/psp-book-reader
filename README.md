# PSP-BookReader 📖

A modern, feature-rich EPUB reader for the Sony PlayStation Portable (PSP).

![PSP-BookReader Library View](thumbnail.png)

## Features

-   **Modern UI**: Glassmorphic library view with cover art support.
-   **Rotated "TATE" Mode**: Squeeze the most out of your screen by holding the PSP vertically (9:16 aspect ratio).
-   **Fancy Typography**: Uses high-quality fonts rendered with FreeType for crisp text.
-   **Fast Navigation**: Smart chapter menu with marquee scrolling for long titles.
-   **Performance**: Optimized C++ engine using SDL2 for hardware acceleration on the PSP.

## Installation

1.  Download the latest `release.zip` or `EBOOT.PBP` from the [Releases](https://github.com/sabraman/pspepub/releases) page.
2.  Connect your PSP to your computer via USB.
3.  Copy the `PSP-BookReader` folder to `ms0:/PSP/GAME/` on your memory stick.
4.  Copy your `.epub` books into the `books/` folder inside the application directory (`ms0:/PSP/GAME/PSP-BookReader/books/`).

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

## Building from Source

Requirements:
-   **PSPdev Toolchain**: Latest version with PSPSDK.
-   **SDL2 for PSP**: Compiled and installed in the toolchain.
-   **Dependencies**: `libzip` (or miniz included), `freetype`, `harfbuzz`.

```bash
make
```

## License

This project is open source. Feel free to modify and distribute.
