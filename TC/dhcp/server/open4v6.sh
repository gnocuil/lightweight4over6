#!/bin/bash
./dhcpd -4v6 -4v6interface 4over6 -p 67 -cf dhcpd.conf -lf dhcpd.leases eth1
