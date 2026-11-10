#define DT_DRV_COMPAT zmk_behavior_oled_brightness

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

#define OLED_CONTRAST_DEFAULT 128
#define OLED_CONTRAST_STEP 32

static uint8_t oled_contrast = OLED_CONTRAST_DEFAULT;

#if IS_ENABLED(CONFIG_ZMK_DISPLAY) &&     (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))
static int apply_oled_contrast(void) {
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display)) {
        return -ENODEV;
    }
    return display_set_contrast(display, oled_contrast);
}

#if IS_ENABLED(CONFIG_SETTINGS)
static int oled_settings_set(const char *name, size_t len,
                             settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "contrast") != 0) {
        return -ENOENT;
    }
    if (len != sizeof(oled_contrast)) {
        return -EINVAL;
    }

    int rc = read_cb(cb_arg, &oled_contrast, sizeof(oled_contrast));
    if (rc < 0) {
        return rc;
    }
    return apply_oled_contrast();
}

SETTINGS_STATIC_HANDLER_DEFINE(chocofi_oled, "chocofi_oled", NULL,
                               oled_settings_set, NULL, NULL);
#endif
#else
static int apply_oled_contrast(void) { return 0; }
#endif

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static const struct behavior_parameter_value_metadata brightness_values[] = {
    {
        .display_name = "Decrease",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = 0,
    },
    {
        .display_name = "Increase",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = 1,
    },
};
static const struct behavior_parameter_metadata_set brightness_param_set = {
    .param1_values = brightness_values,
    .param1_values_len = ARRAY_SIZE(brightness_values),
};
static const struct behavior_parameter_metadata brightness_metadata = {
    .sets_len = 1,
    .sets = &brightness_param_set,
};
#endif

static int oled_brightness_pressed(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 > 1) {
        return -EINVAL;
    }

    if (binding->param1 == 1) {
        oled_contrast = (oled_contrast > 255 - OLED_CONTRAST_STEP)
                            ? 255
                            : oled_contrast + OLED_CONTRAST_STEP;
    } else {
        oled_contrast = (oled_contrast < OLED_CONTRAST_STEP)
                            ? 0
                            : oled_contrast - OLED_CONTRAST_STEP;
    }

    int rc = apply_oled_contrast();
#if IS_ENABLED(CONFIG_SETTINGS)
    if (rc == 0) {
        settings_save_one("chocofi_oled/contrast", &oled_contrast, sizeof(oled_contrast));
    }
#endif
    return ZMK_BEHAVIOR_OPAQUE;
}

static int oled_brightness_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api oled_brightness_driver_api = {
    .binding_pressed = oled_brightness_pressed,
    .binding_released = oled_brightness_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &brightness_metadata,
#endif
};

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
                        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &oled_brightness_driver_api);
#endif
