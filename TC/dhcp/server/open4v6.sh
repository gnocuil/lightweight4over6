#!/bin/bash
./dhcpd -4v6 -4v6interface 4over6 eth1 -p 67 -cf dhcpd.conf -lf dhcpd.leases -f
