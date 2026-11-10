# Chocofi OLED controls

The OLED controls are exposed on the second row of the Chocofi SET layer.

| Binding | Action |
| --- | --- |
| `&base_view_tog` | Cycle the persistent BASE view: Bongo Cat, BASE label, WPM |
| `&timeout_cycle 0` | Display timeout: 30 s → 60 s → 5 min → Never |
| `&timeout_cycle 1` | Split-half sleep timeout: 5 min → 10 min → 15 min → 30 min → Never |
| `&oled_tog` | Toggle manual OLED power |
| `&oled_brightness 0` | Reduce SSD1306 contrast |
| `&oled_brightness 1` | Increase SSD1306 contrast |

Timeout controls use tap to advance and hold (600 ms) to show the current value without changing it. Timeout feedback temporarily replaces the complete status UI with a centered full-screen overlay. Tap feedback remains visible for 2.5 seconds; hold feedback disappears on release. The active keyboard-name banner remains compact and left-aligned. Timeout selections and OLED contrast are stored in settings. Base-view selection is also persistent.

OLED contrast uses the SSD1306 driver's `display_set_contrast()` API, with 32-step adjustments and a default of 128/255. This is the physical OLED panel setting, not host-computer brightness.

The 128×32 build intentionally keeps its compact status-only layout; Bongo Cat, WPM, and layer widgets remain disabled there. The controls that act on the panel (power, contrast, display timeout) are still available.
