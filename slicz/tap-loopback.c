/*
 * Author: Konrad Słoniewski
 * Date:   18 August 2013
 */

/* Przykład użycia interfejsu TAP w Linuksie.
 *
 * Program tworzy wirtualny interfejs sieciowy o nazwie 'siktap',
 * który działa tak, że wszystkie pakiety wysyłane do tego interfejsu
 * są z powrotem przezeń odbierane, jak gdyby podłączona była do niego pętla.
 */

#include <event2/event.h>
#include <event2/util.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <errno.h>
#include <unistd.h>
#include <net/ethernet.h>
#include <stdint.h>
#include <netdb.h>

#include "err.h"
#include "ports.h"

# define BUF_SIZE 1518

/* Network data */
int sock, fd;
struct sockaddr_in my_address;

/* Event data */
struct event_base* base;
struct event* udp_event;
struct event* tun_event;

void udp_response(evutil_socket_t socket, short event, void *arg) {
  struct sockaddr_in switch_address;
  char buf[BUF_SIZE + 1];
  int rbytes, wbytes;
  ssize_t length = sizeof(switch_address);

   /* Każdy odczyt z deskryptora fd zwróci nam jedną ramkę Ethernet, którą
    * system chciałby wysłać przez stworzony interfejs sieciowy. Bufor musi
    * być odpowiednio duży, w przeciwnym razie ramka zostanie obcięta. Można
    * nie obsługiwać ramek dłuższych niż 1500 bajtów + nagłówek. Taka jest też
    * domyślna wartość MTU
    * (http://lxr.linux.no/linux+v3.9/drivers/net/tun.c#L879).
    *
    * Podobnie pakiet przychodzący powinien zostać zapisany do deskryptora fd
    * pojedynczą operacją write.
    *
    * Poza tym deskryptor fd zachowuje się bardzo podobnie jak inne
    * deskryptory. W szczególności można go użyć w select/poll.
    */
  rbytes = recvfrom((int) sock, buf, sizeof(buf), 0,
    (struct sockaddr*)&switch_address, (socklen_t*) &length);
  if (rbytes < 0) {
    perror("reading data");
    return;
  }

  fprintf(stderr, "%d bytes received via UDP.\n", rbytes);
    /* Format ramek: Ethernet II (także zwany DIX), proszę poszukać, z tym że
     * bez preambuły i sumy kontrolnej (FCF). Można też się przyjrzeć
     * w Wiresharku plikowi example-capture.pcap. Też takie ramki należy
     * przekazywać do serwera po UDP.
     */

  wbytes = write(fd, buf, rbytes);
  if (wbytes < 0) {
    perror("writing data");
    return;
  }
}

void tun_read(evutil_socket_t socket, short event, void* arg) {
  char buf[BUF_SIZE + 1];
  ssize_t rbytes;
  int sflags = 0;
  int length = sizeof(struct sockaddr_in);

  rbytes = read(fd, buf, sizeof(buf));
  if (rbytes < 0) {
    perror("reading data");
    return;
  }

  fprintf(stderr, "Forwarded: %d bytes\n", 
          (int) sendto(sock, buf, rbytes, sflags, 
                       (struct sockaddr*) &my_address, length));
}

int main(int argc, char** argv)
{
  /* Default settings */
  struct ifreq ifr;
  int fd, err;

  /* Interface name */
  char* interface_name = "siktap";

  /* Switch data */
  char** switch_data;

  /* Network connection data */
  struct addrinfo addr_hints;
  struct addrinfo* addr_result;
  struct sockaddr_in server_addr;

  /* Reading parameters from input */
  char c;
  fprintf(stderr, "Przed wczytywaniem parametrow\n");
  while ((c = getopt(argc, argv, "d:")) != -1) {
    switch (c) {
      case 'd':
        interface_name = optarg;
        break;
      default:
        abort();
    }
  }

  /* Reading switch parameters from arguments */
  switch_data = split(argv[optind], ":", 2);
  fprintf(stderr, "Switch address is %s:%s\n", switch_data[0], switch_data[1]);

  fprintf(stderr, "Przed otwieraniem deskryptora tun\n");
  if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
    perror("open(/dev/net/tun)");
    return 1;
  }

  /* Po otwarciu urządzenia /dev/net/tun należy na otrzymanym deskryptorze
   * wykonać ioctl TUNSETIFF. W szczególności trzeba podać rodzaj interfejsu
   * (my chcemy TAP; szczegóły: 
   *   https://www.kernel.org/doc/Documentation/networking/tuntap.txt).
   * Można też ustalić nazwę, jak poniżej.
   */

  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, interface_name, IFNAMSIZ);

  /* Ustawiamy następujące flagi:
   *  IFF_TAP   - urządzenie typu TAP, tzn. warstwa 2; wysyłane i odbierane
   *              będą ramki Ethernet
   *  IFF_NO_PI - nie dodawaj dodatkowych nagłówków (bez tego jest jeszcze
   *              czterobajtowy nagłówek dodawany przez kernel)
   */
  fprintf(stderr, "Przed ioctl\n");
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0) {
    perror("ioctl(TUNSETIFF)");
    return 1;
  }
  fprintf(stderr, "Po ioctl\n");

  /* Teraz już w systemie pojawił się interfejs 'siktap'. Możemy mu
   * skonfigurować adres IP ifconfigiem itp. Można też polecić systemowi jego
   * automatyczną konfigurację za każdym razem, gdy powstaje. Robi się to
   * standardowymi metodami, zależnymi od dystrybucji Linuksa. Na przykład w
   * Debianie/Ubuntu można dodać takie zaklęcie do /etc/network/interfaces:
   *
   *   auto siktap
   *   iface siktap inet static
   *     address 192.168.2.1
   *     netmask 255.255.255.0
   *
   * Po skonfigurowaniu możemy obserwować ruch, np. programem Wireshark.
   * Wszystkie ramki wychodzące powinny następnie być z powrotem odebrane (tak
   * działa ten program).
   *
   * Można też konfigurować w systemie dodatkowe interfejsy wysyłające i
   * odbierające ramki tagowane VLAN-ami (zgodne z 802.1q). Musi być jednak
   * zainstalowany pakiet 'vlan'. Jeśli do powyższej konfiguracji dodamy jeszcze
   *
   *   auto siktap.15
   *   iface siktap.15 inet static
   *     vlan-raw-device siktap
   *     address 192.168.3.1
   *     netmask 255.255.255.0
   *
   * i uruchomimy ten program, to powstanie automatycznie także interfejs
   * siktap.15. Przykładowy plik example-capture.pcap (można go otworzyć
   * Wiresharkiem) zawiera dwie ramki Ethernet: jedną wysłaną interfejsem
   * 'siktap.15' i jedną wysłaną przez 'siktap'.
   *
   * Trochę mylące może być to, że Wireshark nasłuchujący na 'siktap' widzi
   * także odebrane ramki tagowane, natomiast nie są one tam przekazywane dalej
   * do podsystemu sieciowego. Są w pełni przetwarzane na 'siktap.15', gdzie
   * są przekazywane po usunięciu tagowania.
   */

  /* Creating network connection */
  (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_family = AF_INET;
  addr_hints.ai_socktype = SOCK_DGRAM;
  addr_hints.ai_protocol = IPPROTO_UDP;
  addr_hints.ai_flags = 0;
  addr_hints.ai_addrlen = 0;
  addr_hints.ai_addr = NULL;
  addr_hints.ai_canonname = NULL;
  addr_hints.ai_next = NULL;
  if (getaddrinfo(switch_data[0], NULL, &addr_hints, &addr_result) != 0) {
    syserr("getaddrinfo");
  }

  my_address.sin_family = AF_INET;
  my_address.sin_addr.s_addr = 
    ((struct sockaddr_in*)(addr_result->ai_addr))->sin_addr.s_addr;
  my_address.sin_port = htons(atoi(switch_data[1]));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = 
    ((struct sockaddr_in*)(addr_result->ai_addr))->sin_addr.s_addr;
  server_addr.sin_port = htons(0);

  freeaddrinfo(addr_result);

  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    syserr("socket");
  
  bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

  free_array(switch_data, 2);

  /* Creating events */
  base = event_base_new();
  if (!base)
    syserr("Error creating base.");

  udp_event =
    event_new(base, sock, EV_READ|EV_PERSIST, udp_response, (void*) base);
  if (!udp_event) 
    syserr("Error creating event for a listener socket.");
  if (event_add(udp_event, NULL) == -1)
    syserr("Error adding listener_socket event.");

  tun_event =
    event_new(base, fd, EV_READ|EV_PERSIST, tun_read, (void*) base);
  if (!tun_event)
    syserr("Error creating event for a listener socket.");
  if (event_add(tun_event, NULL) == -1)
    syserr("Error adding listener_socket event.");

  printf("Slijent started.\n");
  if (event_base_dispatch(base) == -1)
    syserr("Error running slijent dispatch loop.");
  printf("Slijent closed.\n");

  event_free(udp_event);
  event_free(tun_event);
  event_base_free(base);

  return 0;
}

