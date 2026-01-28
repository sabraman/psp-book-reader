# Release v1.8.0: The "Stable I/O" Update 🛡️💾

This release addresses significant I/O issues that were causing high-frequency disk activity and system freezes.

## 🚀 Changes

### ⚡ Optimization & Stability
-   **Fixed Issue #5**: Resolved excessive I/O activity caused by per-frame logging (60Hz).
-   **Logger Optimization**: Removed frequent disk flushes from the debug logger to ensure smoother performance on PSP storage media.
-   **Reduced Log Noise**: Disabled verbose trace logs during library operations and settings navigation.

### 🛠 Improvements
-   **System Stability**: Reduced I/O bus saturation, preventing the application from freezing during exit or long sessions.
-   **Log Size Management**: The `debug.log` file no longer grows to excessive sizes during normal use.
