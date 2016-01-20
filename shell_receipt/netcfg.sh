#!/bin/bash

if [ $# != 3 ]
then
    echo "Usage: $0 <IP> <network> <gateway>"
fi

iface=eth0
how=static
cfgfile=/etc/network/interfaces
dnsfile=/etc/resolv.conf
address=$1
netmask=$2
gateway=$3


#configure static ip address
if [ -f $cfgfile ] && [ -w $cfgfile ]
then
    echo "iface $iface inet $how" >> $cfgfile
    echo "address $address" >> $cfgfile
    echo "netmask $network" >> $cfgfile
    echo "gateway $gateway" >> $cfgfile
    echo "auto $iface" >> $cfgfile
fi

#configure DNS
if [ -f $dnsfile ] && [ -w  $dnsfile ]
then
    echo "nameserver $gateway" >> $dnsfile
fi

#add default gateway
route add default gw $gateway

#restart network service
/etc/init.d/networking restart



