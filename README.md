# esp-idf-t-display-s3

Minimal ESP-IDF project for LilyGO T-Display-S3 with a BSP-style component.

## What this repo now contains

- `components/tdisplays3`: minimal BSP-style API to initialize the panel, lock/unlock LVGL access, and register button callbacks.
- `main/main.c`: example app that:
  - creates 3 LVGL labels (`Hello world`, button 1 state, button 2 state)
  - updates button labels from callbacks (no app polling loop)

All demo-oriented content (LVGL demo flow, SquareLine integration, battery/brightness demo logic) was removed.

## BSP-style API

See `components/tdisplays3/t_display_s3.h`:

- `tdisplays3_init()`
- `tdisplays3_display_lock()`
- `tdisplays3_display_unlock()`
- `tdisplays3_button_register_callback()`

Button callback signature:

- `void callback(button_event_t event)`

## Build

```bash
idf.py build
idf.py flash
```