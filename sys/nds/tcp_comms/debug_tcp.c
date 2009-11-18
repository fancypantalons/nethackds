/*
 * $Id: debug_tcp.c,v 1.2 2006/10/10 12:12:16 ben Exp $
 */
/** \file
 * \brief The debug stub TCP network comms.
 */
#include <nds.h>

#include <stdlib.h>
#include <stdint.h>

#include <dswifi9.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <debug_stub.h>
#include "debug_tcp.h"

#ifdef DO_LOGGING
#define LOG( fmt, ...) logFn_debug( fmt, ##__VA_ARGS__)
#else
#define LOG( fmt, ...)
#endif

/** The Listening socket */
static int listen_sock;

/** The socket connected to GDB */
static int gdb_socket;

/*
 * The initialisation function
 */
static int
init_fn( void *data __attribute__((unused))) {
  struct tcp_debug_comms_init_data *init_data =
    (struct tcp_debug_comms_init_data *)data;
  int success_flag = 0;

  if ( init_data == NULL) {
    LOG("Have not been given initialisation data.\n");
    return 0;
  }

  /* create the listening socket and wait for a connection */
  struct sockaddr_in sain;

  sain.sin_addr.s_addr = 0;
  sain.sin_family = AF_INET;
  sain.sin_port = htons( init_data->port);
  listen_sock = socket( AF_INET,SOCK_STREAM, 0);
  bind( listen_sock, (struct sockaddr *)&sain, sizeof(sain));

  int temp_flag = 1;
  ioctl( listen_sock, FIONBIO, &temp_flag);
  listen( listen_sock, 2);

  LOG( "Waiting for GDB to connect\n");
  do {
    int addr_size = sizeof( sain);
    gdb_socket = accept( listen_sock, (struct sockaddr *)&sain, &addr_size);
  } while ( gdb_socket == -1);

  LOG( "GDB connected\n");
  success_flag = 1;

  return success_flag;
}



static void
writeByte_fn( uint8_t byte) {
  //LOG("Sending byte %02x %c\n", byte, byte);
  send( gdb_socket, &byte, 1, 0);
}


static void
writeData_fn( uint8_t *buffer, uint32_t count) {
  //LOG("Sending byte %02x %c\n", byte, byte);
  send( gdb_socket, buffer, count, 0);
}

static int
readByte_fn( uint8_t *read_byte) {
  int read_good = 0;
  int read_len;

  read_len = recv( gdb_socket, read_byte, 1, 0);

  if ( read_len == 1) {
    //LOG("TCP DEBUG read byte %02x %c\n", *read_byte, *read_byte);
    read_good = 1;
  }

  return read_good;
}


static void
poll_fn( void) {
  /* The TCP socket is interrupt driven */
}


static uint32_t
irqs_fn( void) {
  /* Timer and Fifo interrupts are needed */
  return IRQ_TIMER3;
}


/** The instance of the comms function interface */
struct comms_fn_iface_debug tcpCommsIf_debug = {
  .init_fn = init_fn,

  .readByte_fn = readByte_fn,

  .writeByte_fn = writeByte_fn,

  .writeData_fn = writeData_fn,

  .poll_fn = poll_fn,

  .get_IRQs = irqs_fn
};

