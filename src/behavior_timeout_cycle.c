#define DT_DRV_COMPAT zmk_behavior_timeout_cycle

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/activity.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/runtime_timeout_changed.h>

enum timeout_kind {
    TIMEOUT_KIND_DISPLAY = 0,
    TIMEOUT_KIND_HALVES_SLEEP = 1,
    TIMEOUT_KIND_COUNT,
};

#define TIMEOUT_HOLD_MS 600

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static const struct behavior_parameter_value_metadata timeout_param_values[] = {
    {
        .display_name = "Display Timeout",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = TIMEOUT_KIND_DISPLAY,
    },
    {
        .display_name = "Half Sleep Timeout",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = TIMEOUT_KIND_HALVES_SLEEP,
    },
};

static const struct behavior_parameter_metadata_set timeout_param_set = {
    .param1_values = timeout_param_values,
    .param1_values_len = ARRAY_SIZE(timeout_param_values),
};

static const struct behavior_parameter_metadata timeout_metadata = {
    .sets_len = 1,
    .sets = &timeout_param_set,
};
#endif

static const uint32_t display_timeouts[] = {30000, 60000, 300000, 0};
static const uint32_t halves_sleep_timeouts[] = {300000, 600000, 900000, 1800000, 0};

/* Match the current Chocofi defaults: display 60 s, half sleep 15 min. */
static uint8_t display_timeout_index = 1;
static uint8_t halves_sleep_timeout_index = 2;
static bool timeout_pressed[TIMEOUT_KIND_COUNT];
static int64_t timeout_pressed_at[TIMEOUT_KIND_COUNT];

static uint32_t current_timeout_ms(enum timeout_kind kind) {
    return kind == TIMEOUT_KIND_DISPLAY
               ? display_timeouts[display_timeout_index]
               : halves_sleep_timeouts[halves_sleep_timeout_index];
}

static void emit_timeout_feedback(enum timeout_kind kind,
                                   enum zmk_runtime_timeout_feedback feedback) {
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    raise_zmk_runtime_timeout_changed((struct zmk_runtime_timeout_changed) {
        .kind = kind == TIMEOUT_KIND_DISPLAY
                    ? ZMK_RUNTIME_TIMEOUT_DISPLAY
                    : ZMK_RUNTIME_TIMEOUT_HALVES_SLEEP,
        .timeout_ms = current_timeout_ms(kind),
        .feedback = feedback,
    });
#else
    ARG_UNUSED(kind);
    ARG_UNUSED(feedback);
#endif
}

static void display_hold_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    if (timeout_pressed[TIMEOUT_KIND_DISPLAY]) {
        emit_timeout_feedback(TIMEOUT_KIND_DISPLAY, ZMK_RUNTIME_TIMEOUT_FEEDBACK_HOLD);
    }
}

static void halves_hold_work_cb(struct k_work *work) {
    ARG_UNUSED(work);
    if (timeout_pressed[TIMEOUT_KIND_HALVES_SLEEP]) {
        emit_timeout_feedback(TIMEOUT_KIND_HALVES_SLEEP, ZMK_RUNTIME_TIMEOUT_FEEDBACK_HOLD);
    }
}

K_WORK_DELAYABLE_DEFINE(display_hold_work, display_hold_work_cb);
K_WORK_DELAYABLE_DEFINE(halves_hold_work, halves_hold_work_cb);

static void apply_display_timeout(void) {
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    zmk_activity_set_idle_timeout_ms(display_timeouts[display_timeout_index]);
#endif
}

static void apply_halves_sleep_timeout(void) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#if IS_ENABLED(CONFIG_ZMK_SLEEP)
    zmk_activity_set_sleep_timeout_ms(halves_sleep_timeouts[halves_sleep_timeout_index]);
#endif
#endif
}

#if IS_ENABLED(CONFIG_SETTINGS)
static int timeout_settings_set(const char *name, size_t len,
                                settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "display") == 0) {
        uint8_t value;
        if (len != sizeof(value)) {
            return -EINVAL;
        }
        int rc = read_cb(cb_arg, &value, sizeof(value));
        if (rc < 0) {
            return rc;
        }
        if (value >= ARRAY_SIZE(display_timeouts)) {
            return -EINVAL;
        }
        display_timeout_index = value;
        apply_display_timeout();
        return 0;
    }

    if (strcmp(name, "halves") == 0) {
        uint32_t timeout_ms;
        if (len == sizeof(uint8_t)) {
            static const uint32_t legacy_values[] = {300000, 900000, 1800000, 0};
            uint8_t old_index;
            int rc = read_cb(cb_arg, &old_index, sizeof(old_index));
            if (rc < 0) {
                return rc;
            }
            if (old_index >= ARRAY_SIZE(legacy_values)) {
                return -EINVAL;
            }
            timeout_ms = legacy_values[old_index];
        } else if (len == sizeof(uint32_t)) {
            int rc = read_cb(cb_arg, &timeout_ms, sizeof(timeout_ms));
            if (rc < 0) {
                return rc;
            }
        } else {
            return -EINVAL;
        }

        for (uint8_t i = 0; i < ARRAY_SIZE(halves_sleep_timeouts); i++) {
            if (halves_sleep_timeouts[i] == timeout_ms) {
                halves_sleep_timeout_index = i;
                apply_halves_sleep_timeout();
                return 0;
            }
        }
        return -EINVAL;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(chocofi_timeout, "chocofi_timeout", NULL,
                               timeout_settings_set, NULL, NULL);

static void persist_timeout(enum timeout_kind kind) {
    if (kind == TIMEOUT_KIND_DISPLAY) {
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        settings_save_one("chocofi_timeout/display", &display_timeout_index,
                          sizeof(display_timeout_index));
#endif
    } else {
        uint32_t timeout_ms = halves_sleep_timeouts[halves_sleep_timeout_index];
        settings_save_one("chocofi_timeout/halves", &timeout_ms, sizeof(timeout_ms));
    }
}
#else
static void persist_timeout(enum timeout_kind kind) { ARG_UNUSED(kind); }
#endif

static void cycle_timeout(enum timeout_kind kind) {
    if (kind == TIMEOUT_KIND_DISPLAY) {
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        display_timeout_index = (display_timeout_index + 1) % ARRAY_SIZE(display_timeouts);
        apply_display_timeout();
        persist_timeout(kind);
        emit_timeout_feedback(kind, ZMK_RUNTIME_TIMEOUT_FEEDBACK_TIMED);
#endif
        return;
    }

    halves_sleep_timeout_index =
        (halves_sleep_timeout_index + 1) % ARRAY_SIZE(halves_sleep_timeouts);
    apply_halves_sleep_timeout();
    persist_timeout(kind);
    emit_timeout_feedback(kind, ZMK_RUNTIME_TIMEOUT_FEEDBACK_TIMED);
}

static int timeout_cycle_pressed(struct zmk_behavior_binding *binding,
                                 struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 >= TIMEOUT_KIND_COUNT) {
        return -EINVAL;
    }

    enum timeout_kind kind = binding->param1;
    timeout_pressed[kind] = true;
    timeout_pressed_at[kind] = k_uptime_get();

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    if (kind == TIMEOUT_KIND_DISPLAY) {
        k_work_reschedule(&display_hold_work, K_MSEC(TIMEOUT_HOLD_MS));
    } else {
        k_work_reschedule(&halves_hold_work, K_MSEC(TIMEOUT_HOLD_MS));
    }
#endif
    return ZMK_BEHAVIOR_OPAQUE;
}

static int timeout_cycle_released(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 >= TIMEOUT_KIND_COUNT) {
        return -EINVAL;
    }

    enum timeout_kind kind = binding->param1;
    int64_t held_ms = k_uptime_get() - timeout_pressed_at[kind];
    timeout_pressed[kind] = false;

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    if (kind == TIMEOUT_KIND_DISPLAY) {
        k_work_cancel_delayable(&display_hold_work);
    } else {
        k_work_cancel_delayable(&halves_hold_work);
    }
#endif

    if (held_ms >= TIMEOUT_HOLD_MS) {
        emit_timeout_feedback(kind, ZMK_RUNTIME_TIMEOUT_FEEDBACK_HIDE);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    cycle_timeout(kind);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api timeout_cycle_driver_api = {
    .binding_pressed = timeout_cycle_pressed,
    .binding_released = timeout_cycle_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &timeout_metadata,
#endif
};

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL,
                        POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &timeout_cycle_driver_api);
#endif
