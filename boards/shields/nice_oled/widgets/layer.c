#include "layer.h"
#include <fonts.h>
#include <zephyr/kernel.h>

void draw_layer_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_CENTER);
#else
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
#endif // CONFIG_NICE_EPAPER_ON

    char text[10] = {};

    /*
     * Keep the current Chocofi Num layer label explicit until the shared
     * Studio layer-name synchronization is implemented. Other layer names
     * are rendered exactly as supplied by zmk_keymap_layer_name().
     */
    if (state->layer_index == 1) {
        strncpy(text, "NUM", sizeof(text) - 1);
    } else if (state->layer_label == NULL) {
        snprintf(text, sizeof(text), "Layer %i", state->layer_index);
    } else {
        strncpy(text, state->layer_label, sizeof(text) - 1);
    }

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT)
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
#endif
    canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_Y, 68, &label_dsc, text);
}
