#!/bin/sh

if [ $# -ne 3 ] ; then
echo "usage: `basename $0` [port] [port] [network]"
exit 1
fi

if [ $1 -lt 0 ] || [ $1 -gt $2 ] || [ $2 -gt 65535 ] ; then
echo -e "\nInput error!\n"
echo -e "rule: 0<start<over<65535!\n"
exit 1
fi
#start port
START=$1

#end port
OVER=$2

#Change this when needed
#INNERIP=58.205.200.0/24
#INNERIP=10.0.2.0/24
#INNERIP=219.243.208.16/28
PUBLICIP=$3

PRIVATEIP=192.168.2.0/24
#/etc/rc.d/iptables start
#we shoule open iptables service first

echo "1" > /proc/sys/net/ipv4/ip_forward 

iptables -t nat -F
iptables --flush

#Disable default FORWARD rules and just allow INNERIP packets to go through
iptables -P FORWARD DROP
iptables -A FORWARD -s $PUBLICIP -j ACCEPT
iptables -A FORWARD -s $PRIVATEIP -j ACCEPT  

#Configure the NAPT rules
iptables -A FORWARD -m state --state ESTABLISHED,RELATED -j ACCEPT

iptables -t nat -A POSTROUTING -s $PUBLICIP -p TCP -j MASQUERADE --to-ports $START-$OVER
iptables -t nat -A POSTROUTING -s $PUBLICIP -p UDP -j MASQUERADE --to-ports $START-$OVER
iptables -t nat -A POSTROUTING -s $PRIVATEIP -p TCP -j MASQUERADE --to-ports $START-$OVER
iptables -t nat -A POSTROUTING -s $PRIVATEIP -p UDP -j MASQUERADE --to-ports $START-$OVER

iptables -t mangle -A FORWARD -p tcp --tcp-flags SYN,RST SYN -j TCPMSS --set-mss 1420

iptables -t nat -A POSTROUTING -s $PRIVATEIP -j MASQUERADE
