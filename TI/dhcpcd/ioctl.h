#ifndef __PUBLIC4OVER6_IOCTL_H__
#define __PUBLIC4OVER6_IOCTL_H__
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define TUNNEL_DEVICE_NAME "public4over6"

#define TUNNELPORTSET SIOCDEVPRIVATE+1//set port set mask and index to support ping (icmp).

typedef struct portset
{
	uint16_t index;
	uint16_t mask;
} portset_t;

void set_mapping(uint16_t index, uint16_t mask)
{
    struct portset pset;
    struct ifreq req;
    int sock;

    sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);//raw socket in order to interact with tunnel
    if (sock<0)
    {
        fprintf(stderr,"Can't create io control socket for interface %s!\n",TUNNEL_DEVICE_NAME);
        return ;
    }
    pset.index = index;
    pset.mask = mask;
    memset(&req,0,sizeof(struct ifreq));
    strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
    req.ifr_data=(caddr_t)&pset;//mapping item
    if(ioctl(sock,TUNNELPORTSET,&req)<0)
    {
        printf("can't send ioctl message TUNNEL_SET_MAPPING!\n");
        return ;
    }
    close(sock);
}
#endif
