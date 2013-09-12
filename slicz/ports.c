/*
 * Author: Konrad Słoniewski
 * Date:   18 August 2013
 */

#include "ports.h"


static port_t *head;

/* Returns a port with a given number */
port_t* get_port(int number) {
  port_t* node = head;
  while (node != NULL && node->number != number)
    node = node->next;
  
  return node;
}


port_t* parse_port(const char* raw) {
  char** data;
  int number;
  port_t* port;
  int vlan_count;
  char **vlan_list;
  int i;
  int tagged;
  char** sender_data;

  data = split(raw, "/", 3);
  number = atoi(data[0]); 
  
  /* Checking if VLAN list is empty */
  if (!strcmp(data[2], "")) {
    fprintf(stderr, "No VLANs provided, deleting port %d\n", number);
    del_port(number);
    free_array(data, 3);
    return NULL;
  }

  /* VLAN list is not empty - creating port */
  port = create_port(number);

  /* Checking if port already exist */
  if (port == NULL) {
    fprintf(stderr, "Port %d already exist.\n", number);
    free_array(data, 3);
    return NULL;
  }
 
  /* Port does not exist - adding VLANs */ 
  vlan_count = count_occurrences(data[2], ',') + 1;
  vlan_list = split(data[2], ",", vlan_count);

  for (i = 0; i < vlan_count; i += 1) {
    char *vlan = strdup(vlan_list[i]);
    tagged = check_tagging(&vlan);
    if (!tagged) {
      add_untagged_vlan(port, atoi(vlan));
    }
    add_vlan(port, atoi(vlan));
    free(vlan);
  }

  free_array(vlan_list, vlan_count);
  
  if (strlen(data[1]) == 0) { /* No client on port */
    port->status = INACTIVE;
  } else {                    /* Client on port provided */
    sender_data = split(data[1], ":", 2);
    activate_port(port, extract_addr(sender_data[0]), atoi(sender_data[1]));
    free_array(sender_data, 2);
  }
  
  free_array(data, 3);
  
  return port;
}


void del_port(int number) {
  port_t *node = head;
  port_t *node_guard = NULL;
 
  while (node != NULL && node->number != number) {
    node_guard = node;
    node = node->next;
  }
  if (node != NULL && node_guard != NULL) { /* Port found - deleting */
    node_guard->next = node->next;
    del_vlans(node);
    free(node);
  } else if (node != NULL) { /* Port found on head */
    head = head->next;
    del_vlans(node);
    free(node);
  }  
}


void del_vlans(port_t *port) {
  vlan_t *vlans = port->vlans;
  vlan_t *tmp;

  while (vlans != NULL) {
    tmp = vlans;
    vlans = vlans->next;
    free(tmp);
  }
}


void free_array(char** array, int limit) {
  int i;

  for (i = 0; i < limit; i += 1)
    free(array[i]);
  free(array);
}


port_t* create_port(int number) {
  port_t *node = head;
  port_t *tmp = NULL;
  port_t *new_node = malloc(sizeof(port_t));

  /* Creating port */
  new_node->number = number;
  new_node->status = INACTIVE;
  new_node->untagged_vlan = -1; /* not tagged */
  new_node->vlans = NULL;

  /* Checking if port already exist */
  if (get_port(number) != NULL)
    return NULL;
  node = head;

  /* Adding port */
  while (node != NULL && node->number < number) {
    tmp = node;
    node = node->next;
  }
  
  if (tmp != NULL)
    tmp->next = new_node;
  else
    head = new_node;
   
  new_node->next = node;
  fprintf(stderr, "New port: %d\n", new_node->number);
 
  return new_node;
}


int check_tagging(char** string) {
  int len = strlen(*string);
  if ((*string)[len-1] == 't') {
    (*string)[len-1] = '\0';
    return 1;
  }
  return 0;
}


void add_untagged_vlan(port_t *port, int number) {
  if (port->untagged_vlan == -1)
    port->untagged_vlan = number;
  else
    fprintf(stderr,
      "Untagged VLAN %d already exist at port %d. Auto tagging.\n",
      port->untagged_vlan, port->number);
}


void add_vlan(port_t* port, int number) {
  vlan_t* vlans = port->vlans;
  vlan_t* node = vlans;
  vlan_t* tmp = NULL;
  vlan_t* new_vlan = malloc(sizeof(vlan_t));

  /* Creating new VLAN */
  new_vlan->number = number;
 
  /* Adding VLAN */
  while (node != NULL && node->number < number) {
    tmp = node;
    node = node->next;
  }
  
  if (tmp != NULL)
    tmp->next = new_vlan;
  else
    port->vlans = new_vlan;

  new_vlan->next = node;
}


void activate_port(port_t* port, unsigned long sender_addr, int sender_port) {
  port->status = ACTIVE;
  port->sender_addr = sender_addr;
  port->sender_port = sender_port;
}


unsigned long extract_addr(const char* addr) {
  struct addrinfo addr_hints;
  struct addrinfo *addr_result;
  unsigned long s_addr;

  memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_family = AF_INET;
  addr_hints.ai_protocol = IPPROTO_TCP;
  getaddrinfo(addr, "0", &addr_hints, &addr_result);
  s_addr = ((struct sockaddr_in*) addr_result->ai_addr)->sin_addr.s_addr;
  freeaddrinfo(addr_result);

  return s_addr;
}


/* Socket initialization, returns socket index in socket array */
int init_socket(int port_num) {
  int i;
  struct sockaddr_in sin;

  i = 0;
  while (i < 100 && sockets[i] != -1)
    i += 1;

  /* Checking for a space for a new socket */
  if (i >= 100)
    return -1;

  /* There is a free space for a new socket */
  sockets[i] = socket(PF_INET, SOCK_DGRAM, 0);
  ports[i] = port_num;

  if (sockets[i] == -1 ||
      evutil_make_listen_socket_reuseable(sockets[i]) ||
      evutil_make_socket_nonblocking(sockets[i])) {
    syserr("Creating socket.");
  }

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port_num);
  if(bind(sockets[i], (struct sockaddr*) &sin, sizeof(sin)) == -1)
    syserr("Binding socket.");
  
  return i;
}


void init_arrays() {
  int i;
  
  for(i = 0; i < MAX_SOCKETS; i += 1) {
    sockets[i] = -1;
    ports[i] = -1;
    events[i] = NULL;
    udp_sent[i] = 0;
    udp_recv[i] = 0;
    udp_errs[i] = 0;
  }
}


/* Returns current head of ports list */
static port_t* head;
port_t* get_head() {
  return head;
}


/* Prints config of a given port 
 * 
 * @param port - given port
 * @param buffer - buffer to print configuration in
 */
void print_config(port_t* port, char** buffer) {
  struct in_addr sin_addr;
  char addr[INET_ADDRSTRLEN];
  char* vlan_buffer = malloc(1024*sizeof(char));
  /* memset(vlan_buffer, 0, sizeof(vlan_buffer));*/
  sin_addr.s_addr = port->sender_addr;
  inet_ntop(AF_INET, &sin_addr, addr, INET_ADDRSTRLEN);
  print_vlans(port, &vlan_buffer);
  if (port->status == ACTIVE) {
    sprintf(*buffer, "%d/%s:%d/%s", port->number, addr, port->sender_port,
      vlan_buffer);
  } else {
    sprintf(*buffer, "%d//%s", port->number, vlan_buffer);
  }
  free(vlan_buffer);
}


/* Prints VLANs of a given port 
 * 
 * @param port - given port
 * @param buffer - buffer to print configuration in
 */
void print_vlans(port_t* port, char** buffer) {
  int offset, untagged;
  vlan_t* vlan = port->vlans;
  char* string_node = *buffer;

  /* Check if port is untegged */
  untagged = port->untagged_vlan;

  offset = 0;
  while (vlan != NULL) {
    if (vlan->number == untagged) {
      offset += sprintf(string_node + offset, "%d,", vlan->number);
    } else {
      offset += sprintf(string_node + offset, "%dt,", vlan->number);
    }
    vlan = vlan->next;
  }
  (*buffer)[strlen(*buffer) - 1] = '\0';
}


void clean_ports() {
  while (head != NULL) {
    del_port(head->number);
  }
}


int valid_vlan(port_t* port, int number) {
  vlan_t* vlans = port->vlans;
  vlan_t* node = vlans;
  while (node != NULL && node->number != number) {
    node = node->next;
  }
  return (node != NULL) ? 1 : 0;
}
