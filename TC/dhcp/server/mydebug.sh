#!/bin/bash
gdb dhcpd
#set args -4 -p 67 -cf ./dhcpd.conf -lf ./dhcpd.leases -f

set args -4 -p 67 -f
