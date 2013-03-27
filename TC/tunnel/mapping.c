#include<linux/netdevice.h>
#include<linux/in.h>
#include<linux/in6.h>
#include<net/ipv6.h>

#include"mapping.h"
#include"tunnel.h"

//compare two strings
int comp_string(unsigned char *source,unsigned char *dest,int size)
{
   int i=0; 
   for(i=0;i<size;i++)
   {  
      if(source[i]==dest[i])
         continue;
      else if(source[i]<dest[i])
         return -1;
      else
         return 1;
   }
   return 0;
}

int mask_bits_pset(unsigned short mask)
{
	if (mask == 0) return 0;
	int result = 16;
	while(mask && (mask & 1) == 0){
		--result;
		mask = mask >> 1;
	}
	return result;
}
//lookup the encapsulation item according remote IPv4 address + portNum [pset]
struct ecitem* lw4over6_ecitem_lookup(struct net_device *dev,struct in_addr *remote, unsigned short portNum)
{  
  struct ecitem *t;
   __be32 h0=remote->s_addr;
   int bits = 16;
   //unsigned key0=HASH(h0);//hash func should be changed
   unsigned key0;
   struct lw4over6_tunnel_private *priv=netdev_priv(dev);
/***bits change from 16 to 0 to find the hashed ecitem*/
   while(bits >= 0){
	key0 = HASH(h0, bits, portNum);
   	read_lock_bh(&lw4over6_lock);
   	for(t=priv->ectables[key0];t && remote ;t=t->next)
   	{
      		if(remote->s_addr==t->remote.s_addr && ((portNum & t->pset_mask) == t->pset_index))//pset
      		{
          		read_unlock_bh(&lw4over6_lock);
          		return t;
      		}
   	}
   	read_unlock_bh(&lw4over6_lock);
	bits--;
   }
   return NULL;
      
} 

//Just For Debug.
char* inet_ntop_ipv6(struct in6_addr addr, char *dst)
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
//lookup the encapsulation item according remote IPv6 address
struct ecitem* lw4over6_ecitem_lookup_by_ipv6(struct net_device *dev,struct in6_addr *remote6)
{  
   int i=0;
   //char addr1[255],addr2[255];
   unsigned char *s,*d;
   struct ecitem *pecitem,*pcur;
   struct lw4over6_tunnel_private *priv=netdev_priv(dev);
   read_lock_bh(&lw4over6_lock);
   for(i=0;i<HASH_SIZE;i++)
   {
      pecitem=priv->ectables[i];
      pcur=pecitem;
      while(pcur)
      {
         s=(unsigned char*)&pcur->remote6;
         d=(unsigned char*)remote6;
         //inet_ntop_ipv6(pcur->remote6,addr1);
         //printk("pcur->remote6 is %s\n",addr1);
         //inet_ntop_ipv6(*remote6,addr2);
         //printk("remote6 is %s\n",addr2);
         //if(comp_string(s,d,sizeof(struct in6_addr))==0)
         if(ipv6_addr_equal(&pcur->remote6,remote6))
         {
             read_unlock_bh(&lw4over6_lock);
             return pcur;
         }
         pcur=pcur->next;
      }
   }
   read_unlock_bh(&lw4over6_lock);
   return NULL;
      
} 

static struct ecitem* lw4over6_ecitem_lookup_pset(struct net_device *dev, struct in_addr *remote, unsigned short pset_index, unsigned short pset_mask)
{
   struct ecitem *t;
   __be32 h0=remote->s_addr;
   int bits;
   //unsigned key0=HASH(h0);
   unsigned key0;
   struct lw4over6_tunnel_private *priv=netdev_priv(dev);

   bits = mask_bits_pset(pset_mask);
   key0 = HASH(h0, bits, pset_index);
   read_lock_bh(&lw4over6_lock);
   for(t=priv->ectables[key0];t && remote ;t=t->next)
   {
      if(remote->s_addr==t->remote.s_addr && pset_index == t->pset_index && pset_mask == t->pset_mask)
      {
          read_unlock_bh(&lw4over6_lock);
          return t;
      }
   }
   read_unlock_bh(&lw4over6_lock);
   return NULL;

}

  
//set ecitem: add or modify; invoke the new lookup func.
void lw4over6_ecitem_set(struct net_device *dev,struct ecitem *pect)
{
   int i;
   pect->local6zero = 1;
   for (i = 0; i < 16; ++i)
       if (pect->local6.s6_addr[i]) {
           pect->local6zero = 0;
           break;
       }
           
   struct ecitem *p;
   p=lw4over6_ecitem_lookup_pset(dev,&pect->remote, pect->pset_index, pect->pset_mask);//pset
   if(!p)
      lw4over6_ecitem_link(dev,pect);//add a ecitem  
   else//modify the ecitem
   {   
       write_lock_bh(&lw4over6_lock);
       p->remote6 = pect->remote6;
       p->local6 = pect->local6;
	   write_unlock_bh(&lw4over6_lock);
       kfree(pect);
   }
}

//unlink the encapsulation item according IPv4 remote address + pset_index & pset_mask. Is it needed to check whether the ecitem exists?
struct ecitem* lw4over6_ecitem_unlink(struct net_device *dev,struct in_addr *remote, unsigned short pset_index, unsigned short pset_mask, int tag)
{
   __be32 h0=remote->s_addr;
   unsigned key0=HASH(h0, mask_bits_pset(pset_mask), pset_index);////pset///
   struct lw4over6_tunnel_private *priv=netdev_priv(dev);
   struct ecitem *t=NULL,*prev=NULL;
   for(t=priv->ectables[key0];t && remote;prev=t,t=t->next)
   {
      if(remote->s_addr==t->remote.s_addr && pset_index==t->pset_index && pset_mask==t->pset_mask && t->tag==tag)//tag match.
      {  
         write_lock_bh(&lw4over6_lock);
         if(prev==NULL)//unlink the first ecitem
         {
            prev=t;
            t=t->next;
            priv->ectables[key0]=t;//update the header pointer
         }
         else
         {
             prev->next=t->next;
             prev=t;
         }
         write_unlock_bh(&lw4over6_lock);
         return prev;
     }
  }
  return NULL;
}

//link the encapsulation item
void lw4over6_ecitem_link(struct net_device *dev,struct ecitem *ect)
{
   __be32 h0=ect->remote.s_addr;
   unsigned key0=HASH(h0, mask_bits_pset(ect->pset_mask), ect->pset_index);//pset
   struct lw4over6_tunnel_private *priv=netdev_priv(dev);
   if(lw4over6_ecitem_lookup_pset(dev,&ect->remote, ect->pset_mask, ect->pset_index)==NULL)
   {
      struct ecitem *t=priv->ectables[key0];
      write_lock_bh(&lw4over6_lock);
      ect->next=t;//ect->next=priv->ectables[key0];
      priv->ectables[key0]=ect;
      write_unlock_bh(&lw4over6_lock);
   }
}

//free all the ecitems
void lw4over6_ecitem_free(struct net_device *dev)
{
   struct lw4over6_tunnel_private *priv=netdev_priv(dev);
   struct ecitem *pecitem=NULL;
   struct ecitem *pcur=NULL,*pnext=NULL;
   int i=0;
   for(i=0;i<HASH_SIZE;i++)
   {
      pecitem=priv->ectables[i];
      pcur=pecitem;
      while(pcur)
      {
         write_lock_bh(&lw4over6_lock);
         pnext=pcur->next;
         kfree(pcur);
         pcur=pnext;
         write_unlock_bh(&lw4over6_lock);
      }
      priv->ectables[i]=pcur;
   }
}

