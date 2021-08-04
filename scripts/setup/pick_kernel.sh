#!/bin/bash
kernel=$1
kernlist="$(grep -i "menuentry '" /boot/grub/grub.cfg|sed -r "s|--class .*$||g")"
printf "%s$kernlist"
menuline="$(printf "%s$kernlist\n"|grep -ne $kernel | grep -v recovery | cut -f1 -d":")"
menunum="$(($menuline-2))"
grub-reboot "1>$menunum"
echo "The next grub's menu entry will be choosen after the reboot: 1>$menunum"
