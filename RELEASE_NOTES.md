# Release v1.9.0: The "Power Save" Update 🔋⚡

This release introduces dynamic power management to significantly extend PSP battery life during reading sessions.

## Install

1. Download `PSP-BookReader-v1.9.0.zip` from the release assets.
2. Copy the included `PSP-BookReader` folder to `PSP/GAME/` on your Memory Stick.
3. Keep the bundled `fonts/` directory next to `EBOOT.PBP`.
4. Launch the app from the PSP Game menu.

## 🚀 Changes

### 🔋 Battery Optimization
- **Dynamic Frequency Scaling (DFS)**: 
  - Automatically downclocks the CPU to **66MHz** after 2 seconds of inactivity.
  - Scales up to **222MHz** for smooth UI interaction.
  - Peaks at **333MHz** for intensive tasks like library scanning.
- **Frame rate throttling**: Reduces main loop wake-ups when the device is idle, further saving power.
- **Activity Detection**: Intelligent input monitoring ensures the device stays at full speed whenever you are interacting with it.

### ⚡ Optimization & Stability (from v1.8.0)
- **Stable I/O**: Resolved excessive disk activity caused by high-frequency logging.
- **Quiet Logger**: Removed frequent flushes and reduced log noise for better performance.

### 🛠 Improvements
- **Responsive Wake-up**: Instant transition from power-save to performance mode upon any button press.
- **System Longevity**: Reduced heat and wear by running the hardware at lower clock speeds when idle.
