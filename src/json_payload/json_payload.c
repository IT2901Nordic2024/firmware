/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>
#include <zephyr/logging/log.h>
#include <zephyr/data/json.h>

#include "json_payload.h"

/* Register log module */
LOG_MODULE_REGISTER(json_payload, CONFIG_AWS_IOT_SAMPLE_LOG_LEVEL);

int json_payload_construct(char *message, size_t size, struct payload *payload)
{
	int err;
    const struct json_obj_descr parameters[] = {
        JSON_OBJ_DESCR_PRIM_NAMED(struct payload, "version",
                                  version, JSON_TOK_NUMBER),
        JSON_OBJ_DESCR_PRIM_NAMED(struct payload, "count",
                                  state.reported.count, JSON_TOK_NUMBER),
    };
    const struct json_obj_descr side_item_descr[] = {
        JSON_OBJ_DESCR_PRIM(struct side_item, id, JSON_TOK_STRING),
        JSON_OBJ_DESCR_PRIM(struct side_item, type, JSON_TOK_STRING),
    };
    const struct json_obj_descr reported[] = {
        JSON_OBJ_DESCR_ARRAY_NAMED(struct payload, "reported", state.reported.side_items, 11, state.reported.count, side_item_descr),
    };
    const struct json_obj_descr root[] = {
        JSON_OBJ_DESCR_OBJECT(struct payload, state, reported),
    };

    err = json_obj_encode_buf(root, ARRAY_SIZE(root), payload, message, size);
    if (err) {
        LOG_ERR("json_obj_encode_buf, error: %d", err);
        return err;
    }

    return 0;
}