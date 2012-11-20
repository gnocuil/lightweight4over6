#ifndef __PUBLIC4OVER6_IOCTL_H__
#define __PUBLIC4OVER6_IOCTL_H__
#include "tunnel.h"
void set_mapping(struct in_addr remote,struct in6_addr remote6)
{
    struct ecitem itm;
    struct ifreq req;
    int sock;

    sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);//raw socket in order to interact with tunnel
    if (sock<0)
    {
        fprintf(stderr,"Can't create io control socket for interface %s!\n",TUNNEL_DEVICE_NAME);
        return ;
    }
    memset(&itm,0,sizeof(struct ecitem));
    itm.remote=remote;
    itm.remote6=remote6;
    itm.tag=0;//manual type,so we should do adjustment for the specified manual mapping item.
    itm.seconds=5000;
    memset(&req,0,sizeof(struct ifreq));
    strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
    req.ifr_data=(caddr_t)&itm;//mapping item
    if(ioctl(sock,TUNNEL_SET_MAPPING,&req)<0)
    {
        printf("can't send ioctl message TUNNEL_SET_MAPPING!\n");
        return ;
    }
    close(sock);
}
#endif
