#!/bin/bash
rm -f dhcpd_ipv4.leases
touch dhcpd_ipv4.leases
./dhcpd -4 eth0 -p 67 -cf dhcpd.conf -lf dhcpd.leases -f
