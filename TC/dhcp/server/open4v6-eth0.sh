#!/bin/bash
rm -f dhcpd.leases
touch dhcpd.leases
./dhcpd -4v6 -4v6interface eth0 eth1 -p 67 -cf dhcpd.conf -lf dhcpd.leases -f
