#ifndef _NDS_H_
#define _NDS_H_

#define POWER_STATE_ON            0
#define POWER_STATE_TRANSITIONING 1
#define POWER_STATE_ASLEEP        2

int nds_power_state();
void nds_show_console();
void nds_hide_console();
void nds_error();
void nds_break_into_debugger();

#endif
