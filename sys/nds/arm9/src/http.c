#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include "http.h"

#define NULLFREE(ptr) ((ptr != NULL) ? free(ptr) : 0);
#define CLOSE(fd) close(fd)

/*
 * Some basic strict functions.
 */

int _http_split(char *str, char *delim, char **parts, int limit)
{
  int count;

  count = 0;

  while (1) {
    char *ptr = NULL;
    int i;

    if ((limit > 0) && (count >= limit - 1)) {
      break;
    }

    for (i = 0; delim[i]; i++) {
      char *tmp = index(str, delim[i]);

      if (tmp == NULL) {
        continue;
      }

      if ((ptr == NULL) || (tmp < ptr)) {
        ptr = tmp;
      }
    }

    if (ptr == NULL) {
      break;
    }

    parts[(count)++] = str;

    *ptr = '\0';
    str = ptr + 1;
  }

  parts[(count)++] = str;

  return count;
}

char *_http_strip_leading_spaces(char *str)
{
  while (isspace(*str)) {
    str++;
  }

  return str;
}

/*
 * A little abstraction over the raw socket layer.
 */

typedef struct
{
  int fd;
  char buffer[256];
  int buflen;
  int readlen;
  int pos;
} _http_socket;

_http_socket *_http_socket_connect(http_request_t *request)
{
  static _http_socket _the_socket;

  int fd;
  struct hostent *host;
  struct sockaddr_in addr;
  long ip;

  memset(&_the_socket, 0, sizeof(_the_socket));

  host = gethostbyname(request->hostname);

  if (host == NULL) {
    return NULL;
  }

  fd = socket(AF_INET, SOCK_STREAM, 0);

  ip = *((unsigned long *)(host->h_addr_list[0]));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ip;
  addr.sin_port = htons(request->port);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr))) {
    CLOSE(fd);

    return NULL;
  }

  _the_socket.fd = fd;
  _the_socket.buflen = sizeof(_the_socket.buffer);

  return &_the_socket;
}

int _http_socket_destroy(_http_socket *socket)
{
  closesocket(socket->fd);
  
  return 0;
}

int _http_socket_send(_http_socket *socket, char *format, ...)
{
  va_list args;
  char buffer[2048];
  int len;
  int ret;

  va_start(args, format);

  len = vsprintf(buffer, format, args);
  //iprintf(buffer);
  ret = send(socket->fd, buffer, len, 0);

  va_end(args);

  return ret;
}

char _http_socket_get_char(_http_socket *socket)
{
  if (socket->pos >= socket->readlen)
  {
    socket->readlen = recv(socket->fd, socket->buffer, socket->buflen, 0);
    socket->pos = 0;

    swiWaitForVBlank();
    swiWaitForVBlank();
  }

  if (socket->readlen <= 0) {
    return -1;
  } else {
    return socket->buffer[socket->pos++];
  }
}

char *_http_socket_read(_http_socket *socket, int length)
{
  char *buffer = (char *)calloc(length + 1, 1);
  int i;

  for (i = 0; i < length; i++) {
    buffer[i] = _http_socket_get_char(socket);
  }

  return buffer;
}

char *_http_socket_read_until_end(_http_socket *socket)
{
  int buflen = 10;
  int bufpos = 0;
  char *buffer = (char *)malloc(buflen);
  int c;

  while ((c = _http_socket_get_char(socket)) >= 0)
  {
    if (bufpos >= buflen) {
      char *tmp = (char *)realloc(buffer, buflen + 10);

      if (tmp == NULL) {
        free(buffer);

        return NULL;
      }

      buffer = tmp;
      buflen += 10;
    }

    buffer[bufpos++] = c;
  }

  return buffer;
}

char *_http_socket_read_line(_http_socket *socket)
{
  char *result = (char *)malloc(1);
  int pos = 0;
  int buflen = 1;

  result[0] = '\0';

  while (1) {
    int c = _http_socket_get_char(socket);

    if (c == '\r') {
      continue;
    }

    if ((c == '\n') || (c < 0)) {
      break;
    }

    if (pos >= (buflen - 1)) {
      char *tmp = (char *)realloc(result, buflen + 10);

      if (tmp != NULL) {
        result = tmp;
        buflen += 10;
      } else {
        free(result);

        return NULL;
      }
    }

    result[pos++] = c;
    result[pos] = '\0';
  }

  return result;
}

/*
 * Now, functions for manipulating headers and parameters.
 */

int http_headers_add(http_headers_t *headers, char *name, char *value)
{
  http_header_t *tmp = (http_header_t *)realloc(headers->headers, (headers->num_headers + 1) * sizeof(http_header_t));

  if (tmp != NULL) {
    headers->headers = tmp;

    headers->headers[headers->num_headers].name = strdup(name);
    headers->headers[headers->num_headers].value = strdup(value);

    headers->num_headers++;

    return 0;
  } else {
    return -1;
  }
}

int http_headers_destroy(http_headers_t *headers)
{
  int i;

  for (i = 0; i < headers->num_headers; i++) {
    free(headers->headers[i].name);
    free(headers->headers[i].value);
  }

  free(headers->headers);

  return 0;
}

int http_params_add(http_params_t *params, char *name, char *value)
{
  http_param_t *tmp = (http_param_t *)realloc(params->params, (params->num_params + 1) * sizeof(http_param_t));

  if (tmp != NULL) {
    params->params = tmp;

    params->params[params->num_params].name = strdup(name);
    params->params[params->num_params].value = strdup(value);

    params->num_params++;

    return 0;
  } else {
    return -1;
  }
}

int http_params_destroy(http_params_t *params)
{
  int i;

  for (i = 0; i < params->num_params; i++) {
    free(params->params[i].name);
    free(params->params[i].value);
  }

  free(params->params);

  return 0;
}

/*
 * And finally the primary interface for creating requests and executing
 * them.
 */

http_request_t *http_request_new(http_request_type_t request_type, char *hostname, unsigned short port, char *path)
{
  static char _hostname[256];
  static char _path[256];
  static http_request_t _request;

  memset(&_request, 0, sizeof(_request));

  strcpy(_hostname, hostname);
  strcpy(_path, path);

  _request.request_type = request_type;

  _request.hostname = _hostname;
  _request.port = port;
  _request.path = _path;
  
  return &_request;
}

int http_request_destroy(http_request_t *request)
{
  http_headers_destroy(&(request->headers));
  http_params_destroy(&(request->params));

  return 0;
}

int http_request_add_header(http_request_t *request, char *name, char *value)
{
  return http_headers_add(&(request->headers), name, value);
}

int http_request_add_param(http_request_t *request, char *name, char *value)
{
  return http_params_add(&(request->params), name, value);
}

http_response_t *http_response_new()
{
  static http_response_t _response;

  memset(&_response, 0, sizeof(_response));

  _response.content_length = -1;

  return &_response;
}

int http_response_add_header(http_response_t *response, char *name, char *value)
{
  return http_headers_add(&(response->headers), name, value);
}

int http_response_destroy(http_response_t *response)
{
  http_headers_destroy(&(response->headers));

  NULLFREE(response->content);
  NULLFREE(response->content_type);

  return 0;
}

char *http_response_get_header(http_response_t *response, char *name)
{
  int i;

  for (i = 0; i < response->headers.num_headers; i++) {
    if (strcasecmp(response->headers.headers[i].name, name) == 0) {
      return response->headers.headers[i].value;
    }
  }

  return NULL;
}

int _http_build_params(http_request_t *request, char *buffer, int buflen)
{
  int i;

  strcpy(buffer, request->path);

  for (i = 0; i < request->params.num_params; i++) {
    if (i == 0) {
      strcat(buffer, "?");
    } else {
      strcat(buffer, "&");
    }

    strcat(buffer, request->params.params[i].name);
    strcat(buffer, "=");
    strcat(buffer, request->params.params[i].value);
  }

  return 0;
}

int _http_send_headers(http_request_t *request, _http_socket *socket)
{
  int i;

  _http_socket_send(socket, "User-Agent: %s\n", HTTP_USER_AGENT);
  _http_socket_send(socket, "Host: %s:%d\n", request->hostname, request->port);
  _http_socket_send(socket, "Accept: */*\n");
  _http_socket_send(socket, "Connection: close\n");

  if (request->content != NULL) {
    _http_socket_send(socket, "Content-Length: %d\n", request->content_length);
    _http_socket_send(socket, "Content-Type: %s\n", request->content_type);
  }

  for (i = 0; i < request->headers.num_headers; i++) {
    _http_socket_send(socket, "%s: %s\n", request->headers.headers[i].name, request->headers.headers[i].value);
  }

  return 0;
}

#define SEND_BLOCK_SIZE 256

int _http_send_request(http_request_t *request, _http_socket *socket)
{
  char buffer[2048];

  _http_build_params(request, buffer, sizeof(buffer));
  _http_socket_send(socket, "%s %s HTTP/1.1\r\n", 
             (request->request_type == HTTP_REQUEST_GET) ? "GET" : "POST",
             buffer);
  _http_send_headers(request, socket);
  _http_socket_send(socket, "\r\n");

  if (request->content != NULL) {
    char *tmp = request->content;
    char *end = request->content + request->content_length;

    while (tmp < end) {
      int left = end - tmp;
      int size = (left < 256) ? left : 256;
      int count;
      
      count = send(socket->fd, tmp, size, 0);

      tmp += count;

      swiWaitForVBlank();
      swiWaitForVBlank();
    }
  }

  return 0;
}

int _http_parse_headers(http_response_t *response, _http_socket *socket)
{
  while (1)
  {
    char *line = _http_socket_read_line(socket);
    char *parts[2];
    int count;

    if (*line == '\0') {
      free(line);

      break;
    }

    count = _http_split(line, ":", parts, 2);

    if (count == 2) {
      //iprintf("%s -> %s\n", parts[0], parts[1]);

      if (strcasecmp(parts[0], "Content-Length") == 0) {
        sscanf(parts[1], "%d", &(response->content_length));
      }

      http_response_add_header(response, parts[0], _http_strip_leading_spaces(parts[1]));
    }

    free(line);
  }

  return 0;
}

int _http_parse_response(http_response_t *response, _http_socket *socket)
{
  int ret = 0;
  char *response_line = _http_socket_read_line(socket);

  int count;
  char *response_parts[3];
  
  count = _http_split(response_line, " ", response_parts, 3);
  
  if (count >= 2) {
    sscanf(response_parts[1], "%d", (int *)&(response->response_type));

    _http_parse_headers(response, socket);

    if (response->content_length >= 0) {
      response->content = _http_socket_read(socket, response->content_length);
    } else {
      response->content = _http_socket_read_until_end(socket);
    }
  } else {
    ret = -1;
  }

  free(response_line);

  return ret;
}

http_response_t *http_request_perform(http_request_t *request)
{
  _http_socket *socket;
  http_response_t *response;

  socket = _http_socket_connect(request);

  if (socket == NULL) {
    return NULL;
  }

  response = http_response_new();

  _http_send_request(request, socket);
  _http_parse_response(response, socket);

  _http_socket_destroy(socket);

  return response;
}
