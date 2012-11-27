#include "cra.h"

int main(int argc, char **argv)
{
	//Initialize some variables
        strcpy(TUNNEL_IFNAME,"lw4over6");
        strcpy(PHYSIC_IFNAME,"eth0");
        buffLen = BUFFLEN;
        status = INITIATE;

        device.sll_family = AF_PACKET;
        device.sll_halen = htons (6);

        strcpy(local6addr,"2001:da8:bf:19::7");
        strcpy(remote6addr,"2001:da8:bf:19::3");
        
	memset(macaddr_remote, 0xFF, 6);
	
	//dhcp4_src_port = 68;

	//Read the arguments and operate
	int index = 2;
	if (argc < 2)
	{
		show_help();
		return 0;
	}
	while (index <= argc)
	{
		if (strcmp(argv[index - 1],"-h") == 0) //help info
      		{
          		show_help();
          		return 0;
      		}
		else if (strcmp(argv[index - 1],"-a") == 0) //set local6addr and remote6addr
      		{
         		if(argc < index + 2)
         		{
           			 printf("[4over6 CRA]: wrong number of arguments.\n");
            			 return 1;
         		}
         		strcpy(local6addr,argv[index]);
			strcpy(remote6addr,argv[index + 1]);
			index += 3;
      		}
		else if (strcmp(argv[index - 1],"-b") == 0) //set physic device name
                {
                        if(argc < index + 1)
                        {
                                 printf("[4over6 CRA]: wrong number of arguments.\n");
                                 return 1;
                        }
                        strcpy(PHYSIC_IFNAME,argv[index]);
			index += 2;
                }
                else if (strcmp(argv[index - 1],"-c") == 0) //set tunnel device name
                {
                        if(argc < index + 1)
                        {
                                 printf("[4over6 CRA]: wrong number of arguments.\n");
                                 return 1;
                        }
                        strcpy(TUNNEL_IFNAME,argv[index]);
                        index += 2;
                }
		else if (strcmp(argv[index - 1],"-d") == 0)//run with default configuration
		{
			if(index == 2)
			{
				printf("\n[4over6 CRA]:Continue with default configuration.\n");
				index = argc + 1;
			}
			index ++;
		}
		else
		{
			show_help();
			return 0;
		}
	}
	//Show current configuration
	printf("[4over6 CRA]:Current configuration:\n");
        printf("local v6 address:%s\nremote v6 address:%s\n",local6addr,remote6addr);
        printf("physic interface:%s\n",PHYSIC_IFNAME);

	inet_pton(AF_INET6,remote6addr,remote6addr_buf);

	//Get MAC address of physics interface
	s_info = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifopt.ifr_name,PHYSIC_IFNAME);
	if (ioctl(s_info, SIOCGIFHWADDR, &ifopt) == -1)
	{
		printf("[4over6 CRA]: Failed to get MAC address of 4over6 interface.\n");
		return 1;
	}
	memcpy(macaddr_phy, ifopt.ifr_hwaddr.sa_data,ETH_ALEN);
	printf("macphy = %s\n",mac_to_str(macaddr_phy));

	//Get MAC address of 4over6 interface
	s_info = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifopt.ifr_name,TUNNEL_IFNAME);
	if (ioctl(s_info, SIOCGIFHWADDR, &ifopt) == -1)
	{
		printf("[4over6 CRA]: Failed to get MAC address of 4over6 interface.\n");
		return 1;
	}
	memcpy(macaddr_4o6, ifopt.ifr_hwaddr.sa_data,ETH_ALEN);
	printf("mac4o6 = %s\n",mac_to_str(macaddr_4o6));
	device.sll_family = AF_PACKET;
        memcpy (device.sll_addr, macaddr_4o6, 6);
        device.sll_halen = htons (6);
	close(s_info);

	//Create the socket that listening to all packets
	s_dhcp = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s_dhcp < 0)
	{
		printf("[4over6 CRA]: Failed to create listening socket.\n");
		return 1;
	}

	//Create the socket that send out DHCPv4-over-v6 or send back DHCPv4 packets
	s_send = socket(PF_PACKET, SOCK_RAW, htons (ETH_P_ALL));
	if (s_send < 0)
	{
		printf("[4over6 CRA]: Failed to create send socket.\n");
		return 1;
	}

	char cmd[256] = {0};
	sprintf(cmd, "ping6 -c 1 %s", remote6addr);
	printf("cmd=%s\n", cmd);
	system(cmd);

	printf("[4over6 CRA]: Listening...\n");
	getFakeReply();
	//CRA auto-machine
	while (1)
	{
            status = getPacket();
	    if (status == 4)
	    {
		//Find out the DHCP packet from 4over6 interface	
	        if (isUDPpacket(iphead,4))
		{
		    if(isDHCPpacket(4, udphead))
		    {
			//Send out DHCPv4-over-v6 packet
//			printf("sending 6!\n");
//			printf("mac addr = %s\n",mac_to_str(ethhead+6));
			if(isTunnelDhcp(udphead))
			{
				uint8_t message_type = *(udphead + 8);
				if (message_type == 1) {
					//dhcp4_src_port = dhcp4_src_port_t;
					sendPacket6(ethhead,udphead,udplen);
				}
			}// else printf("is not TunnelDhcp!!!!!\n");
		    }// else printf("is not DHCP!!!!!\n");
		}
	    }
	    else if (status == 6)
	    {//printf("Here I am! isDHCPpacket = %d\n",isDHCPpacket(udphead,67,68));
		//find out the DHCPv4-over-v6 packet from physic interface
		if (isUDPpacket(iphead,6))	
		{
		    if(isDHCPpacket(6, udphead))
		    {
			//Send back DHCPv4 packet to 4over6 interface.
			if(isTunnelDhcp(udphead))
			{
				uint8_t message_type = *(udphead + 8);
				if (message_type == 2) {

					sendPacket4(ethhead,udphead,udplen);

				}
			}
		    }
		}
	    }
/*
	    //Get a packet from all the interfaces
	    if (status == WAIT_FOR_REQ)
	    	getPacket(4);
	    else if (status == WAIT_FOR_REPLY)
	    {
		getFakeReply();
		getPacket(6);
	    }
	    else
		printf("[4over6 CRA]:Status error!\n");
*/
	}
	 
	close(s_dhcp);
	close(s_send);
	printf("[4over6 CRA]: Closed.\n");
	return 0;
}

unsigned short int
checksum (unsigned short int *addr, int len)
{
  int nleft = len;
  int sum = 0;
  unsigned short int *w = addr;
  unsigned short int answer = 0;

  while (nleft > 1) {
    sum += *w++;
    nleft -= sizeof (unsigned short int);
  }

  if (nleft == 1) {
    *(unsigned char *) (&answer) = *(unsigned char *) w;
    sum += answer;
  }

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  answer = ~sum;
  return (answer);
}

char* mac_to_str(unsigned char *ha)
{
	int i;  
    	static char macstr_buf[18] = {'\0', };    
  	memset(macstr_buf, 0x00, 18);   
    	for ( i = 0 ; i < ETH_ALEN ; i++)  
    	{  
        	hexNumToStr(ha[i],&macstr_buf[i*3]);  
       		if ( i < 5 )  
        	{  
            		macstr_buf[(i+1)*3-1] = ':';  
        	}  
    	}  
    	return macstr_buf; 
}

void hexNumToStr(unsigned int number, char *str)  
{  
	char * AsciiNum={"0123456789ABCDEF"};        
	str[0]=AsciiNum[(number>> 4)&0xf];  
	str[1]=AsciiNum[number&0xf];  
} 
int setDevIndex(char* devname)
{
	// Resolve the interface index.
        if ((device.sll_ifindex = if_nametoindex (devname)) == 0)
        {
                printf("[4over6 CRA]: Failed to resolve the index of %s.\n",devname);
                return 1;
        }
        //printf ("Index for interface %s is %i\n", devname, device.sll_ifindex);
	return 0;
}

int isUDPpacket(char* iphead, int type)
{
	switch (type)
	{
		case 4:
			if (iphead[9] != IPPROTO_UDP)
			{
				//printf("[4over6 CRA]: Got a v4 packet but is not UDPv4.\n");
				return 0;
			}
			break;
		case 6:
			if (iphead[6] != IPPROTO_UDP)
			{
				//printf("[4over6 CRA]: Got a v6 packet but is not UDPv6.\n");
				return 0;
			}
			break;
		default:
			//printf("[4over6 CRA]: Wrong argument of isUDPpacket.\n");
			return 0;
			break;
	}
	return 1;	
}

int isDHCPpacket(int type, char* udphead)
{
	uint16_t src_port = ntohs(*(uint16_t*)(udphead + 0));
	uint16_t dst_port = ntohs(*(uint16_t*)(udphead + 2));
//printf("src_port = %d, dst_port = %d\n",src_port, dst_port);
	if (type == 4) {
		if (dst_port != 67 /*|| src_port != 68*/)
		{
			//printf("[4over6 CRA]: Got a packet but not targeted to port %d.\n",dst);
			return 0;
		}  
		//dhcp4_src_port_t = src_port;
	}
	if (type == 6) {
		if (dst_port != 67 || src_port != 67)
		{
			//printf("[4over6 CRA]: Got a packet but not targeted to port %d.\n",dst);
			return 0;
		}  
	}

	return 1;
}

int isTunnelDhcp(char *udphead)
{
	char *dhcphead = udphead + 8;
	if (memcmp(macaddr_4o6,dhcphead + 28,6) == 0 || memcmp(macaddr_phy,dhcphead + 28,6) == 0)
	{
		printf("[4over6 CRA]:Got the right DHCP packet!\n");
		return 1;
	}
	return 0;
}

int isDHCPACK(char* udphead)
{
	char *dhcphead = udphead + 8;
	if (dhcphead[0]&0XFF == 1)
	{
		printf("[4over6 CRA]:This is not a BOOTREPLY.\n");
		return 0;
	}
	if (dhcphead[240]&0XFF != 53)
	{
		printf("[4over6 CRA]:There is no Option 53.\n");
		return 0;
	}
	if (dhcphead[242]&0XFF == 5)
	{
		printf("[4over6 CRA]:This is DHCPACK.\n");
		return 1;
	}
	else
	{
		printf("[4over6 CRA]:This is not DHCPACK.\n");
		return 0;
	}
}

int getPacket()
{
	memset(buff,0,buffLen);
	//Get a packet from all the interfaces
	//int result = recvfrom(s_dhcp, buff, buffLen, 0, NULL, NULL);
	int result = recv(s_dhcp,buff,buffLen,0);
	//Locate the headers of the packet
    ethhead = buff;
    iphead = ethhead + 14;
    //printf("ethhead=%x\n", ethhead);
    if (ethhead[0] == 0x45 && ethhead[1] == 0x00) {
    	iphead = ethhead;
	//puts("starts from ip header!!!!!\n");
    }
    //printf("iphead=%x\n", iphead);
    int type = 0;
    if (iphead[0] == 0x45)
    	type = 4;
    else if (iphead[0] == 0x60)
    	type = 6;
    	
        if (type == 4)
	{
		udphead = iphead + 20;
        	udplen = ((iphead[2]<<8)&0XFF00 | iphead[3]&0XFF) - 20;
	}
	else if (type == 6)
	{
		udphead = iphead + 40;
		udplen = ((iphead[4]<<8)&0XFF00 | iphead[5]&0XFF) - 40;
		if (memcmp(iphead + 8, remote6addr_buf, 16) == 0) {
			memcpy(macaddr_remote, ethhead + 6, 6);
		}
	}
	
	return type;
}

int sendPacket6(char* ethhead, char* udphead, int udplen)
{printf("sendPacket6!!!\n");
	char *frame = NULL;
	int frame_len = 14 + 40 + udplen;
	frame = malloc (sizeof(char) * frame_len);

	*(uint16_t*)udphead = htons(67);

	//Add ethernet header
	memcpy(frame, ethhead, 14);
//printf("dest mac=%02x:%02x:%02x:%02x:%02x:%02x\n", frame[0], frame[1], frame[2], frame[3], frame[4], frame[5]);
//printf("ethhead=%x iphead=%x\n", ethhead, iphead);
//int i;for (i = 0; i < 30; ++i) printf("%02x ", (uint8_t)ethhead[i]);printf("\n");
	//memset(frame, 0xFF, 6);
	memcpy(frame, macaddr_remote, 6);
	memcpy(frame + 6, macaddr_phy, 6);
	frame[12] = ETH_P_IPV6 / 256;
	frame[13] = ETH_P_IPV6 % 256;
	//Add ipv6 header
	send_ip6hdr.ip6_flow = htonl((6 << 28) | (0 << 20) | 0);		
	send_ip6hdr.ip6_plen = htons(udplen);
	send_ip6hdr.ip6_nxt = IPPROTO_UDP;
	send_ip6hdr.ip6_hops = 128;
	inet_pton(AF_INET6,local6addr,&send_ip6hdr.ip6_src);
	inet_pton(AF_INET6,remote6addr,&send_ip6hdr.ip6_dst);	
	memcpy(frame + 14,(char *)&send_ip6hdr, 40);

	memcpy(udphead + 36, macaddr_phy, 6);
	
	//Re-caculate the udp checksum
	uint16_t newchecksum = udpchecksum((char*)&send_ip6hdr, udphead, udplen,6);
	udphead[6] = (newchecksum >> 8) & 0xFF;
	udphead[7] = newchecksum & 0xFF;
	//Add udp header + dhcp data
	memcpy(frame + 14 + 40, udphead, udplen);
	//Send out packet
	if(setDevIndex(PHYSIC_IFNAME))
	{
		return 1;
	}
	if(sendto(s_send, frame, frame_len, 0, (struct sockaddr *)&device, sizeof(device)) <= 0)
	{
		printf("[4over6 CRA]: Failed to send out dhcpv4-over-v6 packet.\n");
		return 1;
	}
	free(frame);
	return 0;
}

int sendPacket4(char *ethhead, char *udphead, int udplen)
{
	//*(uint16_t*)(udphead + 2) = htons(dhcp4_src_port);
	*(uint16_t*)(udphead + 2) = htons(68);
	char *frame = NULL;
	udplen += 40;
        int frame_len = 14 + 20 + udplen;
        frame = malloc (sizeof(char) * frame_len);
        //Add ethernet header
        memcpy(frame, ethhead, 14);
        memcpy(frame, macaddr_4o6, 6);
	frame[12] = ETH_P_IP / 256;
        frame[13] = ETH_P_IP % 256;
        //Add ipv4 header
        send_ip4hdr.ip_hl = 5;
        send_ip4hdr.ip_v = 4;
	send_ip4hdr.ip_tos = 0;
	send_ip4hdr.ip_len = htons(20 + udplen);
        send_ip4hdr.ip_id = htons(0);
        send_ip4hdr.ip_off = htons(0);
	send_ip4hdr.ip_ttl = 255;
	send_ip4hdr.ip_p = IPPROTO_UDP;
	//ciaddr is from yiaddr field
	memcpy(ciaddr,udphead + 24,4);
	//siaddr is fixed to a custom value, and this may not matter
//	memcpy(siaddr,udphead + 20,4);
//	inet_pton(AF_INET,"58.205.200.1",&siaddr);
	inet_pton(AF_INET,"0.0.0.0",&siaddr);
//        inet_pton(AF_INET,ciaddr,&send_ip4hdr.ip_src);
//        inet_pton(AF_INET,siaddr,&send_ip4hdr.ip_dst);
	memcpy((char*)&send_ip4hdr.ip_dst,ciaddr,4);
	memcpy((char*)&send_ip4hdr.ip_src,siaddr,4);
	send_ip4hdr.ip_sum = 0;
	send_ip4hdr.ip_sum = checksum((unsigned short int*)&send_ip4hdr,20);
        memcpy(frame + 14,(char *)&send_ip4hdr, 20);

	memcpy(udphead + 36, macaddr_4o6, 6);

	//Re-caculate the udp checksum
        uint16_t newchecksum = udpchecksum((char*)&send_ip4hdr, udphead, udplen,4);

	udphead[6] = (newchecksum >> 8) & 0xFF;
        udphead[7] = newchecksum & 0xFF;
        //Add udp header + dhcp data
        memcpy(frame + 14 + 20, udphead, udplen);
        //Send out packet
	//printf("Going to setDevIndex in sendPacket4.\n");
        if(setDevIndex("lo"))
        {
                return 1;
        }
	//printf("After setDevIndex in sendPacket4~!\n");
        int count = 0;
	//if(sendto(s_send, frame, frame_len, 0, (struct sockaddr *)&device, sizeof(device)) <= 0)
	if((count = sendto(s_send, frame, frame_len, 0, (struct sockaddr *)&device, sizeof(device))) <= 0)
        {
                printf("[4over6 CRA]: Failed to send back dhcpv4 packet.\n");
                return 1;
        }
	//printf("count %d,udplen %d\n", count, udplen);
/*
	//用普通udp socket来发dhcpv4报文给虚接口
	struct sockaddr_in psedoaddr, localaddr;
        int s_sendv4 = socket(AF_INET, SOCK_DGRAM, 0);
	psedoaddr.sin_family = AF_INET;
	psedoaddr.sin_port = htons(68);
	inet_pton(AF_INET, "127.0.0.1", &psedoaddr.sin_addr);
	
	localaddr.sin_family = AF_INET;
	localaddr.sin_port = htons(67);
	inet_pton(AF_INET, "58.205.200.1", &localaddr.sin_addr);
	if (bind(s_sendv4, (struct sockaddr*)&localaddr, sizeof(struct sockaddr_in)) == -1)
	{
		printf("[4over6 CRA]: Failed to bind s_sendv4 to local port 67.\n");
//		return 1;
	}

	if(sendto(s_sendv4, udphead + 8, udplen - 8 + 40, 0, (struct sockaddr *)&psedoaddr, sizeof(struct sockaddr_in)) <= 0)
	{
		printf("[4over6 CRA]: Failed to send back dhcpv4 packet.\n");
		return 1;
	}
*/
	free(frame);
        return 0;	
}


uint16_t udpchecksum(char *iphead, char *udphead, int udplen, int type)
{
	udphead[6] = udphead[7] = 0;
        uint32_t checksum = 0;
	//printf("udp checksum is 0x%02x%02x\n", (uint8_t)udphead[6], (uint8_t)udphead[7]);
	if (type == 6)
	{
		struct udp6_psedoheader header;
		memcpy(header.srcaddr, iphead + 24, 16);
		memcpy(header.dstaddr, iphead + 8, 16);
		header.length = ntohs(udplen);
		header.zero1 = header.zero2 = 0;
		header.next_header = 0x11;
		uint16_t *hptr = (uint16_t*)&header;
		int hlen = sizeof(header);
		while (hlen > 0) {
			checksum += *(hptr++);
			hlen -= 2;
		}
	}
	else if (type == 4)
	{
		struct udp4_psedoheader header;
                memcpy((char*)&header.srcaddr, iphead + 12, 4);
                memcpy((char*)&header.dstaddr, iphead + 16, 4);
                header.zero = 0;
                header.protocol = 0x11;
                header.length = ntohs(udplen);
                uint16_t *hptr = (uint16_t*)&header;
                int hlen = sizeof(header);
                while (hlen > 0) {
                        checksum += *(hptr++);
                        hlen -= 2;
                }
	}	
	uint16_t *uptr = (uint16_t*)udphead;
	while (udplen > 1) {	
		checksum += *(uptr++);
		udplen -= 2;
	}
	if (udplen) {
//		checksum += (*((uint8_t*)uptr)) << 8;
		checksum += (*((uint8_t*)uptr)) ;
	}
	do {
		checksum = (checksum >> 16) + (checksum & 0xFFFF);
	} while (checksum != (checksum & 0xFFFF));
	uint16_t ans = checksum;
	//printf("ans: 0x%04x\n", (ans == 0xFF)? 0xFF :ntohs(~ans));
	return (ans == 0xFF)? 0xFF :ntohs(~ans);
}




//UI Function    ===========================
void show_help(void)
{
   printf("./cra [-h] | [-d]\n      [-a local_v6_addr remote_v6_addr] | [-b interface_name] | [-c interface_name]\n");
   printf("-h : display this help information.\n");
   printf("-d : run program with default settings.\n");
   printf("-a : set local and remote ipv6 address.\n");
   printf("-b : set physic device (interface) name.\n");
   printf("-c : set the name of the interface on which runs the DHCP client.\n");
   return ;
}

//Debuging function  =========================
int getFakeReply(void) // This function is to listen on IPv6 port 67, and then vanish the ICMPv6 Port-Unreachable message
{
	if (fork() == 0) {
		struct sockaddr_in6 sin6addr;
		int addr_len = sizeof (sin6addr);
		char buff[1024];
		sin6addr.sin6_family = AF_INET6;
		sin6addr.sin6_flowinfo = 0;
		sin6addr.sin6_port = htons(67);
		sin6addr.sin6_addr = in6addr_any;
		int s = socket(AF_INET6, SOCK_DGRAM, 0);
		if (bind(s, (struct sockaddr*)&sin6addr, sizeof(sin6addr)) == -1)
		{
			//printf("[4over6 CRA]: Failed to bind fake v6listener to port 68.\n");
			return 1;
		}
		//printf("Ready to get a Fake v6\n");	
		while (1) {
			int result = recvfrom(s, buff, 1024, 0, (struct sockaddr*)&sin6addr, &addr_len);
			//usleep(1000);
		}
	}
	return 0;
}
