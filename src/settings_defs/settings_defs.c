#include "settings_defs.h"
#include <errno.h>

#define DEFAULT_TYPE_VALUE 0

struct settings_data side_0_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_1_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_2_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_3_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_4_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_5_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_6_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_7_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_8_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_9_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };
struct settings_data side_10_settings = { .id = "", .type = DEFAULT_TYPE_VALUE };

struct settings_data *side_settings[MAX_SIDES] = {
    &side_0_settings,
    &side_1_settings,
    &side_2_settings,
    &side_3_settings,
    &side_4_settings,
    &side_5_settings,
    &side_6_settings,
    &side_7_settings,
    &side_8_settings,
    &side_9_settings,
    &side_10_settings,
};

int side_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg, struct settings_data *side_settings) {
    const char *next;
    int rc;

    if (settings_name_steq(name, "type", &next) && !next) {
        if (len != sizeof(side_settings->type)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &side_settings->type, sizeof(side_settings->type));
        if (rc >= 0) {
            return 0;
        }
        return rc;
    } else if (settings_name_steq(name, "id", &next) && !next) {
        if (len != sizeof(side_settings->id)) {
            return -EINVAL;
        }

        rc = read_cb(cb_arg, &side_settings->id, sizeof(side_settings->id));
        if (rc >= 0) {
            return 0;
        }
        return rc;
    }

    return -ENOENT;
}

int side_0_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_0_settings);
}

int side_1_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_1_settings);
}

int side_2_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_2_settings);
}

int side_3_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_3_settings);
}

int side_4_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_4_settings);
}

int side_5_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_5_settings);
}

int side_6_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_6_settings);
}

int side_7_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_7_settings);
}

int side_8_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_8_settings);
}

int side_9_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_9_settings);
}

int side_10_config_settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    return side_config_settings_set(name, len, read_cb, cb_arg, &side_10_settings);
}

struct settings_handler side_0_conf = {
    .name = "side_0",
    .h_set = side_0_config_settings_set,
};

struct settings_handler side_1_conf = {
    .name = "side_1",
    .h_set = side_1_config_settings_set,
};

struct settings_handler side_2_conf = {
    .name = "side_2",
    .h_set = side_2_config_settings_set,
};

struct settings_handler side_3_conf = {
    .name = "side_3",
    .h_set = side_3_config_settings_set,
};

struct settings_handler side_4_conf = {
    .name = "side_4",
    .h_set = side_4_config_settings_set,
};

struct settings_handler side_5_conf = {
    .name = "side_5",
    .h_set = side_5_config_settings_set,
};

struct settings_handler side_6_conf = {
    .name = "side_6",
    .h_set = side_6_config_settings_set,
};

struct settings_handler side_7_conf = {
    .name = "side_7",
    .h_set = side_7_config_settings_set,
};

struct settings_handler side_8_conf = {
    .name = "side_8",
    .h_set = side_8_config_settings_set,
};

struct settings_handler side_9_conf = {
    .name = "side_9",
    .h_set = side_9_config_settings_set,
};

struct settings_handler side_10_conf = {
    .name = "side_10",
    .h_set = side_10_config_settings_set,
};

struct settings_handler *side_confs[MAX_SIDES] = {
    &side_0_conf,
    &side_1_conf,
    &side_2_conf,
    &side_3_conf,
    &side_4_conf,
    &side_5_conf,
    &side_6_conf,
    &side_7_conf,
    &side_8_conf,
    &side_9_conf,
    &side_10_conf,
};

