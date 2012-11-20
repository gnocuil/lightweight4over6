#!/bin/bash
rm -f dhcpd.leases
touch dhcpd.leases
./dhcpd -4v6 -4v6interface public4over6 eth1 -p 67 -cf dhcpd.conf -lf dhcpd.leases -f
