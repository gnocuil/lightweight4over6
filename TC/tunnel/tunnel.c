#include <net/protocol.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/arp.h>
#include <net/inet_ecn.h>
#include <linux/etherdevice.h>
#include <net/ipv6.h>

#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include "tunnel.h"

#include <net/icmp.h>

#ifndef NF_IP6_LOCAL_OUT
#define NF_IP6_LOCAL_OUT 3
#endif

#define DEBUG_lw4over6_  1

#ifdef DEBUG_lw4over6_
#define CDBG(msg,args...) printk(KERN_DEBUG msg,##args)
#else
#define CDBG(msg,args...) do {}while(0)
#endif


//advance declaration
char* inet_ntoa( struct in_addr in, char* buf, size_t* rlen );
int xmit( struct sk_buff* skb, struct in6_addr out_addr, struct net_device* dev );
char* inet_ntop_v6( struct in6_addr, char* dst );
void inet_pton_v6(char*,struct in6_addr*);
static void tunnel_setup( struct net_device* dev );


static uint16_t bigpacket[65536];

//[pset]get the destination port number from skb
unsigned short get_portNum_dest(struct sk_buff* skb, int isSend)
{
    int protoff;
    struct tcphdr *tcph;
    struct udphdr *udph;
    struct iphdr *iph;
    
    uint16_t ret = 0;
    
    iph = ip_hdr(skb);
    
    uint8_t flags = (uint8_t)(ntohs(iph->frag_off) >> 13);
    
    //fragment offset
    uint16_t frag_off = ntohs(iph->frag_off) & 0x1FFF;
    
    //for fragmented ipv4 packets, choose the port number in the first packet
    if (!isSend && frag_off != 0) {
        return bigpacket[iph->id];
    }
    
    protoff = iph -> ihl * 4;
    if (iph->protocol == IPPROTO_TCP) {
        tcph=skb_header_pointer(skb, protoff, sizeof(tcph), &tcph); 
        ret = ntohs(tcph->dest);
    } else if (iph->protocol == IPPROTO_UDP) {
        udph=skb_header_pointer(skb, protoff, sizeof(udph), &udph);
        ret = ntohs(udph->dest);
    } else if (!isSend && iph -> protocol == IPPROTO_ICMP) {
        //uint8 _t *
        struct icmphdr *icmph = (struct icmphdr *)(skb->data+(iph->ihl<<2));
        uint16_t id;
        int i;
        switch (icmph -> type) {
            case 0:
            case 8:
                id = htons(*(uint16_t*)(((uint8_t*)icmph) + 4));
                ret = id;
                break;
            default:
                iph = (struct iphdr*) (((uint8_t*)icmph) + 8);
                protoff = iph -> ihl * 4;
                if (iph->protocol == IPPROTO_TCP) {
                    tcph = (struct tcphdr*) (iph + protoff);
                    ret = ntohs(tcph->source);
                } else if (iph->protocol == IPPROTO_UDP) {
                    udph = (struct udphdr*) (iph + protoff);
                    ret = ntohs(udph->source);
                }
                break;
        }
    } 
    
    if (!isSend && (flags & 1) && frag_off == 0) {
        bigpacket[iph->id] = ret;
    }
    
    return ret;
}

//[pset] get source port number
unsigned short get_portNum_src(struct sk_buff* skb, int isSend)
{
    int protoff;
    struct tcphdr *tcph;
    struct udphdr *udph;
    struct iphdr *iph;

    iph = ip_hdr(skb);
    protoff = iph -> ihl * 4;
    if(iph->protocol == IPPROTO_TCP){
        tcph=skb_header_pointer(skb, protoff, sizeof(tcph), &tcph);
        return ntohs(tcph->source);
    }else if(iph->protocol == IPPROTO_UDP){
        udph=skb_header_pointer(skb, protoff, sizeof(udph), &udph);
        return ntohs(udph->source);
    } else if (isSend && iph -> protocol == IPPROTO_ICMP) {
        struct icmphdr *icmph = (struct icmphdr *)(skb->data+(iph->ihl<<2));
        uint16_t id;
        switch (icmph -> type) {
            case 0:
            case 8:
                id = htons(*(uint16_t*)(((uint8_t*)icmph) + 4));
                return id;
            default:
                break;
        }
    } 
    return 0;
}

int tunnel_open(struct net_device* dev)
{
    //MOD_INC_USE_COUNT;
    return 0;
}

int tunnel_close(struct net_device* dev)
{
    //MOD_DEC_USE_COUNT;
    return 0;
}


int tunnel_neigh_setup(struct neighbour* n)
{
    if (n->nud_state==NUD_NONE)
    {
        n->ops = &arp_broken_ops;
        n->output = n->ops->output;
    }
    return 0;
}

int tunnel_neigh_setup_dev(struct net_device* dev,struct neigh_parms *p)
{
    if ( p->tbl->family==AF_INET)
    {
        p->neigh_setup = tunnel_neigh_setup;
        p->ucast_probes = 0;
        p->mcast_probes = 0;
    }
    return 0;
}

/* Generate a random Ethernet address (MAC) that is not multicast
 * and has the local assigned bit set.
 */
void generate_random_hw(struct net_device *dev)
{
    unsigned char tmp;
    struct net_device *pdev=__dev_get_by_name(&init_net,"eth0");
    if (pdev == NULL) {
        get_random_bytes(dev->dev_addr, ETH_ALEN);
        dev->dev_addr[0] &= 0xfe;	// clear multicast bit 
        dev->dev_addr[0] |= 0x02;	//set local assignment bit (IEEE802)
    } else {
        memcpy(dev->dev_addr,pdev->dev_addr,6);
        tmp=dev->dev_addr[1];
        dev->dev_addr[1]=dev->dev_addr[4];
        dev->dev_addr[4]=tmp;
    }
}

//return the type number of dhcp packet and hardware address
unsigned char* dhcp_type(struct sk_buff *skb,unsigned char *type)
{
   struct iphdr *iph=ip_hdr(skb);
   int offset=0;
   unsigned char *buff=skb->data;//pointer to the start of skb.
   unsigned char op,htype,hlen,hops;//dhcp field.
   unsigned char *hw;
   
   CDBG("This is dhcp_type()!\n");
    
   offset+=(int)(iph->ihl)*4;
   *type=0;
     
   if(buff[9]==0x11 &&  ((buff[offset]==0x00 && buff[offset+1]==0x43) || (buff[offset+2]==0x00 && buff[offset+3]==0x43)) )//This is dhcp packet
   { 
      offset+=8;//UDP header is 8-byte length
      op=buff[offset];
      htype=buff[offset+1];
      hlen=buff[offset+2];
      hops=buff[offset+3];
      
      //Message type+Hardware type+Hardware addr length+Hops
      //+Transaction ID+Seconds elapsed+Bootp flags+
      //+Client IP+Your IP+Next server IP+Relay agent IP
      offset+=1+1+1+1+4+2+2+4+4+4+4;//reference to RFC2131
      
      hw=buff+offset;
      //Client MAC address+Server host name+Boot file name+
      //Magic cookie+Option
      offset+=16+64+128+4+2;//reference to RFC2131
      *type=*(buff+offset);
     
      CDBG("DHCP:op is %d,htype is %d,hlen is %d,hops is %d,type is %d!\n",op,htype,hlen,hops,*type);
      CDBG("This is the end of dhcp_type()!\n");
      
      return hw;
       
   } 
   CDBG("This is the end of dhcp_type()!\n");
   return 0x00;//This is not dhcp packet.
}

//get the lease time from DHCP ACK packet and return. skb->data points to the start of ip header.
int dhcp_lease_time(struct sk_buff *skb)
{
   struct iphdr *iph=ip_hdr(skb);
   int offset,seconds,i;
   unsigned char *buff=skb->data;//pointer to the start of skb.
   offset=0;
   offset+=(int)(iph->ihl)*4;//offset the IP header
   offset+=8;//offset the UDP header

   //Message type+Hardware type+Hardware addr length+Hops
   //+Transaction ID+Seconds elapsed+Bootp flags+
   //+Client IP+Your IP+Next server IP+Relay agent IP
   offset+=1+1+1+1+4+2+2+4+4+4+4;//reference to RFC2131
 
   //Client MAC address+Server host name+Boot file name+
   //Magic cookie+Option
   offset+=16+64+128+4+3;//reference to RFC2131
   offset+=6+2;
   buff+=offset;
   seconds=0;
   for(i=0;i<4;i++)
      seconds=(seconds*256+*(buff+i));
   return seconds;   
}


//get the IPv6 address of the specified interface.
struct in6_addr* get_ipv6_addr(char ifname[])
{  
   struct net_device *dev;
   struct inet6_dev *inet6dev;
   struct inet6_ifaddr *ifaddr6;
   struct in6_addr *addr6=NULL;
   dev=dev_get_by_name(&init_net,ifname);
   if(!dev)
   {
        return 0;
   }
   inet6dev=(struct inet6_dev*)dev->ip6_ptr;
   if(!inet6dev)
   {
        dev_put(dev);
        return 0;
   }
   ifaddr6=inet6dev->addr_list;
   while(ifaddr6)
   {  
        addr6=&ifaddr6->addr;
        if(ipv6_addr_type(addr6)==IPV6_ADDR_UNICAST)
            break;
        ifaddr6=ifaddr6->if_next;
   }
   dev_put(dev);
   return addr6;
}

/*
   Main process routine
   if we use the way of invoking xmit() to send the package,
   then we still need some changes.
*/
int lw4over6_tunnel_xmit(struct sk_buff *skb,struct net_device *dev)
{
 
    struct iphdr *iph;
    struct in6_addr src_addr,out_addr;
    struct in_addr daddr,saddr;
    struct ipv6hdr *ip6h;
    struct flowi fl;
    struct dst_entry *dst;
    struct ecitem *ect;
    struct net_device_stats *stats;//statistics
    struct lw4over6_tunnel_private *pinfo;
    char buf[512];
    unsigned int head_room;    
    unsigned short portNum;
    CDBG("xmit in %d,protocol %d\n",htons(ETH_P_IP), skb-> protocol);
    if(skb->protocol!= htons(ETH_P_IP)) //judgement of IP protocol
    {
        CDBG("lw4over6_tunnel_xmit:this is not IPv4 protocol this is %d\n",skb->protocol);
        goto tx_error;
    }
    stats=&dev->stats;
    pinfo=(struct lw4over6_tunnel_private*)netdev_priv(dev);
    iph = ip_hdr(skb);
    daddr.s_addr = iph->daddr;
    saddr.s_addr = iph->saddr;
    
    /* print source and destination of skb */
#ifdef DEBUG_lw4over6_
    inet_ntoa(saddr,buf,NULL);
    CDBG("lw4over6_tunnel_xmit:the source IPv4 address is %s\n",buf);
    inet_ntoa(daddr,buf,NULL);
    CDBG("lw4over6_tunnel_xmit:the destination IPv4 address is %s\n",buf);
#endif
    /* [pset] get the portNum of the IPv4 packet for ecitem lookup*/
    portNum = get_portNum_dest(skb, 0);  
    CDBG("[lw4over6]lw4over6_tunnel_xmit:the portNum is %d\n", portNum);
    /* find corresponding encapsulation item */
    ect=lw4over6_ecitem_lookup(dev,(struct in_addr*)&daddr, portNum);//pset:add portNum
    if(ect)
    {
       ect->inbound_bytes+=skb->len;
       ect->in_pkts++;
       src_addr=pinfo->local6;
       out_addr=ect->remote6;
    }
    else 
    {   
        CDBG("lw4over6_tunnel_xmit: Can not find corresponding ipv6 destination in ECT!\n");            
        goto tx_error;
    }   

#ifdef DEBUG_lw4over6_
    inet_ntop_v6(src_addr,buf);
    printk(KERN_INFO "IPv6 source address is %s!\n",buf);
    inet_ntop_v6(out_addr,buf);
    printk(KERN_INFO "IPv6 dest address is %s!\n",buf);
#endif
    
    head_room=sizeof(struct ipv6hdr)+16;
    if (skb_headroom(skb)<head_room || skb_cloned(skb) || (skb_shared(skb)&&!skb_clone_writable(skb,0)))
    {  
       struct sk_buff* new_skb = skb_realloc_headroom(skb,head_room);
       if (!new_skb)
       {
            stats->tx_dropped++;
            goto tx_error;
       }
       if (skb->sk)
       {
            skb_set_owner_w(new_skb, skb->sk);
       }
       dev_kfree_skb(skb);
       skb = new_skb;
       //goto tx_error;
     }
     
    skb_push(skb,sizeof(struct ipv6hdr));
    skb->transport_header = skb->network_header;
    skb_reset_network_header(skb);//skb->network_header=skb->data

    //free the old router entry
    skb_dst_drop(skb);
     
     //Fill the IPv6 header
    ip6h = ipv6_hdr(skb);
    memset(ip6h, 0, sizeof( struct ipv6hdr));
    ip6h->version = 6;
    ip6h->priority = 0;
    ip6h->payload_len = htons( skb->len - sizeof( struct ipv6hdr ));
    ip6h->nexthdr = IPPROTO_IPIP;//IPv4 over IPv6 protocol
    ip6h->hop_limit = 64;
     ip6h->saddr = src_addr;
    ip6h->daddr = out_addr;
    skb->protocol = htons(ETH_P_IPV6);

#ifdef CONFIG_NETFILTER
    nf_conntrack_put(skb->nfct);
    skb->nfct = NULL;
     
#ifdef CONFIG_NETFILTER_DEBUG
    skb->nf_debug = 0;
#endif
#endif
        
    fl.proto = IPPROTO_IPV6;
    fl.nl_u.ip6_u.daddr = out_addr;
    dst = ip6_route_output(dev_net(dev),NULL,&fl);
    skb_dst_set(skb,dst_clone(dst));
    nf_reset(skb);
    if(!skb_dst(skb))
    {
        printk(KERN_WARNING":lw4over6_tunnel_xmit:Cannot find route information for packet!\n");
        goto tx_error;
    }
    else
    {
        stats->tx_packets++; 
        stats->tx_bytes+=skb->len;
        /*
        ((struct inet_sock*)(skb->sk))->pinet6 is a pointer to struct ipv6_pinfo(private data of IPv6)
        If I don't add the following line,then when ftp telnet or http(and so on)coexist with dhcp,
        then at the very time of dhcp packet is sent through tunnel_xmit(), ((struct inet_sock*)(skb->sk))->pinet6
        would be a novalid pointer which will result a kernel panic.
        But at present,I don't know why it is the situation.So I just add this line in order to stop kernel panic from
        happening.
        Maybe as I study and debug the Linux(Network)kernel further and further,I will firgure out the troublesome problem.
        */
        if(skb->sk && ((struct inet_sock*)(skb->sk))->pinet6)
           ((struct inet_sock*)(skb->sk))->pinet6=NULL;
        NF_HOOK(PF_INET6,NF_IP6_LOCAL_OUT,skb,NULL,skb_dst(skb)->dev,skb_dst(skb)->output);
        return NETDEV_TX_OK;
    }    
tx_error: 
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}


//Should we do something here?
void lw4over6_err( struct sk_buff* skb,struct inet6_skb_parm* opt,u8 type,u8 code,int offset,__be32 info )
{
    CDBG("IP6IP Error!(type=%d, code=%d, offset=%d)\n",type,code,offset);
}


//IOCTL function
int tunnel_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{  
   struct lw4over6_tunnel_private *pinfo; 
   struct ecitem *pecit,*ptables;
   int i=0,ecitem_num=0,err;
   if (!capable(CAP_NET_ADMIN))
   {
      return -EPERM;
   }
   if(cmd==TUNNEL_MAPPING_NUM)//we should know all the ecitems(lw4over6 tunnel encapsulation item)
   {
       ecitem_num=0;
       pinfo=(struct lw4over6_tunnel_private*)netdev_priv(dev);
       for(i=0;i<HASH_SIZE;i++)
       {  
          if(pinfo && pinfo->ectables[i])//NULL pointer check.
          {
             pecit=pinfo->ectables[i];
             while(pecit)
             {
                ecitem_num++;
                pecit=pecit->next;
             }
          }
       }
       err=copy_to_user(ifr->ifr_data,&ecitem_num,sizeof(ecitem_num));
       if(err)
       {
          CDBG("copy_to_user fails!\n");
          return err;
       } 
   }
   else if(cmd==TUNNEL_MAPPING_INFO)//we would like to retrieve the mapping item info.
   {   
       //First,we should calculate the number of ecitems.
       pinfo=(struct lw4over6_tunnel_private*)netdev_priv(dev);
       ecitem_num=0;
       for(i=0;i<HASH_SIZE;i++)
       {  
          if(pinfo && pinfo->ectables[i])//NULL pointer check.
          {
             pecit=pinfo->ectables[i];
             while(pecit)
             {
                ecitem_num++;
                pecit=pecit->next;
             }
          }
       }
       //Then,we alloc memory for the ecitems.
       ptables=(struct ecitem*)kmalloc(ecitem_num*sizeof(struct ecitem),GFP_KERNEL);
       if(!ptables)
       {
          CDBG("kmalloc fails!\n");
          return -1;
       }
       //Next,we should copy all the ecitems.
       ecitem_num=0;
       pinfo=(struct lw4over6_tunnel_private*)netdev_priv(dev);
       for(i=0;i<HASH_SIZE;i++)
       {  
          if(pinfo && pinfo->ectables[i])//NULL pointer check.
          {
             pecit=pinfo->ectables[i];
             while(pecit)
             { 
                memcpy(&ptables[ecitem_num],pecit,sizeof(*pecit)); 
                ecitem_num++;
                pecit=pecit->next;
             }
          }
       }
       err=copy_to_user(ifr->ifr_data,ptables,ecitem_num*sizeof(struct ecitem));
       if(err)
       {
          CDBG("copy_to_user fails!\n");
          return err;
       }
       
       kfree(ptables);
   }
   else if(cmd==TUNNEL_DEL_ALL_MAPPING)//we just delete all the mapping(including mac mapping)
   {
       lw4over6_ecitem_free(netdev);
   }
   else if(cmd==TUNNEL_SET_MAPPING)
   {
      pecit=(struct ecitem*)kmalloc(sizeof(struct ecitem),GFP_KERNEL);
      err=copy_from_user(pecit,ifr->ifr_data,sizeof(struct ecitem));
      if(err)
      {
         CDBG("copy_from_user fails!\n");
         return err;
      }
      do_gettimeofday(&pecit->start_time);
      lw4over6_ecitem_set(netdev,pecit);
   }
   else if(cmd==TUNNEL_DEL_MAPPING)
   {
      pecit=(struct ecitem*)kmalloc(sizeof(struct ecitem),GFP_KERNEL);
      err=copy_from_user(pecit,ifr->ifr_data,sizeof(struct ecitem));
      if(err)
      {
         CDBG("copy_from_user fails!\n");
         return err;
      }
      pecit=lw4over6_ecitem_unlink(netdev,&pecit->remote, pecit->pset_index, pecit->pset_mask, 1);//we just delete mapping item whose tag is 1(manual mapping item)
      if(pecit)
        kfree(pecit);//do not forget kfree
   }
   else if(cmd==TUNNEL_GET_BINDING)
   {
      pinfo=(struct lw4over6_tunnel_private*)netdev_priv(dev);
      err=copy_to_user(ifr->ifr_data,pinfo,sizeof(struct lw4over6_tunnel_private));
      if(err)
      {
         CDBG("copy_to_user fails!\n");
         return err;
      }
   }
   else if(cmd==TUNNEL_SET_BINDING)
   {
      pinfo=(struct lw4over6_tunnel_private*)netdev_priv(dev);
      err=copy_from_user(pinfo->ifname,ifr->ifr_data,IFNAMSIZ);
      if(err)
      {
         CDBG("copy_from_user fails!\n");
         return err;
      }
      pinfo->local6=*(get_ipv6_addr(pinfo->ifname));//get the first IPv6 from the binded interface.
      
   }
   //More IOCTLs.
   
   return 0;
}



static int tunnel_change_mtu(struct net_device* dev, int new_mtu)
{
    if ((new_mtu<68)||(new_mtu>1500))
    {
        return -EINVAL;
    }
    dev->mtu = new_mtu;
    return 0;
}

void inet_pton_v6(char* addr,struct in6_addr* dst)
{
     const char xdigits[]="0123456789abcdef";
     u_char* ptr=dst->s6_addr;
     int i,j;
     int index;
     int sum;
     for (i=0;i<16;i++)
     {
    sum=0;
    for (j=0;j<2;j++)
    {
       index=0;
       while (addr[i*2+j]!=xdigits[index]&&index<16) index++;
       CDBG("index=%d\n",index);
       sum*=16;
       sum+=index;
    }
    ptr[i]=(u_char)sum;
    CDBG("in inet_pton_v6 , sum=%d\n",(int)sum);
     }
    CDBG("out inet_pton_v6 \n");
}


char* inet_ntop_v6(struct in6_addr addr, char* dst)
{
    const char xdigits[] = "0123456789abcdef";
    int i;
    const u_char* ptr = addr.s6_addr;

    for ( i = 0; i < 8; ++i )
    {
        int non_zerop = 0;

        if ( non_zerop || ( ptr[0] >> 4 ) )
        {
            *dst++ = xdigits[ptr[0] >> 4];
            non_zerop = 1;
        }
        if ( non_zerop || ( ptr[0] & 0x0F ) )
        {
            *dst++ = xdigits[ptr[0] & 0x0F];
            non_zerop = 1;
        }
        if ( non_zerop || ( ptr[1] >> 4 ) )
        {
            *dst++ = xdigits[ptr[1] >> 4];
            non_zerop = 1;
        }
        *dst++ = xdigits[ptr[1] & 0x0F];
        if ( i != 7 )
        {
            *dst++ = ':';
        }
        ptr += 2;
    }
    *dst++ = 0;
    return dst;
}

char* inet_ntoa(struct in_addr in, char* buf, size_t* rlen)
{
    int i;
    char* bp;

    /*
     * This implementation is fast because it avoids sprintf(),
     * division/modulo, and global static array lookups.
     */

    bp = buf;
    for (i = 0;i < 4; i++ )
    {
        unsigned int o, n;
        o = ((unsigned char*)&in)[i];
        n = o;
        if ( n >= 200 )
        {
            *bp++ = '2';
            n -= 200;
        }
        else if ( n >= 100 )
        {
            *bp++ = '1';
            n -= 100;
        }
        if ( o >= 10 )
        {
            int i;
            for ( i = 0; n >= 10; i++ )
            {
                n -= 10;
            }
            *bp++ = i + '0';
        }
        *bp++ = n + '0';
        *bp++ = '.';
    }
    *--bp = 0;
    if ( rlen )
    {
        *rlen = bp - buf;
    }

    return buf;
}


static inline void ipgre_ecn_decapsulate(struct iphdr *iph, struct sk_buff *skb)
{
   if (INET_ECN_is_ce(iph->tos)) 
   {
      if (skb->protocol == htons(ETH_P_IP)) 
     {
        IP_ECN_set_ce(ip_hdr(skb));
     } 
     else if (skb->protocol == htons(ETH_P_IPV6)) 
     {
        IP6_ECN_set_ce(ipv6_hdr(skb));
     }
  }
}


#define IP6_TNL_F_RCV_DSCP_COPY 0x10
static inline void ip6ip_ecn_decapsulate(struct net_device* dev, struct ipv6hdr *ipv6h, struct sk_buff* skb )
{ 
    __u8 dsfield = ipv6_get_dsfield(ipv6h) & ~INET_ECN_MASK;

    if (dev->flags & IP6_TNL_F_RCV_DSCP_COPY)
        ipv4_change_dsfield(ip_hdr(skb), INET_ECN_MASK, dsfield);

    if (INET_ECN_is_ce(dsfield))
        IP_ECN_set_ce(ip_hdr(skb));

}


int lw4over6_rcv(struct sk_buff *skb)
{
 
    struct ipv6hdr *ipv6h;
    struct iphdr *iph;
    int err;
    struct ecitem *pect=0,*ptmp;
    struct net_device *ndev=dev_get_by_name(dev_net(skb->dev),TUNNEL_DEVICE_NAME);  
    struct net_device_stats *stats=&ndev->stats;
    char buff[512];
    if(!pskb_may_pull(skb, sizeof(struct iphdr)))
    {
       stats->rx_errors++;  
       CDBG("lw4over6_rcv: error in decapsulating a packet! skb->len=%d\n", skb->len);
       dev_put(ndev);
       goto rcv_error;
    }
    CDBG("lw4over6_rcv: receiving a packet skb->len=%d\n", skb->len);
    stats->rx_packets++;
    stats->rx_bytes+=skb->len;
    dev_put(ndev);
    ipv6h=ipv6_hdr(skb);
    /* print the addresses if needed */
#ifdef DEBUG_lw4over6_
    inet_ntop_v6(ipv6h->saddr,buff);
    CDBG("the source IPv6 address is %s\n",buff);
    inet_ntop_v6(ipv6h->daddr,buff);
    CDBG("the destination IPv6 address is %s\n",buff);
#endif
    skb->mac_header = skb->network_header;
    skb_reset_network_header(skb);//skb->network_header = skb->data;
    skb->protocol=htons(ETH_P_IP);
    skb->pkt_type = PACKET_HOST;
    skb->dev=dev_get_by_name(&init_net,TUNNEL_DEVICE_NAME);
    if (skb->dev) dev_put(skb->dev);
    memset(skb->cb,0, sizeof(struct inet6_skb_parm));
    /* search for suitable ecitem */
    iph=ip_hdr(skb);
    ////[pset]////
    unsigned short portNum_dest, portNum_src;
    portNum_src = get_portNum_src(skb, 1);
    pect=lw4over6_ecitem_lookup(netdev,(struct in_addr*)&iph->saddr, portNum_src);
    portNum_dest = get_portNum_dest(skb, 1);
    ptmp=lw4over6_ecitem_lookup(netdev,(struct in_addr*)&iph->daddr, portNum_dest);
    //for basic flow statistics.
    if(pect)
    {
       pect->outbound_bytes+=skb->len;
       pect->out_pkts++;
    }
    if(ptmp)
    {
       ptmp->inbound_bytes+=skb->len;
       ptmp->in_pkts++;
    } 
    skb_dst_drop(skb);
    nf_reset(skb);
    ip6ip_ecn_decapsulate(skb->dev,ipv6h,skb);
    err=netif_rx(skb); 
    if(err)
    {
        CDBG("lw4over6_rcv: decaped packet is dropped\n");
        goto rcv_error;
    }
    else
        CDBG("lw4over6_rcv: finish decapsulating packet\n" );
    return 0;
rcv_error: 
    dev_kfree_skb(skb);//kfree_skb(skb);
    return 0;
}

static struct inet6_protocol ip6ip_protocol={
  .handler = lw4over6_rcv,
  .err_handler = lw4over6_err,
  .flags = INET6_PROTO_NOPOLICY |INET6_PROTO_FINAL,
};

static const struct net_device_ops ip6ip_netdev_ops = {
    //.ndo_init        = tunnel_init,
    //.ndo_uninit        = ipgre_tunnel_uninit,
    .ndo_open        = tunnel_open,
    .ndo_stop        = tunnel_close,
    .ndo_start_xmit        = lw4over6_tunnel_xmit,
    .ndo_do_ioctl        = tunnel_ioctl,
    .ndo_change_mtu        = tunnel_change_mtu,
};

static void tunnel_setup(struct net_device *dev)
{   
    //Hook Function.
    //dev->uninit = tunnel_uninit;
    dev->netdev_ops = &ip6ip_netdev_ops;
    dev->destructor = free_netdev;
    dev->type = ARPHRD_TUNNEL6;
    dev->needed_headroom = LL_MAX_HEADER + sizeof(struct ipv6hdr);
    dev->mtu = ETH_DATA_LEN-sizeof (struct ipv6hdr);  //  dev->mtu = 1500 - sizeof( struct ipv6hdr );
    dev->flags |= IFF_NOARP|IFF_BROADCAST; 
    //dev->get_stats = tunnel_get_stats;
    //dev->neigh_setup = tunnel_neigh_setup_dev;
    dev->hard_header_len =14; //  dev->hard_header_len = 14;
    dev->iflink = 0;
    dev->addr_len = 6;//ethernet mac address 
    dev->features |= NETIF_F_NETNS_LOCAL; 
}


static int __init tunnel_init(void)
{   
    struct lw4over6_tunnel_private *pinfo;
    struct in6_addr *plocal6;
    int err = -ENOMEM;
    /* allocate memory for device private info */
    netdev = alloc_netdev(sizeof(struct lw4over6_tunnel_private),TUNNEL_DEVICE_NAME,tunnel_setup);
    if(!netdev)
    {
        CDBG("tunnel_init: alloc_netdev failed: %i\n",err);
        return err;
    }
    strcpy(netdev->name,TUNNEL_DEVICE_NAME);
    memset(netdev_priv(netdev),0,sizeof(struct lw4over6_tunnel_private));
    if((err=register_netdev(netdev)))
    {
        CDBG("tunnel_init: register_netdev failed: %i\n",err);
        free_netdev(netdev);
        return err;
    }
    if(inet6_add_protocol(&ip6ip_protocol,IPPROTO_IPIP) < 0)
    {
        CDBG("tunnel_init: inet6_add_protocol failed\n" );
        err=-EAGAIN;
        free_netdev(netdev);
        return err;
    }
    pinfo=(struct lw4over6_tunnel_private*)(netdev_priv(netdev));
    pinfo->dev=netdev;
    /* get an ipv6 address as the local tunnel point */
    memcpy(pinfo->ifname,TUNNEL_BIND_IFACE,(int)strlen(TUNNEL_BIND_IFACE));
    plocal6=get_ipv6_addr(pinfo->ifname);
    if (plocal6)
    {
        pinfo->local6=*plocal6;
        CDBG("tunnel_init: initialize\n");
    }
    return 0;
}

static void __exit tunnel_exit(void)
{   
    //never forget to delete the mapping.
    lw4over6_ecitem_free(netdev);
    unregister_netdev(netdev);
    if(inet6_del_protocol( &ip6ip_protocol,IPPROTO_IPIP)<0)
    {
        CDBG("Can't remove the protocol!\n");
    }
    CDBG("tunnel_exit: finalize\n");
}

module_init(tunnel_init);
module_exit(tunnel_exit);
MODULE_LICENSE("Dual BSD/GPL"); 

