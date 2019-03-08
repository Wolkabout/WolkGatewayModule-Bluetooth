#!/bin/sh

sudo apt-get update

sudo apt-get install libdbus-1-dev libglib2.0-dev libudev-dev libical-dev libreadline-dev -y

wget www.kernel.org/pub/linux/bluetooth/bluez-5.50.tar.xz

tar xvf bluez-5.50.tar.xz && cd bluez-5.50

./configure --prefix=/usr --mandir=/usr/share/man --sysconfdir=/etc --localstatedir=/var --enable-experimental 

make -j4

sudo make install