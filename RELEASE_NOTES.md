# Release v1.1.0: The "Clean & Polish" Update 🧹✨

This release focuses on significant codebase cleanup, documentation improvements, and build optimization. No functional changes to the reader itself, but the project structure is now much lighter and more professional.

## 🚀 Changes

### 🧹 Project Cleanup
-   **Removed ~4000 lines of unused code**: Deleted the `libintrafont` library which was included but never actually used.
-   **Optimized Assets**: Removed ~13MB of unused font variations. The app now efficiently uses a single `Inter-Regular.ttf`.
-   **Root Directory**: Cleared clutter files (`inter.zip`, debug logs, temporary build files).

### 📖 Documentation
-   **New README**: completely rewrote `README.md` with:
    -   Accurate dependency list (SDL2, pugixml, miniz).
    -   "How We Made This" section detailing technical hacks (custom HTML stripper, texture rotation).
    -   Clear contribution guidelines.
    -   Correct build instructions.

### 🛠 Build System
-   **Makefile**: Removed unused linker flags (`-lpspnet`, `-lpspnet_apctl`).
-   Note: `-lbz2` is retained as a transitive dependency for `libfreetype`.
