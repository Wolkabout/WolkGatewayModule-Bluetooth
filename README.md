# WolkGatewayModule-Bluetooth
WolkAbout Gateway module for connecting bluetooth devices to WolkAbout Gateway.
This module was tested on Raspberry Pi 3 Model B+ running Raspbian.
It should work on any bluetooth enabled Linux device as it uses Bluez - the default Linux bluetooth stack. It uses the hci0 bluetooth adapter.

Supported protocol(s):
* JSON_PROTOCOL

Installing from source
----------------------

This repository must be cloned from the command line using:
```sh
git clone --recurse-submodules https://github.com/Wolkabout/WolkGatewayModule-Bluetooth.git
```

Prerequisite
------
Following tools/libraries are required in order to build this project:

* cmake - version 3.5 or later
* autotools
* autoconf
* libtool
* m4
* zlib1g-dev
* python
* python pip
* conan
* bluez - version 5.5

With the exception of bluez the former can be installed on Debian based system from terminal by invoking

```sh
sudo apt-get install autotools-dev autoconf libtool m4 zlib1g-dev cmake python python-pip && python -m pip install conan
```

To install bluez 5.5 with all it's dependencies you can run the 
```sh
install_bluez.sh
```  
located in the `dependencies` folder of this project.
After installing bluez it's recommended you reboot your system and check whether it was updated by invoking
```sh
bluetoothctl -v
```
It should display **5.50**.

After the dependencies have been built, the Makefile build system is generated by invoking
`./configure`

Generated build system is located inside `out` directory


The module and the example are built from `out` directory by invoking
`make` in terminal

Before running the module, you should check whether your bluetooth daemon is running. You can do so by invoking
```sh
systemctl status bluetooth
```
If it's not running you can start it by invoking
```sh
sudo systemctl start bluetooth
```
The example should be run with two arguments - first one being the `deviceConfiguration.json` file and the second one
being the interval of the bluetooth scan in seconds(scan goes on/off alternately on the specified timeframe).
eg. 
```sh
sudo ./example deviceConfiguration.json 15
```
for a scan interval of 15 seconds. Sudo is necessary in order to access the adapter. 

The example will then scan for nearby bluetooth devices, detect if one of the devices that were specified in the .json file
by their key is present and if it is it will publish a `1` to the Wolkabout platform. If the device wasn't spotted in the timeframe
it will send a `0`. 

Example Usage
-------------

**Setting up the Gateway**
First we need to set up the Gateway our module will connect to. In order to do so visit [this repo](https://github.com/Wolkabout/WolkGateway) and follow the instructions.

**Setting up our module**
In the `out` folder locate the `deviceConfiguration.json` file. 
set up `host` to target the Gateway you set up. eg.
```json
"host":"tcp://localhost:1883"
```

**Creating devices:**
Devices are created from the `deviceConfiguration.json` file. In order to create a new device you should define it in the device section of this file. A new device should contain two attributes - name and key. Name is the name of the device to be displayed on the platform and the key is the bluetooth address of the device.
```cpp
"devices":[
{"name":"device_name1",     //the name under which the device will be displayed on the platform
"key":"xx:xx:xx:xx:xx:xx"   //bluetooth address in xx:xx:xx:xx:xx:xx format. 
},
{"name":"device_name2",
"key":"xx:xx:xx:xx:xx:xx"
}
]
```
All devices share a common template of one numeric sensor defined in the `Configuration.cpp` file.