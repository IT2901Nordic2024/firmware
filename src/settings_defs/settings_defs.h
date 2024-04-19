#ifndef SETTINGS_DEFS_H
#define SETTINGS_DEFS_H

#include <zephyr/settings/settings.h>
#include <zephyr/types.h>

#define MAX_SIDES 11

struct settings_data {
    char *id;
    int32_t type;
};

extern struct settings_data side_0_settings;
extern struct settings_data side_1_settings;
extern struct settings_data side_2_settings;
extern struct settings_data side_3_settings;
extern struct settings_data side_4_settings;
extern struct settings_data side_5_settings;
extern struct settings_data side_6_settings;
extern struct settings_data side_7_settings;
extern struct settings_data side_8_settings;
extern struct settings_data side_9_settings;
extern struct settings_data side_10_settings;

extern struct settings_handler side_0_conf;
extern struct settings_handler side_1_conf;
extern struct settings_handler side_2_conf;
extern struct settings_handler side_3_conf;
extern struct settings_handler side_4_conf;
extern struct settings_handler side_5_conf;
extern struct settings_handler side_6_conf;
extern struct settings_handler side_7_conf;
extern struct settings_handler side_8_conf;
extern struct settings_handler side_9_conf;
extern struct settings_handler side_10_conf;

extern struct settings_handler *side_confs[MAX_SIDES];
extern struct settings_data *side_settings[MAX_SIDES];



#endif 
