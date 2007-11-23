#ifndef DS_KBD_H
#define DS_KBD_H

#include <nds.h>

typedef struct {
  u16 width;
  u16 code;
} nds_kbd_key;

void redraw_kbd(int);
void kbd_init();
u16 kbd_xy2key(u8 x, u8 y);
void kbd_togglemod(int which, int how);
void kbd_set_color_from_pos(u16 r, u16 k,u8 color);
void kbd_set_color_from_code(u16 code,u8 color);
u8 kbd_vblank(void);

#define K_UP_LEFT       0x0F0
#define K_UP            0x0F1
#define K_UP_RIGHT      0x0F2
#define K_LEFT          0x0F3
#define K_NOOP          0x0F4
#define K_RIGHT         0x0F5
#define K_DOWN_LEFT     0x0F6
#define K_DOWN          0x0F7
#define K_DOWN_RIGHT    0x0F8

#define K_MODIFIER	0x100	// if set: it's shift or ctrl or alt
#define K_CAPS		0x101
#define K_SHIFT		0x102
#define K_CTRL		0x103
#define K_ALT		0x104
#define K_F(n)		(0x200 + n)
#define K_SHIFTED_MOVE		0x0200

#endif

