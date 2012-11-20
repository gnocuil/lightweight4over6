#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
//#include<arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
//#include <linux/in.h>
#include <unistd.h>
#include <netinet/in.h>  //IPPROTO_RAW,IPPROTO_UDP
#include <netinet/ip.h>  //struct ip
#include <netinet/ip6.h> //struct ip6_hdr
#define BUFFLEN 1501
#define TUNNEL_IFNAME "public4over6"

struct udp6_psedoheader {
        uint8_t srcaddr[16];
        uint8_t dstaddr[16];
        uint32_t length;
        uint16_t zero1;
        uint8_t zero2;
        uint8_t next_header;
};
struct udp4_psedoheader {
        uint32_t srcaddr;
        uint32_t dstaddr;
        uint8_t zero;
        uint8_t protocol;
        uint16_t length;
};



char PHYSIC_IFNAME[20];

struct ifreq ifopt;
char buff[BUFFLEN];
int buffLen;
char *ethhead, *iphead, *udphead, *payload;
int udplen;
char macaddr_4o6[6], macaddr_phy[6], local6addr[128], remote6addr[128], macaddr_remote[6];
char remote6addr_buf[16];

int s_dhcp, s_info, s_send;
struct sockaddr_in6 remote_addr6,local_addr6;
char ciaddr[4], siaddr[4];
struct sockaddr_ll device;
struct ip send_ip4hdr;
struct ip6_hdr send_ip6hdr;
//uint16_t dhcp4_src_port_t, dhcp4_src_port;

int status;
#define STOP 0
#define INITIATE 1
#define WAIT_FOR_REQ 2
#define WAIT_FOR_REPLY 3

unsigned short int checksum (unsigned short int*, int);
char* mac_to_str(unsigned char *);
void hexNumToStr(unsigned int, char *);
int setDevIndex(char *);
int getPacket( );
int isUDPpacket(char* iphead, int type);
int isDHCPpacket(int type, char*);
int isTunnelDhcp(char*);
int isDHCPACK(char*);

int sendPacket6(char*, char*, int);
int sendPacket4(char*, char*, int);

uint16_t udpchecksum(char *, char *, int, int);


//UI Function
void show_help(void);

//Debugging Function
int getFakeReply(void); // This function is to listen on IPv6 port 67, and then vanish the ICMPv6 Port-Unreachable message

