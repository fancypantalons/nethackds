#ifndef _HEARSE_H_
#define _HEARSE_H_

typedef struct {
  char *name;
  char *nickname;
  int timestamp;

  int length;
  char *bytes;
} hearse_bonesfile_t;

typedef char * (*hearse_string_getter_f)();
typedef void (*hearse_string_setter_f)(char *);

typedef char ** (*hearse_boneslist_getter_f)();
typedef hearse_bonesfile_t * (*hearse_bones_getter_f)(char *name);
typedef int (*hearse_bones_setter_f)(hearse_bonesfile_t *bones);

typedef int (*hearse_int_getter_f)();
typedef void (*hearse_int_setter_f)(int val);

typedef void (*hearse_print_f)(char *buffer);

typedef void (*hearse_delete_bones_f)(char *name);

typedef void (*hearse_crc_f)(char *bytes, int length, char *digest);

typedef struct {
  hearse_print_f info;
  hearse_print_f fatal;

  hearse_string_getter_f get_hearse_server;
  hearse_int_getter_f get_hearse_port;
  hearse_string_getter_f get_hearse_path;

  hearse_string_getter_f get_clientid;
  hearse_string_getter_f get_hearsecrc;
  hearse_string_getter_f get_nick;
  hearse_string_getter_f get_email;
  hearse_int_getter_f get_extra_bones_threshold;
  hearse_int_getter_f get_extra_bones_max;

  hearse_string_getter_f get_token;
  hearse_string_setter_f set_token;

  hearse_int_getter_f get_numbones;
  hearse_boneslist_getter_f get_boneslist;

  hearse_bones_getter_f load_bones;
  hearse_bones_setter_f save_bones;
  hearse_int_getter_f should_delete_bones;
  hearse_delete_bones_f delete_bones;

  hearse_int_getter_f get_timestamp;
  hearse_int_setter_f set_timestamp;

  hearse_crc_f compute_crc;
} hearse_functable_t;

void hearse_init(hearse_functable_t tbl);
int hearse_run();
int hearse_register();

hearse_bonesfile_t *hearse_bonesfile_new(char *name, char *nickname, int timestamp, int length, char *bytes);
void hearse_bonesfile_destroy(hearse_bonesfile_t *bonesfile);

#endif
