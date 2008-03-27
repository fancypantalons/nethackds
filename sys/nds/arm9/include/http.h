#ifndef _HTTP_H_
#define _HTTP_H_

#define HTTP_USER_AGENT "libhttp/1.0"

typedef enum
{
  HTTP_REQUEST_GET = 1,
  HTTP_REQUEST_POST = 2
} http_request_type_t; 

typedef enum
{
  HTTP_RESPONSE_OK = 200
} http_response_type_t;

typedef struct
{
  char *name;
  char *value;
} http_header_t;

typedef struct
{
  char *name;
  char *value;
} http_param_t;

typedef struct
{
  http_header_t *headers;
  int num_headers;
} http_headers_t;

typedef struct
{
  http_param_t *params;
  int num_params;
} http_params_t;

typedef struct 
{
  http_request_type_t request_type;
  http_headers_t headers;
  http_params_t params;

  char *hostname;
  unsigned short port;
  char *path;

  char *content;
  char *content_type;
  int content_length;
} http_request_t;

typedef struct
{
  http_response_type_t response_type;
  http_headers_t headers;

  char *content;
  char *content_type;
  int content_length;
} http_response_t;

http_request_t *http_request_new(http_request_type_t request_type, char *hostname, unsigned short port, char *path);
int http_request_destroy(http_request_t *request);
int http_request_add_header(http_request_t *request, char *name, char *value);
int http_request_add_param(http_request_t *request, char *name, char *value);

http_response_t *http_response_new();
int http_response_add_header(http_response_t *response, char *name, char *value);
char *http_response_get_header(http_response_t *response, char *name);
int http_response_destroy(http_response_t *response);

http_response_t *http_request_perform(http_request_t *request);

#endif
