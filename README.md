# esp-idf-t-display-s3

Minimal ESP-IDF project for LilyGO T-Display-S3 with a BSP-style component.

## What this repo now contains

- `components/tdisplays3`: simple API to initialize display and read the two buttons.
- `main/main.c`: example app that prints:
  - `Hello world`
  - state of button 1 and button 2

All demo-oriented content (LVGL demo flow, SquareLine integration, battery/brightness demo logic) was removed.

## BSP-style API

See `components/tdisplays3/t_display_s3.h`:

- `tdisplays3_init()`
- `tdisplays3_display_text()`
- `tdisplays3_button_pressed()`

## Build

```bash
idf.py build
idf.py flash monitor
```