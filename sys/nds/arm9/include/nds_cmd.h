#ifndef _NDS_CMD_H_
#define _NDS_CMD_H_

#define INPUT_BUFFER_SIZE 32

#define CMD_CONFIG     0xFF
#define CMD_PAN_RIGHT  0xFE
#define CMD_PAN_LEFT   0xFD
#define CMD_PAN_UP     0xFC
#define CMD_PAN_DOWN   0xFB
#define CMD_OPT_TOGGLE 0xFA
#define CMD_SHOW_KEYS  0xF9

/* These are our direction keys, ordered ul,u,ur,l,c,r,dl,d,dr */
#define DIR_UP_LEFT    0
#define DIR_UP         1
#define DIR_UP_RIGHT   2
#define DIR_LEFT       3
#define DIR_RIGHT      4
#define DIR_DOWN_LEFT  5
#define DIR_DOWN       6
#define DIR_DOWN_RIGHT 7
#define DIR_WAIT       8

#define NUMDIRS        9

/* The number of buffered command characters we'll support. */

#define MAXCMDS 10

typedef struct {
  s16 f_char;
  char *name;

  int page;
  int row;
  int col;

  rectangle_t region;

  int focused;
  int highlighted;

  int refresh;
} nds_cmd_t;

typedef enum {
  CMDLOOP_STANDARD,
  CMDLOOP_CONFIG,
  CMDLOOP_WHATDOES
} nds_cmdloop_op_type_t;

int nds_init_cmd();

int nds_input_buffer_is_empty();
void nds_input_buffer_push(char c);
void nds_input_buffer_append(char* str);
char nds_input_buffer_shift();
char *nds_input_buffer_shiftall();

char *nds_get_direction_keys();
nds_cmd_t *nds_get_cmdlist();

int nds_get_input(int *x, int *y, int *mod);
int nds_get_ext_cmd();
void nds_number_pad(int value);

nds_cmd_t nds_cmd_loop(nds_cmdloop_op_type_t optype);

#endif
