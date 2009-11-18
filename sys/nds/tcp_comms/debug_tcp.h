#ifndef _DEBUG_TCP_H_
#define _DEBUG_TCP_H_ 1
/*
 * $Id: debug_tcp.h,v 1.1.1.1 2006/09/06 10:13:19 ben Exp $
 */
/** \file
 */


/**
 */
struct tcp_debug_comms_init_data {
  /** the port number on which to listen for connections */
  uint16_t port;
};


/**
 */
extern struct comms_fn_iface_debug tcpCommsIf_debug;

#endif /* End of _DEBUG_TCP_H_ */
