/*
 * Author: Konrad Słoniewski
 * Date:   18 August 2013
 */

#ifndef _MACS_H
#define _MACS_H
                             /* Example of usage: */
#include <net/ethernet.h>    /* struct ether_addr */
#include <stdlib.h>          /* free() */
#include <stdio.h>           /* fprintf() */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

/* Definitions */
#define INACTIVE_PORT -1
#define MAC_MAX_CAP 4096

/* Structs */
struct mac_node {
  struct ether_addr mac;
  int vlan;
  int port;
  int is_tagged;
  struct mac_node *next;
};

/* used for tests */
struct simple_frame {
  u_char dst_mac[ETHER_ADDR_LEN];
  u_char src_mac[ETHER_ADDR_LEN];
  uint16_t tpid;
  uint16_t pcp_dei;
  uint16_t ether_type;
};

/* Types */
typedef struct mac_node mac_t;

/* Functions */
void delete_first_mac();
void clean_mac_map();
int add_mac(struct ether_addr mac, int vlan, int port, int is_tagged);
int get_port_from_mac(struct ether_addr mac, int vlan);
int compare_macs(struct ether_addr mac1, struct ether_addr mac2);
int get_untagged_port_from_mac(struct ether_addr mac);
void reset_vlan_iterator();
int vlan_next_port(int vlan);

#endif
