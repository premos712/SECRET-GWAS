#!/bin/bash

curl https://ipv4.icanhazip.com > ip.txt
cp ip.txt compute_server/host/ip.txt
cp ip.txt client/ip.txt
rm ip.txt