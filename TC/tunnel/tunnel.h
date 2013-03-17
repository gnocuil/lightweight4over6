#ifndef _TUNNEL_46_H_
#define _TUNNEL_46_H_

#include"mapping.h"

#define TUNNEL_SET 1

#define TUNNEL_DEVICE_NAME "4over6"
#define TUNNEL_BIND_IFACE "eth0"//default binded interface.

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

static struct net_device *netdev=NULL;
static DEFINE_RWLOCK(lw4over6_lock);

//lw4over6 tunnel private data--encapsulation table
struct lw4over6_tunnel_private
{   
    char ifname[IFNAMSIZ];//the iface that the tunnel binds with.
    int dhcp_snoofing;//whether dhcp snoofing is on
    struct in6_addr local6;//local ipv6 address of the binded iface
    struct ecitem *ectables[HASH_SIZE];
    struct net_device *dev;
};

#endif


