/*
 * Various graphics functions.
 */

void nds_draw_hline(int x, int y, int width, u16 colour, u16 *dest);
void nds_draw_vline(int x, int y, int height, u16 colour, u16 *dest);
void nds_draw_rect(int x, int y, int width, int height, u16 colour, u16 *dest);
void nds_draw_rect_outline(int x, int y, int width, int height, u8 fill_colour, u8 line_colour, u16 *dest);

void nds_draw_text(struct font *fnt, 
                   char *str,
                   int x, int y,
                   u16 black, u16 white,
                   u16 *dest);

void nds_fill(u16 *dest, int colour);
