/*
 * Author: Konrad Słoniewski
 * Date:   18 August 2013
 */

#ifndef _CONTROL_H
#define _CONTROL_H

#include <arpa/inet.h>
#include <errno.h>
#include <event2/event.h>
#include <event2/util.h>
#include <netinet/in.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>    /* ether_addr */
#include <sys/socket.h>

#include "help_functions.h"
#include "ports.h"
#include "macs.h"
#include "err.h"


/* Definitions */
#define MAX_CONTROL_CONNECTIONS 10
#define BUF_SIZE 1518

/* Structures */
struct connection_description {
  struct sockaddr_in address;     /* client address */
  evutil_socket_t sock;           /* switch port */
  struct event *ev;               /* assigned event */
};

struct connection_description clients[MAX_CONTROL_CONNECTIONS];

/* Global data tables */
struct event* events[MAX_SOCKETS];
int udp_sent[MAX_SOCKETS];
int udp_recv[MAX_SOCKETS];
int udp_errs[MAX_SOCKETS];

/* Functions */
void init_clients();
struct connection_description *get_client_slot();
void client_manage(evutil_socket_t sock, short ev, void *arg);
void listener_manage(evutil_socket_t sock, short ev, void *arg);
void handle_sigint(int signal);
void set_config(evutil_socket_t sock, const char* buf);
void get_config(evutil_socket_t sock);
void counters(evutil_socket_t sock);
void start_event(int index, struct event_base* base, void (*func)
  (evutil_socket_t sock, short ev, void* arg));
void delete_event(int index);
int get_index(int port);
void udp_manage(evutil_socket_t sock, short ev, void *arg);
void tag_frame(char* buffer, char* dst_buffer, int vlan);
void untag_frame(char* buffer, char* dst_buffer);

#endif
