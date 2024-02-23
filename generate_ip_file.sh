#!/bin/bash

curl https://ipv4.icanhazip.com > ip.txt
cp ip.txt enclave_node/host/ip.txt
cp ip.txt dpi/ip.txt
rm ip.txt