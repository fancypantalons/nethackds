#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "http.h"
#include "hearse.h"

static hearse_functable_t functable;

void hearse_init(hearse_functable_t tbl)
{
  functable = tbl;
}

http_request_t *_hearse_new_request(http_request_type_t type, char *action)
{
  http_request_t *request = http_request_new(type,
                                             functable.get_hearse_server(),
                                             functable.get_hearse_port(),
                                             functable.get_hearse_path());
  char *token = functable.get_token();

  http_request_add_param(request, "act", action);

  http_request_add_header(request, "X_HEARSECRC", functable.get_hearsecrc());
  http_request_add_header(request, "X_CLIENTID", functable.get_clientid());

  if (token != NULL) {
    http_request_add_header(request, "X_USERTOKEN", functable.get_token());
    http_request_add_header(request, "X_USERNICK", functable.get_nick());
    http_request_add_header(request, "X_USEREMAIL", functable.get_email());
  }

  return request;
}

void _hearse_free_boneslist(char **bones)
{
  int i;

  for (i = 0; bones[i]; i++) {
    free(bones[i]);
  }

  free(bones);
}

char *_hearse_build_bones_string()
{
  static char buffer[1024];
  char **bones = functable.get_boneslist();
  int i;

  for (i = 0; bones[i]; i++) {
    strcat(buffer, bones[i]);

    if (bones[i + 1]) {
      strcat(buffer, ",");
    }
  }

  _hearse_free_boneslist(bones);

  return buffer;
}

int _hearse_handle_any_error(http_response_t *response)
{
  char *error = http_response_get_header(response, "X_ERROR");

  if (error != NULL) {
    if (strcasecmp(error, "INFO") == 0) {
      functable.info(response->content);

      return 1;
    } else if (strcasecmp(error, "FATAL") == 0) {
      functable.fatal(response->content);

      return -1;
    }
  }

  return 0;
}

int _hearse_new_user()
{
  http_request_t *request = _hearse_new_request(HTTP_REQUEST_GET, "newuser");
  http_response_t *response;
  char *token;
  int retval = 0;

  http_request_add_header(request, "X_USERNICK", functable.get_nick());
  http_request_add_header(request, "X_USEREMAIL", functable.get_email());

  response = http_request_perform(request);

  if (_hearse_handle_any_error(response) < 0) {
    retval = -1;

    goto DONE;
  }

  token = http_response_get_header(response, "X_USERTOKEN");

  if (token == NULL) {
    functable.fatal("Server did not provide a token!");

    retval = -1;
  } else {
    functable.set_token(token);
  }

DONE:

  http_request_destroy(request);
  http_response_destroy(response);

  return retval;
}

int _hearse_upload_one_bonesfile(hearse_bonesfile_t *bonesfile)
{
  http_request_t *request = _hearse_new_request(HTTP_REQUEST_POST, "upload");
  http_response_t *response;
  char crc[33];
  int ret;

  functable.compute_crc(bonesfile->bytes, bonesfile->length, crc);

  http_request_add_header(request, "X_FILENAME", bonesfile->name);
  http_request_add_header(request, "X_BONESCRC", crc);

  request->content = bonesfile->bytes;
  request->content_length = bonesfile->length;
  request->content_type = "application/octet-stream";

  response = http_request_perform(request);

  ret = _hearse_handle_any_error(response);

  http_request_destroy(request);
  http_response_destroy(response);

  return ret;
}

int _hearse_upload_bones()
{
  char **boneslist = functable.get_boneslist();
  int i;
  int timestamp = functable.get_timestamp();
  int count = 0;

  for (i = 0; boneslist[i]; i++) {
    hearse_bonesfile_t *bonesfile = functable.load_bones(boneslist[i]);
    int ret = 1;
    
    if (timestamp < bonesfile->timestamp) {
      ret = _hearse_upload_one_bonesfile(bonesfile);
    }

    hearse_bonesfile_destroy(bonesfile);

    if (ret == 0) {
      if (functable.should_delete_bones()) {
        functable.delete_bones(boneslist[i]);
      }

      count++;
    } else if (ret < 0) {
      break;
    }
  }

  _hearse_free_boneslist(boneslist);

  return count;
}

int _hearse_download_one_bonesfile(int force)
{
  http_request_t *request = _hearse_new_request(HTTP_REQUEST_GET, "download");
  http_response_t *response;
  char *bones_str = _hearse_build_bones_string();
  hearse_bonesfile_t bonesfile;

  int retval = 0;
  int tmp;
  char crc[33];
  char *dl_crc;

  if (*bones_str) {
    http_request_add_header(request, "X_USERLEVELS", bones_str);
  }

  if (force) {
    http_request_add_header(request, "X_FORCEDOWNLOAD", "Y");
  }
  
  response = http_request_perform(request);
  tmp = _hearse_handle_any_error(response);

  if (tmp < 0) {
    retval = -1;

    goto DONE;
  } else if (tmp > 0) {
    goto DONE;
  }

  functable.compute_crc(response->content, response->content_length, crc);
  dl_crc = http_response_get_header(response, "X_BONESCRC");

  if (strcasecmp(crc, dl_crc) == 0) {
    bonesfile.name = http_response_get_header(response, "X_FILENAME");
    bonesfile.nickname = http_response_get_header(response, "X_USERNICK");
    bonesfile.bytes = response->content;
    bonesfile.length = response->content_length;

    functable.save_bones(&bonesfile);

    retval = 1;
  } else {
    retval = -1;
  }

DONE:
  http_request_destroy(request);
  http_response_destroy(response);
  
  return retval;
}

int _hearse_download_bones(int force)
{
  int ret;
  int count = 0;

  while (1) {
    ret = _hearse_download_one_bonesfile(force);

    if (ret > 0) {
      count++;
    } else if (ret <= 0) {
      break;
    }

    if (force && (count >= functable.get_extra_bones_max())) {
      ret = 0;

      break;
    }
  }

  return ret;
}

int hearse_register()
{
  if ((functable.get_token() == NULL) && (_hearse_new_user() < 0)) {
    return -1;
  }

  return 0;
}

int hearse_run()
{
  int uploaded;

  if (hearse_register() < 0) {
    return -1;
  }

  uploaded = _hearse_upload_bones();

  if (uploaded || (functable.get_numbones() <= functable.get_extra_bones_threshold())) {
    _hearse_download_bones(! uploaded);
  }

  if (uploaded) {
    functable.set_timestamp(time(NULL));
  }

  return 0;
}

hearse_bonesfile_t *hearse_bonesfile_new(char *name, char *nickname, int timestamp, int length, char *bytes)
{
  static char _name[256];
  static char _nickname[256];
  static hearse_bonesfile_t _bonesfile = { .name = _name, .nickname = _nickname };

  strcpy(_bonesfile.name, name);
  strcpy(_bonesfile.nickname, nickname);

  _bonesfile.timestamp = timestamp;
  _bonesfile.length = length;
  _bonesfile.bytes = bytes;

  return &_bonesfile;
}

void hearse_bonesfile_destroy(hearse_bonesfile_t *bonesfile)
{
  free(bonesfile->bytes);
}
