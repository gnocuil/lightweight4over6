#ifndef MAPPING_H_
#define MAPPING_H_

//#include<linux/in.h>
//#include<linux/in6.h>
//#include<linux/netdevice.h>

//#define HASH(addr) (((__force u32)addr^((__force u32)addr>>4))&0xF)
//#define HASH_SIZE 16

//#define HASH(addr) ((__force u32)addr)%0x3FEE
//pset hash func
#define HASH_SIZE 0x3FEE
#define HASH(addr,bits,index) (((((__force u32)(addr))%HASH_SIZE)<<(bits))|((index)>>(16-(bits))))%HASH_SIZE 


//lw4over6 tunnel encapsulation item
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

//lookup the ecitem according to IPv4 remote address + portNum [pset]
extern struct ecitem* lw4over6_ecitem_lookup(struct net_device *dev,struct in_addr *remote, unsigned short portNum);

//lookup the ecitem according to v4 remote addr + index & mask
//struct ecitem* puclic4over6_ecitem_lookup_pset(struct net_device *dev, struct in_addr *remote, unsigned short pset_index, unsigned short pset_mask);


//lookup the encapsulation item according IPv6 remote address
extern struct ecitem* lw4over6_ecitem_lookup_by_ipv6(struct net_device
*dev,struct in6_addr *remote6);

//set the encapsulation item
extern void lw4over6_ecitem_set(struct net_device *dev,struct ecitem *pect);
//unlink the encapsulation item according IPv4 remote address + pset_index & pset_mask [pset]
extern struct ecitem* lw4over6_ecitem_unlink(struct net_device *dev,struct in_addr *remote,unsigned short pset_index, unsigned short pset_mask, int tag);
//link the encapsulation item
extern void lw4over6_ecitem_link(struct net_device *dev,struct ecitem *ect);
//free all the ecitems
extern void lw4over6_ecitem_free(struct net_device *dev);

//compare two strings
extern int comp_string(unsigned char *source,unsigned char *dest,int size);

extern int mask_bits_pset(unsigned short mask);

#endif
