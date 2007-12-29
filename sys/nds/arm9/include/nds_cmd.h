#ifndef _NDS_CMD_H_
#define _NDS_CMD_H_

void nds_init_cmd();
int nds_get_input(int *x, int *y, int *mod);
int nds_get_ext_cmd();
void nds_number_pad(int value);
char nds_yn_function(const char *ques, const char *choices, CHAR_P def);

#endif
