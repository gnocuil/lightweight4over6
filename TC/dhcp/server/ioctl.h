#ifndef __PUBLIC4OVER6_IOCTL_H__
#define __PUBLIC4OVER6_IOCTL_H__
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

#define TUNNEL_DEVICE_NAME "public4over6"

#define TUNNELMESSAGE SIOCDEVPRIVATE
#define TUNNEL_MAPPING_NUM SIOCDEVPRIVATE+1//get the num of ecitems
#define TUNNEL_MAPPING_INFO SIOCDEVPRIVATE+2//get the ecitems
#define TUNNEL_MAC_MAPPING_NUM SIOCDEVPRIVATE+3//get the num of mac mappings
#define TUNNEL_MAC_MAPPING_INFO SIOCDEVPRIVATE+4//get the mac mappings
#define TUNNEL_DEL_ALL_MAPPING SIOCDEVPRIVATE+5//del all the ecitems
#define TUNNEL_SET_MAPPING SIOCDEVPRIVATE+6//add or modify a ecitem
#define TUNNEL_DEL_MAPPING SIOCDEVPRIVATE+7//del a ecitem
#define TUNNEL_GET_BINDING SIOCDEVPRIVATE+8//get the tunnel binded info and ipv6.
#define TUNNEL_SET_BINDING SIOCDEVPRIVATE+9//set the tunnel to bind which interface.

//public4over6 tunnel encapsulation item
struct ecitem
{  
   struct in_addr remote;
   struct in6_addr remote6,local6;
   unsigned short pset_index, pset_mask; //pset
   struct timeval start_time;
   int seconds;//lease time limit
   long long in_pkts,inbound_bytes;
   long long out_pkts,outbound_bytes;
   int tag;//if tag==1,then this is manual,if tag==2,then this is auto.
   struct ecitem *next; 
};
void set_mapping(struct in_addr remote,struct in6_addr remote6, struct iaddr_pset ip_pset)
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
    itm.pset_index = ip_pset.pset_index;
    itm.pset_mask = ip_pset.pset_mask;
    itm.tag=2;//manual type,so we should do adjustment for the specified manual mapping item.
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
