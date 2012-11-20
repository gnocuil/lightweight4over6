#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
 
#include "public4over6.h"


void set_tunnel(struct in6_addr remote,struct in6_addr local)
{
    tunnel_info_t info;
    struct ifreq req;
    int sock;
    sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);
    if(sock<0)
    {
       fprintf(stderr,"Can't create io control socket for interface %s\n",TUNNEL_DEVICE_NAME);
       return ;
    }
    memset(&info,0,sizeof(info));  
    memset(&req,0,sizeof(struct ifreq));
    info.type=TUNNEL_SET;
    info.daddr=remote;
    info.saddr=local;
    strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
    req.ifr_data=(caddr_t)&info;
    if(ioctl(sock,TUNNELMESSAGE,&req)<0)
    {
       fprintf(stderr,"cannot send message!\n");
    }
    close(sock);
}

void show_tunnel()
{
   tunnel_info_t info;
   struct ifreq req;
   char ti_addr6[INET6_ADDRSTRLEN],tc_addr6[INET6_ADDRSTRLEN];
   int sock;
   sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);
   if(sock<0)
   {
      fprintf(stderr,"Can't create io control socket for interface %s\n",TUNNEL_DEVICE_NAME);
      return ;
   }
   memset(&info,0,sizeof(info));  
   memset(&req,0,sizeof(struct ifreq));
   info.type=TUNNEL_INFO;
   strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
   req.ifr_data=(caddr_t)&info;
   if(ioctl(sock,TUNNELMESSAGE,&req)<0)
   {
      fprintf(stderr,"cannot send message!\n");
   }
   close(sock);
   inet_ntop(AF_INET6,(void*)&info.saddr,ti_addr6,INET6_ADDRSTRLEN);
   inet_ntop(AF_INET6,(void*)&info.daddr,tc_addr6,INET6_ADDRSTRLEN);
   printf("%-48s%-48s\n","TI IPv6 Addr","TC IPv6 Addr");
   printf("%-48s%-48s\n",ti_addr6,tc_addr6);
}

void del_tunnel()
{

}

void set_mtu(int newmtu)
{
   tunnel_info_t info;
   struct ifreq req;
   int sock;
   sock=socket(AF_INET,SOCK_RAW,IPPROTO_RAW);
   if(sock<0)
   {
      fprintf(stderr,"Can't create io control socket for interface %s\n",TUNNEL_DEVICE_NAME);
      return ;
   }
   memset(&info,0,sizeof(info));  
   memset(&req,0,sizeof(struct ifreq));
   info.type=TUNNEL_SET_MTU;
   info.mtu=newmtu;
   strcpy(req.ifr_name,TUNNEL_DEVICE_NAME);
   req.ifr_data=(caddr_t)&info;//new mtu value.
   if(ioctl(sock,TUNNELMESSAGE,&req)<0)
   {
      fprintf(stderr,"cannot send message!\n");
   }
   close(sock);
}

//display user information
void use_info()
{
   printf("./ioctl [-h|-help] | [-a ti_addr6 tc_addr6 | [-b ] | [-c mtu] | [-m]\n");
   printf("-h|-help : display user information!\n");
   printf("-a : set mapping!\n");
   printf("-b : del mapping!\n");
   printf("-c mtu: set mtu!\n");
   printf("-m : display mapping!\n");
   return ;
}

int main(int argc,char *argv[])
{
   struct in6_addr ti_addr6,tc_addr6;
   int index=2;
   if(argc==1)
      use_info();
   while(index<=argc)
   {
      if(strncmp(argv[index-1],"-help",5)==0 || strncmp(argv[index-1],"-h",2)==0)
      {
          use_info();
          break;
      }
      else if(strncmp(argv[index-1],"-a",2)==0)
      {
          if(index-1+2>=argc)
          {
             printf("sorry,argument is wrong!\n");
             break;
          }
          
          inet_pton(AF_INET6,argv[index],&tc_addr6);
          inet_pton(AF_INET6,argv[index+1],&ti_addr6);
 
          set_tunnel(tc_addr6,ti_addr6);
          break;
      }
      else if(strncmp(argv[index-1],"-b",2)==0)
      {
          del_tunnel();
          break;
      }
      else if(strncmp(argv[index-1],"-c",2)==0)
      {
          if(index-1+1>=argc)
          {
             printf("sorry,argument is wrong!\n");
             break;
          }
          int newmtu=atoi(argv[index]);
          set_mtu(newmtu);
          break;
      }
      else if(strncmp(argv[index-1],"-s",2)==0)
      {
          break;
      }  
      else if(strncmp(argv[index-1],"-m",2)==0)
      {
          show_tunnel();
          break;
      }
      else
         break;
   }
   return 0;	
}
