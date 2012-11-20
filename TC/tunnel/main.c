/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) zhang_daxia@126.com 2011 <zhangdaxia@>
 * 
 * main.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * main.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <time.h>
#include <memory.h>

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN  48
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16  
#endif

#include "tunnel.h"

//display tc_mapping_table for users.
//TUNNEL_MAPPING_NUM and TUNNEL_MAPPING_INFO
void display_tc_mapping_table()
{  
   struct ifreq req;
   int sock;
   int mapping_num=0,i=0;
   struct ecitem *pecitem;
   char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
   struct tm  *p;
   char addr[INET_ADDRSTRLEN],addr6[INET6_ADDRSTRLEN];
   sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);//raw socket in order to interact with tunnel
   if (sock<0)
   {
      fprintf(stderr,"Can't create io control socket for interface %s!\n",TUNNEL_DEVICE_NAME);
      return ;
   }
     
   memset(&req,0,sizeof(struct ifreq));
   strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
   req.ifr_data=(caddr_t)&mapping_num;//mapping item number.
   if(ioctl(sock,TUNNEL_MAPPING_NUM,&req)<0)
   {
     printf("can't send ioctl message TUNNEL_MAPPING_NUM!\n");
     return ;
   }
   mapping_num=*((int*)req.ifr_data);
   printf("TC has %d mapping item(s) in total!\n",mapping_num);
   if(mapping_num>0)
   {
   printf("%-16s%-25s%-15s%-20s%-22s%-10s%-10s%-10s%-10s%-10s","TI IPv4","TI IPv6","Port-Set Index","Port-Set Mask","Start Time","Time Lmt","InPkts","InBytes","OutPkts","OutBytes");
   printf("%-10s\n","MapType");
   //we will alloc memory for the ecitems which are to be displayed.
   pecitem=(struct ecitem*)malloc(mapping_num*sizeof(struct ecitem));
   if(!pecitem)
   {
      printf("malloc fails to allocate dynamic memory!\n");
      return ;
   }
   req.ifr_data=(caddr_t)pecitem;
   if(ioctl(sock,TUNNEL_MAPPING_INFO,&req)<0)
   {
     printf("can't send ioctl message TUNNEL_MAPPING_INFO!\n");
     return ;
   }
   pecitem=(struct ecitem*)(req.ifr_data);
   for(i=0;i<mapping_num;i++)
   {
      inet_ntop(AF_INET,(void*)&pecitem[i].remote,addr,INET_ADDRSTRLEN);
      inet_ntop(AF_INET6,(void*)&pecitem[i].remote6,addr6,INET6_ADDRSTRLEN);
      p=(struct tm*)localtime(&(pecitem[i].start_time.tv_sec));
      printf("%-16s%-25s0x%-15.04x0x%-15.04x%-4d/%-2d/%-2d",addr,addr6,pecitem[i].pset_index,pecitem[i].pset_mask,(1900+p->tm_year),(1+p->tm_mon),p->tm_mday); 
      printf("%-3s%-2d:%-2d:%-3d%-10d",wday[p->tm_wday],p->tm_hour,p->tm_min,p->tm_sec,pecitem[i].seconds);
      printf("%-10lld%-10lld%-10lld%-10lld",pecitem[i].in_pkts,pecitem[i].inbound_bytes,pecitem[i].out_pkts,pecitem[i].outbound_bytes);
      //for mapping type display.
      //if pecitem[i].tag==1,then this is manual;
      //if pecitem[i].tag==2,then this is auto.
      if(pecitem[i].tag==1)
        printf("%-10s\n","manual");
      else if(pecitem[i].tag==2)
        printf("%-10s\n","auto");
   }
   if(pecitem)
      free(pecitem);
   }
   close(sock);
   
}


//add or modify the ecitem.
//this is manual configuration for specified mapping items,so tag is 1.
//[pset]add index and mask; tag may cause prob
void set_mapping(struct in_addr remote,struct in6_addr remote6,unsigned short index, unsigned short mask,int seconds)
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
   itm.tag=1;//manual type,so we should do adjustment for the specified manual mapping item.
   itm.pset_index = index;
   itm.pset_mask = mask;
   if(seconds>0)
      itm.seconds=seconds;
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

//del the ecitem; pset: del ecitem with pset info
void del_mapping(struct in_addr remote, unsigned short index, unsigned short mask)
{
   struct ecitem itm;
   struct ifreq req;
   int sock;

   sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);//raw socket in order to interact with tunnel of public4over6.
   if (sock<0)
   {
      fprintf(stderr,"Can't create io control socket for interface %s!\n",TUNNEL_DEVICE_NAME);
      return ;
   }
   itm.remote=remote;
   itm.pset_index = index;
   itm.pset_mask = mask;
   itm.tag=1;//so we would like to delete manual mapping item through ioctl interface.
   memset(&req,0,sizeof(struct ifreq));
   strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
   req.ifr_data=(caddr_t)&itm;//mapping item
   if(ioctl(sock,TUNNEL_DEL_MAPPING,&req)<0)
   {
     printf("can't send ioctl message TUNNEL_SET_MAPPING!\n");
     return ;
   }
   close(sock);
}


//del all mapping(including mac mapping)
void del_all_mapping()
{
   struct ifreq req;
   int sock;

   sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);//raw socket in order to interact with tunnel
   if (sock<0)
   {
      fprintf(stderr,"Can't create io control socket for interface %s!\n",TUNNEL_DEVICE_NAME);
      return ;
   }
   memset(&req,0,sizeof(struct ifreq));
   strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
   req.ifr_data=(caddr_t)0;
   if(ioctl(sock,TUNNEL_DEL_ALL_MAPPING,&req)<0)
   {
     printf("can't send ioctl message TUNNEL_SET_MAPPING!\n");
     return ;
   }
   close(sock);
}

//Get Tunnel Encapsulation IPv6.
void get_tunnel_ipv6()
{
   struct ifreq req;
   struct public4over6_tunnel_private info;
   struct in6_addr tunnel_ipv6;
   char addr6[INET6_ADDRSTRLEN];
   int sock;
   sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);//raw socket in order to interact with tunnel
   if (sock<0)
   {
      fprintf(stderr,"Can't create io control socket for interface %s!\n",TUNNEL_DEVICE_NAME);
      return ;
   }
   memset(&req,0,sizeof(struct ifreq));
   strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
   req.ifr_data=(caddr_t)&info;
   if(ioctl(sock,TUNNEL_GET_BINDING,&req)<0)
   {
     printf("can't send ioctl message TUNNEL_GET_BINDING!\n");
     return ;
   }
   info=*(struct public4over6_tunnel_private*)req.ifr_data;
   tunnel_ipv6=info.local6;
   inet_ntop(AF_INET6,(void*)&tunnel_ipv6,addr6,INET6_ADDRSTRLEN);
   printf("TC binds %s, TC encapsulation IPv6 is: %-40s",info.ifname,addr6);
   close(sock);
}

void set_tunnel_bind_iface(char ifname[])
{
   struct ifreq req;
   int sock;
   sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);//raw socket in order to interact with tunnel
   if (sock<0)
   {
      fprintf(stderr,"Can't create io control socket for interface %s!\n",TUNNEL_DEVICE_NAME);
      return ;
   }
   memset(&req,0,sizeof(struct ifreq));
   strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
   req.ifr_data=(caddr_t)ifname;//interface name.
   if(ioctl(sock,TUNNEL_SET_BINDING,&req)<0)
   {
     printf("can't send ioctl message TUNNEL_SET_BINDING!\n");
     return ;
   }
   close(sock);
}
//display user information
void use_info()
{
   printf("./ioctl [-h|-help] | [-a source_ipv6 source_ipv4 PortSet_index PortSet_mask lease_time ] | [-b source_ipv4 PortSet_index PortSet_mask]\n");
   printf("[-c specified interface] | [-m]  | [-d] \n");
   printf("-h|-help : display user information!\n");
   printf("-a : set mapping!\n");
   printf("-b : del mapping!\n");
   printf("-c : bind the specified interface,such as eth0 or eth1!\n");
   printf("-s : show the tunnel encapsulation ipv6 addr!\n");
   printf("-m : display tc_mapping_table!\n");
   printf("-d : del all mapping(including mac mapping)!\n");
   return ;
}


//Main function.
int main(int argc,char *argv[])
{
    struct in_addr saddr;
    struct in6_addr saddr6;
    int index=2,seconds;
    unsigned short pset_index, pset_mask;
    
	if(argc==1)
        use_info();
    while(index<=argc)
    {
      if(strncmp(argv[index-1],"-help",5)==0 || strncmp(argv[index-1],"-h",2)==0)//help info
      {
          use_info();//display info for use interface
          break;
      }
      else if(strncmp(argv[index-1],"-a",2)==0)//set mapping of source4 source6
      /* [pset] set mapping of source6 source4 mask index time*/
      {
         if(index-1+4>=argc) //pset
         {
            printf("sorry,argument is wrong!\n");
            break;
         }
         seconds=atoi(argv[index+4]);//pset
	// pset_index = atoi(argv[index+2]);//pset_index
	// pset_mask = atoi(argv[index+3]);//pset_mask
		pset_index = (unsigned short)strtoul(argv[index + 2], 0, 0);
		pset_mask = (unsigned short)strtoul(argv[index + 3], 0, 0);
//		printf("0x%04x,0x%04x\n",pset_index,pset_mask);
		inet_pton(AF_INET,argv[index+1],&saddr);//source ipv4 address
		inet_pton(AF_INET6,argv[index],&saddr6);//source ipv6 address
		set_mapping(saddr,saddr6,pset_index,pset_mask,seconds);
         break;
      }
      else if(strncmp(argv[index-1],"-b",2)==0)
      {//pset: need modification
  	 printf("argc = %d\n",argc);
         if(index-1+3>=argc)
         {
            printf("sorry,argument is wrong!\n");
            break;
         }
         inet_pton(AF_INET,argv[index],&saddr);
	 pset_index = (unsigned short)strtoul(argv[index+1], 0, 0);
	 pset_mask = (unsigned short)strtoul(argv[index+2], 0, 0);
         del_mapping(saddr, pset_index, pset_mask);
         break;
      }
      else if(strncmp(argv[index-1],"-c",2)==0)
      {
         if(strncmp(argv[index],"eth0",4)==0 || strncmp(argv[index],"eth1",4)==0
            || strncmp(argv[index],"eth2",4)==0)
             set_tunnel_bind_iface(argv[index]);
         break;
      }
      else if(strncmp(argv[index-1],"-s",2)==0)
      {
         get_tunnel_ipv6();
         break;
      }
      else if(strncmp(argv[index-1],"-m",2)==0)//display tc_mapping_table
      {
         display_tc_mapping_table();
         break;
      }
      else if(strncmp(argv[index-1],"-d",2)==0)//del all mapping
      {
          del_all_mapping();
          break;
      }
      else
         break;
   }
   return 0;		
}
