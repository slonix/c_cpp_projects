/*
 * Author: Konrad Słoniewski
 * Date:   18 August 2013
 */

#include "control.h"

/* Initializes clients table */
void init_clients() {
  memset(clients, 0, sizeof(clients));
}

/* Searches for first empty control connection slot */
struct connection_description *get_client_slot() {
  int i;
  for (i = 0; i < MAX_CONTROL_CONNECTIONS; i++)
    if (!clients[i].ev)
      return &clients[i];
  return NULL;
}


/* Service control connection */
void client_manage(evutil_socket_t sock, short ev, void *arg) {
  int bytes_read;
  char buf[BUF_SIZE+1];
  struct connection_description *cl;
  int command_count;
  regex_t reg_set, reg_get, reg_count, reg_shut;
  char **commands;
  char *command;
  int i;

  cl = (struct connection_description *) arg;

  /* Checking read */
  bytes_read = read(sock, buf, BUF_SIZE);
  if (bytes_read <= 0) {
    if (bytes_read < 0) {
      fprintf(stderr,
        "Error (%s) while reading data from %s:%d. Closing connection.\n",
        strerror(errno), inet_ntoa(cl->address.sin_addr),
        ntohs(cl->address.sin_port));
    } else {
      fprintf(stderr, "Connection from %s:%d closed.\n",
        inet_ntoa(cl->address.sin_addr), ntohs(cl->address.sin_port));
    }

    /* Ending the event */
    if (event_del(cl->ev) == -1)
      syserr("Deleting the event.");
    event_free(cl->ev);
    if(close(sock) == -1)
      syserr("Closing sock.");
    cl->ev = NULL;

    return;
  }

  /* Sending response */
  buf[bytes_read] = 0;
  write(sock, "OK\n", 3);

  /* REGEX */
  regcomp(&reg_set, "^setconfig", 0);
  regcomp(&reg_get, "^getconfig", 0);
  regcomp(&reg_count, "^counters", 0);
  regcomp(&reg_shut, "^shutdown!", 0);
 
  /* Counting number of commands like "setconfig 1234//1,2t\n getconfig\n" */ 
  command_count = count_occurrences(buf, '\n'); /* +1 ? */
  
  commands = split(buf, "\n", command_count);
  fprintf(stderr, "Received instruction - %s\n", buf);
  fprintf(stderr, "Number of commands: %d\n", command_count);

  /* Processing each line of commands separately */
  for (i = 0; i < command_count; ++i) {
    command = commands[i];
    fprintf(stderr, "Command = %s\n", command);
 
    /* Choosing option */
    if (!regexec(&reg_set, command, 0, NULL, 0)) {
      set_config(sock, command);
    } else if (!regexec(&reg_get, command, 0, NULL, 0)) {
      get_config(sock);
    } else if (!regexec(&reg_shut, command, 0, NULL, 0)) {
      event_del(listener_socket_event);
      event_free(listener_socket_event);

      for (i = 0; i < MAX_SOCKETS; ++i)
        delete_event(i);
    } else if (!regexec(&reg_count, command, 0, NULL, 0)) {
      counters(sock);
    }
    else {
      write(sock, "ERR: Unknown command\n", 21);
    }
  }
  printf("Position END");

  free_array(commands, command_count);

  regfree(&reg_set);
  regfree(&reg_get);
  regfree(&reg_shut);
  regfree(&reg_count);
}


/* Accepting control connections */
void listener_manage(evutil_socket_t sock, short ev, void *arg) {
  struct sockaddr_in sin;
  socklen_t addr_size;
  evutil_socket_t connection_socket;
  struct event_base *base;
  struct connection_description *cl;
  struct event *an_event;

  /* Accepting control user */
  base = (struct event_base *) arg;

  addr_size = sizeof(struct sockaddr_in);
  connection_socket = accept(sock, (struct sockaddr *) &sin, &addr_size);
  
  if (connection_socket == -1)
    syserr("Accepting control connection.");

  cl = get_client_slot();
  if (!cl) {
    close(connection_socket);
    fprintf(stderr, 
      "Ignoring connection attempt from %s:%d due to lack of empty slots.\n",
      inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
    return;
  }

  memcpy(&(cl->address), &sin, sizeof(struct sockaddr_in));
  cl->sock = connection_socket;

  an_event = event_new(base, connection_socket, EV_READ|EV_PERSIST, 
    client_manage, (void *) cl);
  if (!an_event)
    syserr("Creating event for control user");
  cl->ev = an_event;
  if (event_add(an_event, NULL) == -1)
    syserr("Adding event to a base.");

  /* Welcome message */
  write(connection_socket, "SLICZ\n", 6);
}


/* SIGINT handler */
void handle_sigint(int signal) {
  /*
  int i;
  */
  fprintf(stderr, "Signal catched: Switch shutdown initiated.\n");
  /*
  event_del(listener_socket_event);
  event_free(listener_socket_event);
  for (i = 0; i < MAX_SOCKETS; ++i) delete_event(i);
  */
  
  if (raise(SIGQUIT) != 0) {
    syserr("Error inside signal handler.");
  };
  
}


/* Setting configuration by control TCP connection */
void set_config(evutil_socket_t sock, const char * buf) {
  char** tmp;
  int removal;
  int port_number;
  port_t* port;
  int new_sock;
 
  /* Splitting message into parts - switch_port/client_ip:client_port/VLANs */ 
  tmp = split(buf + 10, "/", 3);
  removal = 0;
  if (!strcmp(tmp[2], ""))
    removal = 1;            /* Wrong instruction */

  /* Parsing port */
  port_number = atoi(tmp[0]);
  port = get_port(port_number);

  if (port == NULL) { 
    /* No such port in swich - creating one */
    port = parse_port(buf + 10);
     
    /* Creating new socket */
    new_sock = init_socket(port->number);
    ports[new_sock] = port->number;
    
    /* Starting event for a new socket management */
    start_event(new_sock, base, udp_manage);  
  } else if (!removal) { 
    /* There is a port but new VLAN list is provided */
    del_port(port_number);
    delete_event(get_index(port_number));
    
    port = parse_port(buf + 10);
    new_sock = init_socket(port_number);
    start_event(new_sock, base, udp_manage);
  } else {
    del_port(atoi(tmp[0]));
    delete_event(get_index(atoi(tmp[0])));
  }

  write(sock, "END\n", 4);
  free_array(tmp, 3);
}

void get_config(evutil_socket_t sock) {
  char buffer[BUF_SIZE+1];
  char *buf = buffer;

  port_t *port;
  port = get_head();

  while (port != NULL) {
    memset(buffer, 0, sizeof(buffer));
    print_config(port, &buf);
    write(sock, buf, strlen(buf));
    write(sock, "\n", 1);
    port = port->next;
  }
  write(sock, "END\n", 4);
}

void start_event(int index, struct event_base* base, void (*func) 
    (evutil_socket_t sock, short ev, void* arg)) {
  events[index] = 
    event_new(base, sockets[index], EV_READ|EV_PERSIST, func, 
    (void *) &index);

  if (!events[index])
    syserr("Creating event for a listener socket.");
  if (event_add(events[index], NULL) == -1)
    syserr("Adding UDP socket");
}

void delete_event(int index) {
  if (events[index] == NULL)
    return;

  if (event_del(events[index]) == -1)
    syserr("Can't delete the event");

  event_free(events[index]);
  
  if(close(sockets[index]) == -1) 
    syserr("Error closing socket.");              
  sockets[index] = -1;                                                          
  ports[index] = -1;                                                            
  events[index] = NULL;   
}

int get_index(int port) {
  int i;

  i = 0;
  
  while (i < MAX_SOCKETS && ports[i] != port) {
    i++;
  }

  if (i >= MAX_SOCKETS) {
    i = -1;
  }

  return i;
}

void counters(evutil_socket_t sock) {
  char buffer[BUF_SIZE+1];
  char *buf = buffer;
  int index;
  port_t *port;

  port = get_head();

  while (port != NULL) {
    index = get_index(port -> number);
    memset(buffer, 0, sizeof(buffer));
    sprintf(buf, "%d: recvd:%d sent:%d errs:%d", port->number, udp_recv[index],
      udp_sent[index], udp_errs[index]);
    buf[strlen(buf)] = '\n';
    write(sock, buf, strlen(buf));
    port = port->next;
  }
  write(sock, "END\n", 4);
}


/* Atomic increase of an integer field */
static void atomic_inc(int* field) {
  __sync_val_compare_and_swap(field, *field, *field + 1);
}


/* Forwards a frame (buffer) to sender_addr */
static void forward_frame(struct sockaddr_in sender_addr,
  port_t *forward_port, int addrlen, int r, char *buffer) {
  int fwd_port = forward_port->number;
  sender_addr.sin_addr.s_addr = forward_port->sender_addr;
  sender_addr.sin_port = htons(forward_port->sender_port);
  if (sendto(sockets[get_index(fwd_port)], buffer, r, 0,
     (struct sockaddr *) &sender_addr, addrlen) == -1)
    syserr("UDP send");

  /* increasing counters */
  atomic_inc(udp_sent + get_index(fwd_port));
}


/* Event handler on UDP packet receiving */
void udp_manage(evutil_socket_t sock, short ev, void *arg) {
  int r, index, fwd_port, vlan_number;
  uint16_t *tpid, *pcp_dei, *ether_type;
  char buffer[BUF_SIZE+1];
  char dst_buffer[BUF_SIZE+1];
  socklen_t addrlen;
  struct sockaddr_in sender_addr;
  struct ether_addr *src_addr, *dst_addr;
  port_t *base_port, *forward_port;
  
  index = (int) arg;
  addrlen = sizeof(sender_addr);

  /* Pick maintained port from port map */
  base_port = get_port(ports[index]);

  /* Receive UDP data */
  r = recvfrom(sock, buffer, BUF_SIZE, 0,
           (struct sockaddr *)&sender_addr, &addrlen);

  /* If port is inactive, activate it with sender data */
  if (base_port->status == INACTIVE){
    printf("Activating %lu %d\n", (unsigned long)sender_addr.sin_addr.s_addr, 
           ntohs(sender_addr.sin_port));
    activate_port(base_port, sender_addr.sin_addr.s_addr, 
                  ntohs(sender_addr.sin_port));
  }
    
  /* Ignore datagram if it's not authorized */
  if (sender_addr.sin_addr.s_addr != base_port->sender_addr
      || sender_addr.sin_port != htons(base_port->sender_port)) { 
    fprintf(stderr, "ignoring unauthorized datagram\n");
    return;
  } else {
    /* Increasing counters */
    atomic_inc(udp_recv + index);
  }
  
  /* error handling */
  if(r <= 0) {
    if(r < 0) {
      fprintf(stderr, "Error in read\n");
      /* Increasing counters */
      atomic_inc(udp_errs + index);
    } else {
      fprintf(stderr, "Connection closed\n");
    }
    delete_event(index);
    return;
  }
  
  /* unpack ethernet frame from UDP */
  dst_addr = ((struct ether_addr *) buffer) + 0;
  src_addr = ((struct ether_addr *) buffer) + 1;
  tpid = (uint16_t *) (buffer + 2 * sizeof(struct ether_addr));
  pcp_dei = (uint16_t *) (buffer + 2 * sizeof(struct ether_addr) + 
                          sizeof(uint16_t));
  ether_type = (uint16_t *) (buffer + 2 * sizeof(struct ether_addr) + 
                             2 * sizeof(uint16_t));
  ether_type = ether_type;

  /* If frame is tagged, retrieve src_addr and vlan + add it to mac_map */
  if (ntohs(*tpid) == 0x8100){
    /* Zeroing PCP/DEI field */
    /* Adding pair <mac, tagged_vlan> */
    vlan_number = ntohs(*pcp_dei);
    vlan_number &= 4095; /* zeroing pcp-dei */
    *pcp_dei = htons(vlan_number);
    /* If such a vlan number is supported via UDP port, add it to mac map */
    if (valid_vlan(base_port, vlan_number))
      add_mac(*src_addr, vlan_number, ports[index], 1);
    else {
      atomic_inc(udp_errs + index);
      fprintf(stderr, "Ignoring unauthorized vlan number\n");
      return;
    }
  } else {
    /* Frame is not tagged with 802.1Q, obtaining vlan number from tpid field */
    vlan_number = base_port->untagged_vlan;
    /* If no untagged lan is supported, return */
    if (vlan_number == -1){
      atomic_inc(udp_errs + index);
      fprintf(stderr, "Untagged frame received for tagged-only port\n");
      return;
    }
    /* Adding pair <mac, untagged_vlan> */
    add_mac(*src_addr, vlan_number, ports[index], 0);
  }

  /* sending UDP further */
  if (ntohs(*tpid) == 0x8100) {
    fwd_port = get_port_from_mac(*dst_addr, vlan_number);
  } else {
    fwd_port = get_untagged_port_from_mac(*dst_addr);
  }
  
  /* Receiver found, forward udp frame */
  addrlen = sizeof(sender_addr);
  if (fwd_port != -1){
    forward_port = get_port(fwd_port);
    /* if tagged->untagged, untag forwarded frame */
    if (ntohs(*tpid) == 0x8100 && forward_port->untagged_vlan == vlan_number) {
      fprintf(stderr, "Frame shall be untagged.\n");
      untag_frame(buffer, dst_buffer);
      memcpy(buffer, dst_buffer, sizeof(buffer));
    /* if untagged->tagged, tag forwarded frame */
    } else if (ntohs(*tpid) != 0x8100 && 
               forward_port->untagged_vlan != vlan_number) {
      tag_frame(buffer, dst_buffer, vlan_number);
      memcpy(buffer, dst_buffer, sizeof(buffer));
    }
    
    forward_frame(sender_addr, forward_port, addrlen, r, buffer);
  } else {
    /* Broadcast frame to everyone in a VLAN */
    reset_vlan_iterator();
    /* fwd_port is a port with specified vlan */
    while ( (fwd_port = vlan_next_port(vlan_number)) != -1) {
      forward_port = get_port(fwd_port);

      /* if tagged->untagged, untag forwarded frame */
      if (ntohs(*tpid) == 0x8100 && 
          forward_port->untagged_vlan == vlan_number) {
        fprintf(stderr, "Frame shall be untagged.\n");
        untag_frame(buffer, dst_buffer);
        memcpy(buffer, dst_buffer, sizeof(buffer));
      /* if untagged->tagged, tag forwarded frame */
      } else if (ntohs(*tpid) != 0x8100 && 
                 forward_port->untagged_vlan != vlan_number) {
        tag_frame(buffer, dst_buffer, vlan_number);
        memcpy(buffer, dst_buffer, sizeof(buffer));
      }
      
      /* Avoiding loopback */
      if ((forward_port->sender_addr != sender_addr.sin_addr.s_addr)
          || (htons(forward_port->sender_port) != sender_addr.sin_port))
        forward_frame(sender_addr, forward_port, addrlen, r, buffer);
    }
  }
}


void tag_frame(char* buffer, char* dst_buffer, int vlan){
  uint16_t *tpid, *pcp_dei;

  /* dst_buffer's tpid and pcp_dei */
  tpid = (uint16_t *) (dst_buffer + 2 * sizeof(struct ether_addr));
  pcp_dei = (uint16_t *) (dst_buffer + 2 * sizeof(struct ether_addr) + 
                          sizeof(uint16_t));

  /* copy MAC addresses to dst_buffer*/
  memcpy(dst_buffer, buffer, 12);
  /* set vlan tag */
  *tpid = htons(0x8100);
  *pcp_dei = htons(vlan);
  /* copy the rest */
  memcpy(dst_buffer + 16, buffer + 12, 1508);
}


void untag_frame(char* buffer, char* dst_buffer){
  memcpy(dst_buffer, buffer, 12);
  memcpy(dst_buffer + 12, buffer + 16, 1502);
}

