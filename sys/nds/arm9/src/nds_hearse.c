#include <stdio.h>
#include <sys/dir.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include "hack.h"
#include "date.h"

#include "hearse.h"
#include "md5.h"

#include "wifi.h"

#undef open

#define BONESDIR "/NetHack"

void info_print(char *buffer)
{
  iprintf(buffer);
}

void error_print(char *buffer)
{
  iprintf(buffer);
}

char *get_hearse_server()
{
  return "frodo.dyn.gno.org";
}

int get_hearse_port()
{
  return 80;
}

char *get_hearse_path()
{
  return "/~brettk/hearse.pl";
}

char *get_clientid()
{
  return "DSCLIENT";
}

char *get_hearsecrc()
{
  return "d56522672a63fccc9f1ade8aba2f591d";
}

char *get_nick()
{
  return "fancypantalons";
}

char *get_email()
{
  return "fancypantalons@gmail.com";
}

int get_extra_bones_threshold()
{
  return 0;
}

int get_extra_bones_max()
{
  return 2;
}

char *get_token()
{
  FILE *fh = fopen("hearse-token", "r");
  char buffer[1024];

  if (fh == NULL) {
    return NULL;
  }

  fgets(buffer, sizeof(buffer), fh);
  fclose(fh);

  return strdup(buffer);
}

void set_token(char *token)
{
  FILE *fh = fopen("hearse-token", "w");

  fprintf(fh, "%s", token);

  fclose(fh);
}

int starts_with(char *str, char *prefix)
{
  int i;

  for (i = 0; prefix[i]; i++) {
    if (! str[i] || (tolower(str[i]) != tolower(prefix[i]))) {
      return 0;
    }
  }

  return 1;
}

int get_numbones()
{
  DIR_ITER *bonesdir = diropen(BONESDIR);
  char filename[1024];
  int count = 0;

  if (bonesdir == NULL) {
    return -1;
  }

  while (dirnext(bonesdir, filename, NULL) == 0) {
    if (! starts_with(filename, "bon")) {
      continue;
    }

    count++;
  }

  dirclose(bonesdir);

  return count;
}

char **get_boneslist()
{
  int count = get_numbones();
  DIR_ITER *bonesdir;
  char filename[1024];
  char **results;
  int i, j;

  if (count < 0) {
    return NULL;
  }

  bonesdir = diropen(BONESDIR);
  results = (char **)calloc(count + 1, sizeof(char *));
  i = 0;

  while (dirnext(bonesdir, filename, NULL) == 0) {
    if (! starts_with(filename, "bon")) {
      continue;
    }

    results[i] = strdup(filename);

    for (j = 0; results[i][j]; j++) {
      results[i][j] = tolower(results[i][j]);
    }

    i++;
  }

  dirclose(bonesdir);

  return results;
}

hearse_bonesfile_t *load_bones(char *name)
{
  struct stat statinfo;
  FILE *fh;
  char buffer[1024];
  char *bytes;

  int i;

  hearse_bonesfile_t *bonesfile;
  
  strcpy(buffer, BONESDIR);
  strcat(buffer, "/");
  strcat(buffer, name);

  if (stat(buffer, &statinfo) != 0) {
    return NULL;
  }

  fh = fopen(buffer, "r");

  if (fh == NULL) {
    iprintf("open failed\n");
    return NULL;
  }
  
  bytes = (char *)malloc(statinfo.st_size);

  for (i = 0; i < statinfo.st_size; i += 256) {
    int left = statinfo.st_size - i;
    int size = (left < 256) ? left : 256;

    fread(bytes + i, 1, size, fh);
  }

  fclose(fh);

  ((struct version_info *)bytes)->feature_set = VERSION_FEATURES;

  bonesfile = hearse_bonesfile_new(name, get_nick(), statinfo.st_mtime, statinfo.st_size, bytes);

  return bonesfile;
}

int save_bones(hearse_bonesfile_t *bones)
{
  char buffer[1024];
  FILE *fh;

  strcpy(buffer, BONESDIR);
  strcat(buffer, "/");
  strcat(buffer, bones->name);

  fh = fopen(buffer, "w");

  if (fh == NULL) {
    iprintf("Unable to open %s for writing\n", buffer);

    return -1;
  }

  fwrite(bones->bytes, 1, bones->length, fh);
  fclose(fh);

  iprintf("Downloaded bonesfile %s from %s\n", bones->name, bones->nickname);

  return 0;
}

int should_delete_bones()
{
  return 1;
}

void delete_bones(char *name)
{
  char buffer[1024];

  strcpy(buffer, BONESDIR);
  strcat(buffer, "/");
  strcat(buffer, name);

  unlink(buffer);
}

int get_timestamp()
{
  FILE *fh = fopen("hearse-timestamp", "r");
  int timestamp;

  if (fh == NULL) {
    return 0;
  }

  fscanf(fh, "%d", &timestamp);

  fclose(fh);

  return timestamp;
}

void set_timestamp(int time)
{
  FILE *fh = fopen("hearse-timestamp", "w");

  fprintf(fh, "%d", time);

  fclose(fh);
}

void compute_crc(char *bytes, int length, char *crc)
{
  md5_state_t state;
  md5_byte_t digest[16];
  int i;

  md5_init(&state);
  md5_append(&state, (const md5_byte_t *)bytes, length);
  md5_finish(&state, digest);

  crc[0] = '\0';

  for (i = 0; i < 16; i++) {
    char tmp[3];

    sprintf(tmp, "%.2x", digest[i]);
    strcat(crc, tmp);
  }
}

int nds_hearse()
{
  hearse_functable_t functable = {
    info_print,
    error_print, 
    get_hearse_server,
    get_hearse_port,
    get_hearse_path,
    get_clientid,
    get_hearsecrc,
    get_nick,
    get_email,
    get_extra_bones_threshold,
    get_extra_bones_max,
    get_token,
    set_token,
    get_numbones,
    get_boneslist,
    load_bones,
    save_bones,
    should_delete_bones,
    delete_bones,
    get_timestamp,
    set_timestamp,
    compute_crc
  };

  hearse_init(functable);

  if (wifi_connect() == 0) {
    hearse_run();
  }

  return 0;
}
