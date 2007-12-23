#ifndef _NDS_MSG_H_
#define _NDS_MSG_H_

#include "nds_win.h"

void nds_init_msg();
void nds_update_msg(nds_nhwindow_t *win, int blocking);
int nds_msg_history();

#endif
