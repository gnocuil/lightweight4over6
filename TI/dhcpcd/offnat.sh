#!/bin/sh

iptables -t nat -F
iptables --flush
