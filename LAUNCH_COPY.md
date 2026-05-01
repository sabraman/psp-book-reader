# Launch Copy

## One-Line Description

PSP-BookReader turns a Sony PSP into a dedicated EPUB reader with cover art, chapter navigation, TATE rotation, persistent progress, and battery-saving power management.

## Short Repo Description

An EPUB reader for Sony PSP built in C++/SDL2, optimized for real handheld constraints like low RAM, slow storage, and battery life.

## GitHub Release Summary

PSP-BookReader is a homebrew EPUB reader for the Sony PSP focused on making long-form reading actually usable on the device. It includes a cover-based library view, chapter navigation, persistent reading progress, theme and typography controls, TATE rotation, and hardware-aware power management to stretch battery life during reading sessions.

## Reddit / Forum Post

I built an EPUB reader for the Sony PSP.

It is meant for real reading sessions, not just a one-off demo. It has a library view with cover art, chapter navigation, saved progress, font and theme controls, optional TATE rotation, and battery-saving CPU scaling so the PSP can idle at lower clocks while you read.

The fun part was making it work within actual PSP constraints like 32MB RAM, texture limits, and slow storage. That pushed a bunch of optimizations around caching, incremental layout, and resource loading.

Repo and release:

`https://github.com/sabraman/psp-book-reader`

I would love feedback, especially from people testing unusual EPUBs, different language sets, or older PSP models.

## Developer-Focused Post

Built a PSP EPUB reader in C++/SDL2 and spent a surprising amount of time on memory limits, texture bounds, layout budgeting, and battery-aware CPU scaling.

A few fun constraints:

- 32MB RAM on early PSP hardware
- cover image resampling to stay inside hardware texture limits
- incremental chapter layout to avoid blocking the UI
- dynamic downclocking during idle reading

The result is a practical little homebrew app that turns a PSP into a dedicated ebook reader.

Repo:

`https://github.com/sabraman/psp-book-reader`

## Social Post

Turned a PSP into an EPUB reader.

PSP-BookReader has cover art, chapter navigation, saved progress, TATE rotation, theme controls, and battery-saving CPU scaling for long reading sessions on real hardware.

Repo:

`https://github.com/sabraman/psp-book-reader`

## Demo Video Caption

PSP-BookReader running on real Sony PSP hardware. Library view, cover art, chapter navigation, TATE mode, persistent progress, and battery-aware power scaling for long reading sessions.
