#!/bin/bash
#wlan0在此处为可以上网的网卡，如果eht0可以上网，则共享eth0，-j MASQUERADE的意思是能上网的网卡IP地址为DHCP分配
iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE
