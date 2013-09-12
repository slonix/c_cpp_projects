/*
 * Author: Konrad Słoniewski
 * Date:   18 August 2013
 */

                           /* Example of usage: */
#include <arpa/inet.h>     /* inet_ntoa */
#include <errno.h>         /* errno */
#include <event2/event.h>  /* event handlers library */
#include <event2/util.h>   /* event handler library */
#include <netinet/in.h>    /* struct sockaddr_in */
#include <signal.h>        /* signal */
#include <stdio.h>         /* printf, fprintf */
#include <stdlib.h>        /* atoi */
#include <string.h>        /* memset */
#include <unistd.h>        /* getopt */

#include "err.h"           /* syserr, fatal */
#include "control.h"
#include "ports.h"         /* port_t type, clean_ports() */
#include "macs.h"          /* clean_mac_map() */ 


/*****************************************************************************
 *                               MAIN PROGRAM                                *
 *****************************************************************************/
int main(int argc, char *argv[]) {
  int c;                            /* used as getopt return */
  int console_port;                 /* switch control port */
  evutil_socket_t listener_socket;  /* socket for TCP control service client */
  struct sockaddr_in listener_addr; /* addres of client on console service */
  int sock;                         /* server socket */
  port_t *port;
  opterr = 0;

  /* SIG_INT handle registration */
  printf("LOADING: SIGINT registration.\n");
  if (signal(SIGINT, handle_sigint) == SIG_ERR)
    syserr("Signal handler overwrite.");

  /* Creating new base event */
  base = event_base_new();
  if (!base)
    syserr("Creating new base event.");

  /* Initialize data structures */
  init_arrays();

  /* Setting default console port */
  console_port = 42420;

  /* Reading arguments */
  printf("LOADING: Reading arguments.\n");
  while ((c = getopt(argc, argv, "c:p:")) != -1) {
    switch (c)
    {
      case 'c':
        console_port = atoi(optarg);
        if (console_port == 0) {
          fatal("Wrong port number.");
        } else {
          fprintf(stderr, "Port number: %d.\n", console_port);
        }
        break;
      case 'p':
        fprintf(stderr, "%s\n", optarg);
        port = parse_port(optarg);
        sock = init_socket(port->number);
        fprintf(stderr, "%d\n", sock);
        ports[sock] = port->number;
        start_event(sock, base, udp_manage);
        break;
      default:
        abort();
        /* fatal("Usage: %s -c <parameter1> -p <parameter2>", argv[0]); */
    }
  }

  /* Switch's control service via TCP */
  printf("LOADING: Initialize control service.\n");
  init_clients();
  listener_socket = socket(PF_INET, SOCK_STREAM, 0);
  if (listener_socket == -1)
    syserr("Creating socket.");
  if (evutil_make_listen_socket_reuseable(listener_socket))
    syserr("Making listen socket reuseable.");
  if (evutil_make_socket_nonblocking(listener_socket))
    syserr("Making socket nonblocking.");
  
  listener_addr.sin_family = AF_INET;
  listener_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  listener_addr.sin_port = htons(console_port);
  
  if (bind(listener_socket, (struct sockaddr *) &listener_addr, 
      sizeof(listener_addr)) == -1)
    syserr("Binding socket.");
  
  if (listen(listener_socket, 5) == -1)
    syserr("listen"); 
  
  listener_socket_event = event_new(base, listener_socket, EV_READ|EV_PERSIST, 
    listener_manage, (void *) base);
  if (!listener_socket_event)
    syserr("Error creating event for a listener socket.");

  if (event_add(listener_socket_event, NULL) == -1)
    syserr("Adding new control service event.");

  printf("Waiting for control connections...\n");
  if (event_base_dispatch(base) == -1)
    syserr("Control connections.");
  printf("Control connections closed.\n");
 
  event_free(listener_socket_event); /* ??? */
  event_base_free(base); 

  clean_ports();
  clean_mac_map();
  
  return 0;
}
