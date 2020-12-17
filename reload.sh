#!/bin/sh

make
sudo rmmod keyboard_tablet wacom hid_generic usbhid hid
sudo insmod ./keyboard_tablet.ko
