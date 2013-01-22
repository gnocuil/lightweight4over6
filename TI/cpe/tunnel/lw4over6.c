#include <net/protocol.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/arp.h>
#include <net/inet_ecn.h>
#include <linux/etherdevice.h>

#include <net/icmp.h>
#include "lw4over6.h"


//#define DEBUG_LW4OVER6_


#ifdef DEBUG_LW4OVER6_
#define CDBG(msg,args...) printk(KERN_DEBUG msg,##args)
#else
#define CDBG(msg,args...) {}
#endif



#ifndef NF_IP6_LOCAL_OUT
#define NF_IP6_LOCAL_OUT 3
#endif

struct ectable 
{
   struct in6_addr remote6,local6;
};

char* inet_ntoa( struct in_addr in, char *buf, size_t *rlen);
int xmit( struct sk_buff *skb, struct in6_addr out_addr,struct net_device *dev);
char* inet_ntop_v6(struct in6_addr,char *dst);
void inet_pton_v6(char *,struct in6_addr *);
static int lw4over6_change_mtu(struct net_device* dev,int new_mtu);
static void lw4over6_setup(struct net_device *dev);

static struct net_device *netdev;

static portset_t portset;

struct tunnel_private
{
    struct net_device_stats stat;
    struct net_device *dev;
    struct ectable ect;
};

 
int tunnel_open( struct net_device *dev)
{
    //MOD_INC_USE_COUNT;
    return 0;
}

int tunnel_close( struct net_device *dev)
{
    //MOD_DEC_USE_COUNT;
    return 0;
}


/* Generate a random Ethernet address (MAC) that is not multicast
 * and has the local assigned bit set.
 */
void generate_random_hw(struct net_device *dev)
{
    //printk(KERN_INFO TUNNEL_DEVICE_NAME"generate_random_hw\n" );
    unsigned char tmp;
    struct net_device *pdev=__dev_get_by_name(&init_net,"eth0");
    if (pdev == NULL) {
        get_random_bytes(dev->dev_addr, ETH_ALEN);
        dev->dev_addr[0] &= 0xfe;	// clear multicast bit 
        dev->dev_addr[0] |= 0x02;	//set local assignment bit (IEEE802)
        //printk(KERN_INFO TUNNEL_DEVICE_NAME"generate_random_hw 2\n" );
    } else {
        memcpy(dev->dev_addr,pdev->dev_addr,6);
        tmp=dev->dev_addr[1];
        dev->dev_addr[1]=dev->dev_addr[4];
        dev->dev_addr[4]=tmp;
    }
}

//[pset]get portNum func
unsigned short get_portNum_dest(struct sk_buff* skb)
{
    int protoff;
    struct tcphdr *tcph;
    struct udphdr *udph;
    struct iphdr *iph;
    
    iph = ip_hdr(skb);
    protoff = iph -> ihl * 4;
    if(iph->protocol == IPPROTO_TCP){
	tcph=skb_header_pointer(skb, protoff, sizeof(tcph), &tcph); 
	return ntohs(tcph->dest);
    }else if(iph->protocol == IPPROTO_UDP){
	udph=skb_header_pointer(skb, protoff, sizeof(udph), &udph);
	return ntohs(udph->dest);
    } 
    
}

//[pset] get source port number
unsigned short get_portNum_src(struct sk_buff* skb)
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
    } 
}

static void
icmp_checksum(char *iphead, int32_t old_id, int32_t new_id)
{
	struct iphdr *iph = (struct iphdr *)iphead;
	struct icmphdr *icmph = (struct icmphdr *)(iphead+(iph->ihl<<2));
	//uint16_t icmp_len = ntohs(iph->tot_len) - (iph->ihl<<2);
	//printk("<7> [ICMP checksum] \ticmp_len=%d\n", icmp_len);
	
	/* from rfc1631 */
	int32_t x = (~ntohs(icmph->checksum)) & 0xffff;
	printk("<7> rechecksum: x=%x old=%x new=%x\n", x, old_id, new_id);
	x -= old_id & 0xffff;
	if (x <= 0) {
		x--;
		x &= 0xffff;
	}
	x += new_id & 0xffff;
	if (x & 0x10000) {
		x ++;
		x &= 0xffff;
	}
	icmph->checksum = ntohs(~((uint16_t)x));
/*	
	icmph->checksum = 0;
	uint16_t *p = (uint16_t*)icmph;
	uint32_t checksum = 0;
	while (icmp_len > 1) {
		checksum += *(p++);
		icmp_len -= 2;
	}
	if (icmp_len) {
		checksum += *((uint8_t*)p) ;
	}
	
	do {
		checksum = (checksum >> 16) + (checksum & 0xFFFF);
	} while (checksum != (checksum & 0xFFFF));
	icmph->checksum = ~((uint16_t)checksum);
*/
}

static uint16_t ping_hashtable[65536];

static uint16_t
ping_dohash(uint16_t id)
{
	uint32_t h = id;
	h *= id;
	h = (h >> 8) & 0xFFFF;
	return portset.index | (h & ~portset.mask);
}

static void
handle_ping(char* ipv4header)
{
	struct iphdr *iph = (struct iphdr *)ipv4header;
	if (iph->protocol != IPPROTO_ICMP)
		return;
	struct icmphdr *icmph = (struct icmphdr *)(ipv4header+(iph->ihl<<2));
	if (icmph->type != 8 && icmph->type != 0)
		return;

	uint16_t old_id = ntohs(*(uint16_t*)(((uint8_t*)icmph) + 4));
	uint16_t new_id;

	if (icmph->type == 8) {//echo request
		printk("<7> [ICMP send] old_id=%x\n", old_id);
		new_id = ping_dohash(old_id);
		printk("<7> [ICMP send] \tnewid=%x\n", new_id);
		//*(uint16_t*)(((uint8_t*)icmph) + 4) = ntohs(newid);
		ping_hashtable[new_id] = old_id;
	} else {//echo reply
		printk("<7> [ICMP recv] id=%x\n", old_id);
		new_id = ping_hashtable[old_id];
		printk("<7> [ICMP recv] \toldid=%x\n", new_id);
		//*(uint16_t*)(((uint8_t*)icmph) + 4) = ntohs(ping_hashtable[id]);
	}	
	icmp_checksum((char*)iph, (old_id), (new_id));
	*(uint16_t*)(((uint8_t*)icmph) + 4) = ntohs(new_id);
}


/*
   Main process routine
   if we use the way of invoking xmit() to send the package,
   then we still need some changes.
*/
int lw4over6_tunnel_xmit(struct sk_buff *skb, struct net_device *dev)
{
//printk("<7> [liucong]lw4over6_tunnel_xmit! skb->protocol=%04x skb->len=%d", htons(skb->protocol),skb->len);
//printk("<7> [liucong]data_len=%d\n", htons(skb->data_len));
//printk("<7> [liucong]mac_len=%d\n", (skb->mac_len));
//printk("<7> [liucong]mac_header=%x\n", (skb->mac_header));
 int i;
// for (i = 0; i < 20; ++i) printk("<7> [liucong]\thead:[%x]=%02x", &(skb->head[i]), skb->head[i]);
// for (i = 0; i < 20; ++i) printk("<7> [liucong]\tdata:[%x]=%02x", &(skb->data[i]), skb->data[i]);

	
    struct iphdr *iph;
    struct in6_addr src_addr,out_addr;
    char buf[128];
    struct in_addr daddr,saddr;
    unsigned int head_room;
    struct ipv6hdr *ip6h;
    struct flowi fl;//route key
    struct dst_entry *dst;	
    struct ectable *ect;
    struct net_device_stats *stats=&dev->stats;//for statistics
    iph = ip_hdr(skb);
    daddr.s_addr = iph->daddr;
    saddr.s_addr = iph->saddr;
    
    //handle_ping((char*)iph);
    
    if (skb->protocol!= htons(ETH_P_IP)) //judgement of IP protocol
    {
    	CDBG("[lw 4over6 tunnel]:this is not IPv4 protocol!\n");
	CDBG("[lw 4over6 tunnel]:this is not IPv4 protocol this is %d!\n",ntohs(skb->protocol));
        goto tx_error;
    }

    //for debug information
    #ifdef DEBUG_LW4OVER6_
    inet_ntoa(saddr,buf,NULL);
    CDBG("[lw 4over6 tunnel]:the source IPv4 address is %s!\n",buf);
    inet_ntoa(daddr,buf,NULL);
    CDBG("[lw 4over6 tunnel]:the destination IPv4 address is %s!\n",buf);
    #endif


    if (get_portNum_dest(skb) == 67) {
         CDBG("[lw 4over6 tunnel]: Drop all dhcp packet!!!!!\n");
         return 0;
    }    

    ect=&(((struct tunnel_private*)netdev_priv(dev))->ect);
    //inet_pton_v6(local,&(ect->local6));
    //inet_pton_v6(remote,&(ect->remote6));
    if (ect==0)
    {
        CDBG("[lw 4over6 tunnel]: Can not find data destination in ECT!\n");
        goto tx_error;
    }

    src_addr=ect->local6;
    out_addr=ect->remote6;
         
    head_room = sizeof(struct ipv6hdr)+16;
    if (skb_headroom(skb)<head_room || skb_cloned(skb)||skb_shared(skb))
    {
       struct sk_buff* new_skb=skb_realloc_headroom(skb,head_room);
       if (!new_skb)
       {
          goto tx_error;
       }
       if (skb->sk)
       {
          skb_set_owner_w(new_skb,skb->sk);
       }
       dev_kfree_skb(skb);
       skb=new_skb;
     }
     
     skb_push(skb,sizeof(struct ipv6hdr));
     skb->transport_header=skb->network_header;
     skb_reset_network_header(skb);//skb->network_header=skb->data
     
     
     //free the old router entry
     //dst_release(skb->dst);
     //skb->dst = NULL;
     skb_dst_drop(skb);
     
     //Fill the IPv6 header
     ip6h = ipv6_hdr(skb);
     memset(ip6h,0,sizeof(struct ipv6hdr));
     ip6h->version = 6;
     ip6h->priority = 0;
     ip6h->payload_len = htons(skb->len-sizeof( struct ipv6hdr));
     ip6h->nexthdr=IPPROTO_IPIP;//IPv4 over IPv6 protocol
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
     dst = ip6_route_output(dev_net(dev),NULL, &fl);
     skb_dst_set(skb,dst_clone(dst));
     nf_reset(skb);

     if (!skb_dst(skb))
     {
       CDBG("[lw 4over6 tunnel]:Cannot find route information for packet!\n");
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
     }
     return 0;       
tx_error : 
     dev_kfree_skb(skb);
     return 0;
}



void lw4over6_err( struct sk_buff* skb,struct inet6_skb_parm* opt,int type,int code,int offset,__u32 info)
{
    CDBG("[lw 4over6 tunnel]:IP6IP Error! (type=%d, code=%d, offset=%d).\n",type,code,offset);
}

//IOCTL function
int lw4over6_ioctl(struct net_device *dev,struct ifreq *ifr,int cmd)
{printk("<7> [liucong]lw4over6_ioctl!");
    tunnel_info_t tunnelinfo;
    tunnel_info_t *p_tunnelinfo=NULL;
    portset_t *p_portset = NULL;
    
    char *addr=dev->dev_addr;
    int err;
    struct ectable *ect;
    ect=&(((struct tunnel_private*)netdev_priv(dev))->ect);
    if(!capable(CAP_NET_ADMIN))
    {
       return -EPERM;
    }
    if(cmd==TUNNELMESSAGE)
    {       
      p_tunnelinfo=(tunnel_info_t*)ifr->ifr_data;
      if(p_tunnelinfo->type==TUNNEL_SET)
      {
	  err=copy_from_user(&tunnelinfo,p_tunnelinfo,sizeof(tunnel_info_t));
	  if(err)
             return err;
	  //memcpy(addr,p_tunnelinfo->hw,6);
          ipv6_addr_copy(&ect->remote6,&p_tunnelinfo->daddr);
	  ipv6_addr_copy(&ect->local6,&p_tunnelinfo->saddr);             
      } 
      else if(p_tunnelinfo->type==TUNNEL_INFO)
      {
         /*
         err=copy_to_user(p_tunnelinfo,&tunnelinfo,sizeof(tunnel_info_t));
	 if(err)
            return err;
         */
         ipv6_addr_copy(&p_tunnelinfo->daddr,&ect->remote6);
         ipv6_addr_copy(&p_tunnelinfo->saddr,&ect->local6);
      }
      //more ioctls.
      else if(p_tunnelinfo->type==TUNNEL_SET_MTU)
      {
         
         int mtu;
         err=copy_from_user(&tunnelinfo,p_tunnelinfo,sizeof(tunnel_info_t));
         if(err)
         {
            CDBG("[lw 4over6 tunnel]:copy_from_user failed!\n");
            return err;
         }
         lw4over6_change_mtu(dev,p_tunnelinfo->mtu);
         printk("[lw 4over6 tunnel]:lw4over6 mtu is set to %d!\n",mtu);
      }
      else
      {
	 CDBG("[lw 4over6 tunnel]:command type error!%d\n",tunnelinfo.type);
      }
    }
    else if (cmd == TUNNELPORTSET)
    {
    	p_portset = (struct portset*)ifr->ifr_data;
    	portset = *p_portset;
    	printk("<7> [ICMP TUNNELPORTSET] port=%x mask=%x", portset.index, portset.mask);
    }
    else
    { 
       CDBG("[lw 4over6 tunnel]:command error!\n");
    }
    return 0;
}


static int lw4over6_change_mtu(struct net_device* dev,int new_mtu)
{
    if((new_mtu<68)||(new_mtu>1500))
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
    for(i=0;i<16;i++)
    {
       sum=0;
       for (j=0;j<2;j++)
       {
	  index=0;
	  while (addr[i*2+j]!=xdigits[index]&&index<16) index++;
	  sum*=16;
	  sum+=index;
       }
       ptr[i]=(u_char)sum;
    }
}


char* inet_ntop_v6(struct in6_addr addr, char* dst)
{
    const char xdigits[] = "0123456789abcdef";
    int i;
    const u_char* ptr = addr.s6_addr;

    for(i = 0; i < 8;++i)
    {
       int non_zerop = 0;

       if (non_zerop || ( ptr[0] >>4))
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

char* inet_ntoa( struct in_addr in, char* buf, size_t* rlen )
{
   int i;
   char* bp;

   /*
    * This implementation is fast because it avoids sprintf(),
    * division/modulo, and global static array lookups.
   */

   bp = buf;
   for ( i = 0; i < 4; i++ )
   {
      unsigned int o, n;
      o = ( ( unsigned char * ) &in )[i];
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
    if (INET_ECN_is_ce(iph->tos)) {
	if (skb->protocol == htons(ETH_P_IP)) {
		IP_ECN_set_ce(ip_hdr(skb));
	} else if (skb->protocol == htons(ETH_P_IPV6)) {
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


int lw4over6_rcv( struct sk_buff *skb )
{//printk("<7> [liucong]lw4over6_rcv! skb->len=%d", skb->len);

    struct ipv6hdr* ipv6th;
    char buff[255];
    int err;
    struct net_device *ndev=dev_get_by_name(dev_net(skb->dev),TUNNEL_DEVICE_NAME);  
    struct net_device_stats *stats=&ndev->stats;
    
//    CDBG("[lw 4over6 tunnel]:receive packet:receiving!\n");
    stats->rx_packets++;
    stats->rx_bytes+=skb->len;

    if (!pskb_may_pull( skb, sizeof( struct ipv6hdr )))
    {
       stats->rx_errors++;  
       CDBG("[lw 4over6 tunnel]:receive packet: error decapsulating packet!\n");
       dev_put(ndev);
       goto rcv_error;
    }
  
    dev_put(ndev);
    ipv6th=ipv6_hdr(skb);
    
    //if (ipv6th->nexthdr == 0x04) {
    //	handle_ping((char*)ipv6th + 40);
    //}
    
    
    #ifdef DEBUG_LW4OVER6_
    inet_ntop_v6(ipv6th->saddr, buff);
    CDBG("[lw 4over6 tunnel]:receive packet:the source address is %s.\n" ,buff);
    inet_ntop_v6(ipv6th->daddr,buff);
    CDBG("[lw 4over6 tunnel]:receive packet:the destination address is %s.\n" ,buff);
    #endif
    
    skb_reset_network_header(skb);
    skb->protocol = htons(ETH_P_IP);
    skb->pkt_type = PACKET_HOST; 
    memset( skb->cb, 0, sizeof( struct inet6_skb_parm));
    skb->dev =__dev_get_by_name( &init_net,TUNNEL_DEVICE_NAME);
    
    //for debug info only.
    CDBG("[lw 4over6 tunnel]:skb->len is %d,skb->data_len is %d,skb->hdr_len is %d,skb->mac_len is %d!\n",skb->len,skb->data_len,skb->hdr_len,skb->mac_len);
    skb->hdr_len=14;

    //dst_release(skb->dst);
    //skb->dst = NULL;
    skb_dst_drop(skb);
    nf_reset(skb);
    #ifdef CONFIG_NETFILTER
    nf_conntrack_put(skb->nfct);
    skb->nfct = NULL;
    #ifdef CONFIG_NETFILTER_DEBUG
    skb->nf_debug = 0;
    #endif
    #endif
   
   ip6ip_ecn_decapsulate(skb->dev,ipv6th, skb);
   err=netif_rx(skb);
   if (err)
   {
      CDBG("[lw 4over6 tunnel]:decaped packet is dropped!\n");
      goto rcv_error;
   }
   else
      CDBG("[lw 4over6 tunnel]:finish decapsulating packet!\n" );
   return 0;
rcv_error:
   dev_kfree_skb(skb);
   return 0;
}



static struct inet6_protocol ip6ip_protocol = {
  .handler = lw4over6_rcv,
  .err_handler = lw4over6_err,
  .flags = INET6_PROTO_NOPOLICY |INET6_PROTO_FINAL,
 };


static const struct net_device_ops ip6ip_netdev_ops = {
	//.ndo_init		= tunnel_init,
	//.ndo_uninit		= ipgre_tunnel_uninit,
	.ndo_open		= tunnel_open,
	.ndo_stop		= tunnel_close,
	.ndo_start_xmit		= lw4over6_tunnel_xmit,
	.ndo_do_ioctl		= lw4over6_ioctl,
	.ndo_change_mtu		= lw4over6_change_mtu,
};

//Module initialize
static int __init lw4over6_init(void)
{
  int err;
  netdev = alloc_netdev( sizeof(struct tunnel_private),TUNNEL_DEVICE_NAME,lw4over6_setup);
    strcpy(netdev->name,TUNNEL_DEVICE_NAME);
    memset(netdev_priv(netdev), 0, sizeof(struct tunnel_private));
    if((err=register_netdev(netdev)))//register the device
    {
       CDBG("[lw 4over6 tunnel]:Can't register the device %s,error number is %i.\n",TUNNEL_DEVICE_NAME,err);
       return -EIO;
    }
    else
    {
        
    }
    if(inet6_add_protocol( &ip6ip_protocol,IPPROTO_IPIP)==0)//register the 4over6 protocol in IPv6 level.
    {
       CDBG("[lw 4over6 tunnel]:registered the 4over6 protocol!\n" );
    }
    else 
    {
       CDBG("[lw 4over6 tunnel]:can't register the 4over6 protocol!\n");
       unregister_netdev(netdev);
       return -1;  
    }
    CDBG("[lw 4over6 tunnel]:initialized.\n");
    return 0;
}



static void lw4over6_setup(struct net_device *dev)
{   
    //dev->uninit = tunnel_uninit;
    dev->flags|= IFF_NOARP|IFF_BROADCAST;
    dev->netdev_ops = &ip6ip_netdev_ops;
    dev->addr_len = 6;//ethernet mac address
    //dev->neigh_setup = tunnel_neigh_setup_dev;
    dev->destructor = free_netdev;
    //dev->iflink = 0;
    dev->features|= NETIF_F_NETNS_LOCAL; 
    //dev->type=ARPHRD_ETHER;
    dev->type=ARPHRD_TUNNEL6;
    dev->hard_header_len=14; //  dev->hard_header_len = 14;
    dev->mtu= ETH_DATA_LEN-sizeof(struct ipv6hdr); //dev->mtu = 1500 - sizeof( struct ipv6hdr );
    generate_random_hw(dev);//generate a random hardware address.
	portset.index = 0;
	portset.mask = 0;
}
 

void cleanup_module( void )
{
  unregister_netdev(netdev);
  if (inet6_del_protocol( &ip6ip_protocol,IPPROTO_IPIP)<0)
  {
     CDBG("[lw 4over6 tunnel]:Can't register the 4over6 protocol!\n");
  }
}

module_init(lw4over6_init);
MODULE_LICENSE("Dual BSD/GPL");


