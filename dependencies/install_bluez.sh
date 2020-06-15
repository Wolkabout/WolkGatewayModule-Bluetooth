#!/bin/sh

sudo apt update
sudo apt install libdbus-1-dev libglib2.0-dev libudev-dev libical-dev libreadline-dev -y
wget www.kernel.org/pub/linux/bluetooth/bluez-5.54.tar.xz
tar xvf bluez-5.54.tar.xz && cd bluez-5.54 || exit
./configure --prefix=/usr --mandir=/usr/share/man --sysconfdir=/etc --localstatedir=/var --enable-experimental
make -j$(nproc)
sudo make install
