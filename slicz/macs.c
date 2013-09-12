/*
 * Author: Konrad Słoniewski
 * Date:   18 August 2013
 */

#include "macs.h"

/* Attributes */

static int capacity = 0;
static mac_t *head = NULL;
static mac_t guard;
static mac_t* iterator;


/* Functions */

void delete_first_mac() {
  mac_t *trash;
  if (head != NULL) {
    trash = head;
    head = head->next;
    free(trash);
    capacity -= 1;
  }
}


void clean_mac_map() {
  while(head != NULL) {
    delete_first_mac();
  }
}


int add_mac(struct ether_addr mac, int vlan, int port, int is_tagged) {
  mac_t* node;

  /* Check if already exists */
  if (get_port_from_mac(mac, vlan) != INACTIVE_PORT) {
    return -1;
  }

  /* Checking max value */
  if (capacity >= MAC_MAX_CAP) {
    delete_first_mac();
  }

  fprintf(stderr, "Added: %x-%x-%x-%x-%x-%x vlan %d a port %d\n",
          mac.ether_addr_octet[0], mac.ether_addr_octet[1],
          mac.ether_addr_octet[0], mac.ether_addr_octet[1],
          mac.ether_addr_octet[0], mac.ether_addr_octet[1], vlan, port);

  node = malloc(sizeof(mac_t));
  node->mac = mac;
  node->vlan = vlan;
  node->is_tagged = is_tagged;
  node->port = port;
  node->next = head;
  head = node;
  capacity += 1;
  return 1;
}


int get_port_from_mac(struct ether_addr mac, int vlan) {
  mac_t* node;
  int port;

  node = head;
  port = INACTIVE_PORT;
  while (node != NULL && port == INACTIVE_PORT) {
    if (node->vlan == vlan && compare_macs(node->mac, mac)) {
      port = node->port;
    }
    node = node->next;
  }

  return port;
}


int compare_macs(struct ether_addr mac1, struct ether_addr mac2) {
  int i, cmp;
  i = 0;
  cmp = 1;
  for (i = 0; i < 6; i += 1) {
    if (mac1.ether_addr_octet[i] != mac2.ether_addr_octet[i]) {
      cmp = 0;
    }
  }
  return cmp; 
}


int get_untagged_port_from_mac(struct ether_addr mac) {
  mac_t* node;
  int port;
  node = head;
  port = INACTIVE_PORT;
  while (node != NULL && port == INACTIVE_PORT) {
    if (!(node->is_tagged) && compare_macs(node->mac, mac)) {
      port = node->port;
    }
    node = node->next;
  }

  return port;
}


void reset_vlan_iterator(){
  guard.vlan = -1;
  guard.next = head;
  iterator = &guard;
}


int vlan_next_port(int vlan){
  if (iterator != NULL)
    iterator = iterator->next;

  while (iterator != NULL && iterator->vlan != vlan)
    iterator = iterator->next;
  if (iterator == NULL)
    return -1;
  else
    return iterator->port;
}
