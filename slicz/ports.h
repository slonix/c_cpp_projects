/*
 * Author: Konrad Słoniewski
 * Date:   18 August 2013
 */

#ifndef _PORTS_H
#define _PORTS_H

#include <arpa/inet.h>
#include <event2/event.h>
#include <event2/util.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

#include "help_functions.h"
#include "err.h"

/* Definitions */
#define ACTIVE 1             /* port is not configured */
#define INACTIVE 0           /* port is configured */
#define MAX_SOCKETS 100      /* maximum number of ports */

/* Events data */
struct event_base* base;
struct event* listener_socket_event;

/* Definitions of structures */
struct vlan_node {
  int number;                /* VLAN number */
  struct vlan_node *next;    /* next VLAN in list */
};

struct port_node {
  int number;                /* port number */
  int status;                /* ACTIVE or INACTIVE */
  unsigned long sender_addr; /* client address */
  int sender_port;           /* client port */
  int untagged_vlan;         /* tagged or untagged */
  struct vlan_node *vlans;   /* attached VLANs */
  struct port_node *next;    /* next port node */
};

/* Definitions of types */
typedef struct port_node port_t;
typedef struct vlan_node vlan_t;

/* Global data tables */
evutil_socket_t sockets[MAX_SOCKETS];
int ports[MAX_SOCKETS];
struct event* events[MAX_SOCKETS];
int udp_sent[MAX_SOCKETS];
int udp_recv[MAX_SOCKETS];
int udp_errs[MAX_SOCKETS];

/* Functions */
port_t* get_port(int number);
port_t* parse_port(const char* raw);
void del_port(int number);
void del_vlans(port_t* port);
void free_array(char** array, int limit);
port_t* create_port(int number);
int check_tagging(char** string);
void add_vlan(port_t* port, int number);
void add_untagged_vlan(port_t* port, int number);
void activate_port(port_t* port, unsigned long sender_addr, int sender_port);
unsigned long extract_addr(const char* addr);
int init_socket(int port_num);
void init_arrays();
port_t* get_head();
void print_config(port_t* port, char** buffer);
void print_vlans(port_t* port, char** buffer);
void clean_ports();
int valid_vlan(port_t* port, int number);

#endif
