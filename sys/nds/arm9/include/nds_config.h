#ifndef _NDS_CONFIG_H_
#define _NDS_CONFIG_H_

#define KEY_MAGIC 0xDECAFBAD
#define KEY_CONFIG_FILE "keys.cnf"

/* Since this isn't exported anywhere, we duplicate the definition here. */

extern struct Bool_Opt
{
        const char *name;
        boolean *addr, initvalue;
        int optflags;
} boolopt[];

int nds_map_key(u16 pressed);
int nds_command_key_pressed(int pressed);
int nds_chord_key_held(int pressed);
int nds_chord_key_pressed(int pressed);

void nds_save_key_config();
void nds_config_key();
void nds_show_keys();
char *nds_find_key_options(u16 *helpline, u16 key);
int nds_key_config_init();

#endif
