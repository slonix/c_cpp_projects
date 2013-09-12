Program usage:

0. Instalacja libevent 2.0
     sudo su
     ./configure && make
     make install
     (make verify)
   

1. Run slicz:
   make
   ./slicz

   We can add standard number on which slicz server runs:
   ./slicz -p 42456

2. In another console we can configure slicz via nc:
   echo <command> | nc localhost 42420

   eg.
   echo "setconfig 42123//1,2t,3t" | nc localhost 42420
   echo "getconfig" | nc localhost 42420

3. Prepare for running project
   Host:
      sudo mkdir /dev/net/
      sudo mknod /dev/net/tun c 10 200
      sudo chmod 0666 /dev/net/tun
      
   Virtual machines:
      https://wiki.ubuntu.com/vlan - vlan installation - sudo apt-get install vlan

4. We run slijent using TAP (more info in tap-loopback.c):
   sudo ./slijent -d siktap <host>:<port>
