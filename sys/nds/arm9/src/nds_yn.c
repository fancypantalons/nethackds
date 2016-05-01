#include <nds.h>

#include "hack.h"

#include "nds_debug.h"
#include "nds_util.h"
#include "nds_win.h"
#include "nds_cmd.h"

struct obj *obj_for_let(char invlet)
{
  struct obj *otmp;

  for (otmp = invent; otmp; otmp = otmp->nobj) {
    if (otmp->invlet == invlet) {
      return otmp;
    }
  }

  return NULL;
}

int class_slot_for_class(char oclass)
{
  int i;

  for (i = 0; i < MAXOCLASSES; i++) {
    if (flags.inv_order[i] == oclass) {
      return i;
    }
  }

  return 0;
}

void _nds_insert_choice(char *choices, char let)
{
  if (flags.sortloot) {
    struct obj *otmp = obj_for_let(let);
    int len = strlen(choices);
    int i;

    for (i = 0; choices[i]; i++) {
      struct obj *ochoice = obj_for_let(choices[i]);

      if (strcmpi(cxname_singular(otmp), cxname_singular(ochoice)) < 0) {
        break;
      }
    }

    if (i == len) {
      choices[i] = let;
      choices[i + 1] = '\0';
    } else {
      memmove(&(choices[i + 1]), &(choices[i]), len - i + 1);
      choices[i] = let;
    }
  } else {
    char tmp[2];

    tmp[0] = let;
    tmp[1] = '\0';

    strcat(choices, tmp);
  }
}

char *_nds_parse_choices(const char *ques)
{
  static char choices[BUFSZ];

  char choices_by_class[MAXOCLASSES][BUFSZ / MAXOCLASSES];
  char special_choices[BUFSZ / MAXOCLASSES];

  int i;

  char *ptr = index(ques, '[');
  char last_choice = -1;
  int have_hyphen = 0;
  
  if (ptr == NULL) {
    return NULL;
  } else {
    ptr++;
  }

  for (i = 0; i < MAXOCLASSES; i++) {
    choices_by_class[i][0] = '\0';
  }

  special_choices[0] = '\0';

  for (i = 0; ptr[i] && (ptr[i] != ']'); i++) {

    struct obj *otmp;

    if (strncmp(ptr + i, " or ", 4) == 0) {
      i += 3;
    } else if (ISWHITESPACE(ptr[i])) {
      continue;
    } else if ((ptr[i] == '-') && ! ISWHITESPACE(ptr[i + 1])) {
      have_hyphen = 1;
    } else if (ptr[i] == '$') {
        _nds_insert_choice(choices_by_class[class_slot_for_class(COIN_CLASS)], ptr[i]);
    } else if (have_hyphen) {
      int j;

      for (j = last_choice + 1; j <= ptr[i]; j++) {
        int idx;

        otmp = obj_for_let(j);
        idx = class_slot_for_class(otmp->oclass);

        _nds_insert_choice(choices_by_class[idx], j);
      }

      have_hyphen = 0;
    } else {
      if ((otmp = obj_for_let(ptr[i])) != NULL) {
        int idx = class_slot_for_class(otmp->oclass);

        _nds_insert_choice(choices_by_class[idx], ptr[i]);
      } else {
        char tmp[2];

        tmp[0] = ptr[i];
        tmp[1] = '\0';

        strcat(special_choices, tmp);
      }

      last_choice = ptr[i];
    }
  }

  choices[0] = '\0';

  for (i = 0; i < MAXOCLASSES; i++) {
    strcat(choices, choices_by_class[i]);
  }

  if (*special_choices) {
    strcat(choices, " ");
    strcat(choices, special_choices);
  }

  return choices;
}

/*
 *   Generic yes/no function. 'def' is the default (returned by space or
 *   return; 'esc' returns 'q', or 'n', or the default, depending on
 *   what's in the string. The 'query' string is printed before the user
 *   is asked about the string.
 *   If resp is NULL, any single character is accepted and returned.
 *   If not-NULL, only characters in it are allowed (exceptions:  the
 *   quitchars are always allowed, and if it contains '#' then digits
 *   are allowed); if it includes an <esc>, anything beyond that won't
 *   be shown in the prompt to the user but will be acceptable as input.
 */
char nds_yn_function(const char *ques, const char *cstr, CHAR_P def)
{
  char buffer[INPUT_BUFFER_SIZE];

  char *choices;
  ANY_P header_id;
  ANY_P *ids;
  winid win;
  menu_item *sel = NULL;
  int ret;
  int yn = 0;
  int ynaq = 0;
  char *direction_keys = nds_get_direction_keys();

  if ((strstr(ques, "In what direction") != NULL) ||
      (strstr(ques, "in what direction") != NULL)) {
    /*
     * We're going to use nh_poskey to get a command from the user.  However,
     * we must handle clicks specially.  Unlike normal movement, you can't
     * just click anywhere to pick a direction.  Instead, the user will be
     * expected to click in one of the adjacent squares around the player,
     * and the click will then be translated into a movement character.
     */
    while (1) {
      int x, y, mod;
      int sym;

      nds_draw_prompt("Tap an adjacent square or press a direction key.");
      nds_flush(0);
      sym = nds_get_input(&x, &y, &mod);
      nds_clear_prompt();

      if (mod == CLICK_1) {
        if ((x == u.ux - 1) && (y == u.uy - 1)) {
          return direction_keys[DIR_UP_LEFT];
        } else if ((x == u.ux) && (y == u.uy - 1)) {
          return direction_keys[DIR_UP];
        } else if ((x == u.ux + 1) && (y == u.uy - 1)) {
          return direction_keys[DIR_UP_RIGHT];
        } else if ((x == u.ux - 1) && (y == u.uy)) {
          return direction_keys[DIR_LEFT];
        } else if ((x == u.ux) && (y == u.uy)) {
          return direction_keys[DIR_WAIT];
        } else if ((x == u.ux + 1) && (y == u.uy)) {
          return direction_keys[DIR_RIGHT];
        } else if ((x == u.ux - 1) && (y == u.uy + 1)) {
          return direction_keys[DIR_DOWN_LEFT];
        } else if ((x == u.ux) && (y == u.uy + 1)) {
          return direction_keys[DIR_DOWN];
        } else if ((x == u.ux + 1) && (y == u.uy + 1)) {
          return direction_keys[DIR_DOWN_RIGHT];
        }
      } else if (mod == CLICK_2) {
        if ((x == u.ux) && (y == u.uy)) {
          return '>';
        }
      } else {
        return sym;
      }
    }
  } else if (! iflags.cmdwindow) {
    return nds_prompt_char(ques, cstr, 0);
  } else if (strstr(ques, "Adjust letter to what") != NULL) {
    return nds_prompt_char(ques, cstr, 0);
  } else if (strstr(ques, "What command?") != NULL) {
    nds_cmd_t cmd;
    
    nds_draw_prompt("Select a command.");
    nds_flush(0);
    cmd = nds_cmd_loop(CMDLOOP_WHATDOES);
    nds_clear_prompt();

    return cmd.f_char;
  } else if (strstr(ques, "What do you look for?") != NULL) {
    return nds_prompt_char(ques, cstr, 0);
  } else if (strstr(ques, "adjust?") != NULL) {
    cstr = ynchars;
  }

  if ((index(ques, '[') == NULL) && (cstr == NULL)) {
    nds_draw_prompt(ques);
    return '*';
  }

  win = create_nhwindow(NHW_MENU);

  start_menu(win);
  
  if ((cstr != NULL) && 
      ((strcasecmp(cstr, ynchars) == 0) ||
       (strcasecmp(cstr, ynqchars) == 0) ||
       ((ynaq = strcasecmp(cstr, ynaqchars)) == 0))) {

    ids = (ANY_P *)malloc(sizeof(ANY_P) * 2);

    yn = 1;

    ids[0].a_int = 'y';
    ids[1].a_int = 'n';

    add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Yes", 0);
    add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "No", 0);

    if (ynaq) {
      ids[2].a_int = 'a';

      add_menu(win, NO_GLYPH, &(ids[2]), 0, 0, 0, "All", 0);
    }
  } else if ((cstr != NULL) && (strcasecmp(cstr, "rl") == 0)) {

    ids = (ANY_P *)malloc(sizeof(ANY_P) * 2);

    ids[0].a_int = 'r';
    ids[1].a_int = 'l';

    add_menu(win, NO_GLYPH, &(ids[0]), 0, 0, 0, "Right Hand", 0);
    add_menu(win, NO_GLYPH, &(ids[1]), 0, 0, 0, "Left Hand", 0);
  } else {
    int i;
    char curclass = -1;

    choices = _nds_parse_choices(ques);

    ids = (ANY_P *)malloc(sizeof(ANY_P) * strlen(choices));
    header_id.a_int = 0;

    for (i = 0; i < strlen(choices); i++) {

      ids[i].a_int = choices[i];

      if (choices[i] == ' ') {
        add_menu(win, NO_GLYPH, &(header_id), 0, 0, 0, "Other", 0);
      } else if (choices[i] == '*') {
        add_menu(win, NO_GLYPH, &(ids[i]), 0, 0, 0, "Something from your inventory", 0);
      } else if ((choices[i] == '-') || (choices[i] == '.')) {
        add_menu(win, NO_GLYPH, &(ids[i]), 0, 0, 0, "Nothing/your finger", 0);
      } else if (choices[i] == '?') {
        continue;
      } else {
        int oclass;
        char oname[BUFSZ];

        if (choices[i] == '$') {
          long int money = money_cnt(invent);

          oclass = COIN_CLASS;
          sprintf(oname, "%ld gold piece%s", money, plur(money));
        } else {
          struct obj *otmp = obj_for_let(choices[i]);

          oclass = otmp->oclass;
          strcpy(oname, doname(otmp));
        }

        if (oclass != curclass) {
          add_menu(win, NO_GLYPH, &(header_id), 0, 0, 0, let_to_name(oclass, FALSE, FALSE), 0);

          curclass = oclass;
        } 

        add_menu(win, NO_GLYPH, &(ids[i]), 0, 0, 0, oname, 0);
      }
    }
  }

  end_menu(win, ques);

  int mode = ((cstr == NULL) || (index(cstr, '#') != NULL)) ? PICK_ONE_TYPE : PICK_ONE;
  int cnt = select_menu(win, mode, &sel);

  if (cnt <= 0) {
    ret = yn ? 'n' : '\033';
  } else if ((mode == PICK_ONE) || (sel->count < 0)) {
    ret = sel->item.a_int;
  } else if (mode == PICK_ONE_TYPE) {
    sprintf(buffer, "%d%c", sel->count, sel->item.a_int);

    nds_input_buffer_append(buffer + 1);
    ret = *buffer;
  }

  if (sel != NULL) {
    free(sel);
  }

  free(ids);

  destroy_nhwindow(win);

  return ret;
}

