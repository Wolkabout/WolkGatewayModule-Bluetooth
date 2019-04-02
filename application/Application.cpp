/*
 * Copyright 2018 WolkAbout Technology s.r.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <fstream>
#include <iterator>

#include <glib.h>
#include <gio/gio.h>
#include <sys/time.h>

#include "Configuration.h"
#include "Wolk.h"
#include "model/DeviceTemplate.h"
#include "utilities/ConsoleLogger.h"
#include "model/SensorTemplate.h"
#include "Adapter.h"
#include "utils.h"

#define BT_ADDRESS_STRING_SIZE 18

wolkabout::Adapter adapter; 

gboolean is_scanning;
std::map<std::string, int> device_status;
wolkabout::DeviceConfiguration appConfiguration;

static void device_appeared(GDBusConnection *sig,
                const gchar *sender_name,
                const gchar *object_path,
                const gchar *interface,
                const gchar *signal_name,
                GVariant *parameters,
                gpointer user_data)
{
    (void)sig;
    (void)sender_name;
    (void)object_path;
    (void)interface;
    (void)signal_name;
    (void)user_data;

    GVariantIter *interfaces;
    const char *object;
    const gchar *interface_name;
    GVariant *properties;
    int rc;

    g_variant_get(parameters, "(&oa{sa{sv}})", &object, &interfaces);
    while(g_variant_iter_next(interfaces, "{&s@a{sv}}", &interface_name, &properties)) {
        if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
            const gchar *property_name;
            GVariantIter i;
            GVariant *prop_val;
            const gchar* value;
            g_variant_iter_init(&i, properties);
            while(g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val))
                if(!(g_strcmp0(property_name, "Address"))){
                        /*find the device*/
                        value = g_variant_get_string(prop_val, NULL);
                        std::cout<<"Found device with adress:"<<value<<"\n";
                        if( !(device_status.find(value) == device_status.end()) ){
                            std::cout<<"Found the listed device adress:"<<value<<"\n";
                        /*do whatever*/
                            device_status[value] = 1;
                        /*remove device*/
                         rc = adapter.call_method("RemoveDevice", g_variant_new("(o)", object));
                         if(rc)
                            std::cout<<"Unable to remove "<<object<<"\n";
                    }
                }
            g_variant_unref(prop_val);
        }
        g_variant_unref(properties);
    }
    return;
}

static void device_disappeared(GDBusConnection *sig,
                const gchar *sender_name,
                const gchar *object_path,
                const gchar *interface,
                const gchar *signal_name,
                GVariant *parameters,
                gpointer user_data)
{
    (void)sig;
    (void)sender_name;
    (void)object_path;
    (void)interface;
    (void)signal_name;

    GVariantIter *interfaces;
    const char *object;
    const gchar *interface_name;
    char address[BT_ADDRESS_STRING_SIZE] = {'\0'};

    g_variant_get(parameters, "(&oas)", &object, &interfaces);
    while(g_variant_iter_next(interfaces, "s", &interface_name)) {
        if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
            int i;
            char *tmp = g_strstr_len(object, -1, "dev_") + 4;

            for(i = 0; *tmp != '\0'; i++, tmp++) {
                if(*tmp == '_') {
                    address[i] = ':';
                    continue;
                }
                address[i] = *tmp;
            }
        }
    }
    return;
}

gboolean timer_scan_publish(gpointer user_data)
{

    wolkabout::Wolk* wolk = (wolkabout::Wolk*)user_data;

    std::cout<<"Time is up!\n";

    if(is_scanning){
        int rc = adapter.call_method("StopDiscovery", NULL);
        if(rc){
            std::cout<<"Unable to stop scanning\n";
            return FALSE;
        }
        g_usleep(100);
        is_scanning = FALSE;

        for(const auto& device : appConfiguration.getDevices()){
            for (const auto& sensor : device.getTemplate().getSensors()){
                std::string key = device.getKey();
                wolk->addSensorReading(key, sensor.getReference(), device_status[key]);
            }
        }

        wolk->publish();

        /*set all to false*/
        for (auto it = device_status.begin(); it != device_status.end(); it++) {
            device_status[it->first] = 0;
        }
    }
    else{
        int rc = adapter.call_method("StartDiscovery", NULL);
        if(rc){
            std::cout<<"Unable to scan for new devices\n";
            return FALSE;
        }
        is_scanning = TRUE;
    }

    return TRUE;
}

int main(int argc, char** argv)
{
    int rc;
    guint prop_changed;
    guint iface_added;
    guint iface_removed;

    auto logger = std::unique_ptr<wolkabout::ConsoleLogger>(new wolkabout::ConsoleLogger());
    logger->setLogLevel(wolkabout::LogLevel::DEBUG);
    wolkabout::Logger::setInstance(std::move(logger));

    if (argc < 2)
    {
        LOG(ERROR) << "WolkGatewayModule Application: Usage -  " << argv[0] << " [configurationFilePath] [scanInterval]";
        return -1;
    }

    try
    {
        appConfiguration = wolkabout::DeviceConfiguration::fromJson(argv[1]);
    }
    catch (std::logic_error& e)
    {
        LOG(ERROR) << "WolkGatewayModule Application: Unable to parse configuration file. Reason: " << e.what();
        return -1;
    }

    for (const auto& device : appConfiguration.getDevices())
    {
            device_status.insert(std::pair<std::string, int>(device.getKey(), 0));
    }

    std::unique_ptr<wolkabout::Wolk> wolk =
      wolkabout::Wolk::newBuilder()
        .actuationHandler([&](const std::string& key, const std::string& reference, const std::string& value) -> void {
        })
        .actuatorStatusProvider([&](const std::string& key, const std::string& reference) -> wolkabout::ActuatorStatus {
        })
        .deviceStatusProvider([&](const std::string& deviceKey) -> wolkabout::DeviceStatus {
            auto it =
              std::find_if(appConfiguration.getDevices().begin(), appConfiguration.getDevices().end(),
                           [&](const wolkabout::Device& device) -> bool { return (device.getKey() == deviceKey); });

            if (it != appConfiguration.getDevices().end())
            {
                return wolkabout::DeviceStatus::CONNECTED;
            }

            return wolkabout::DeviceStatus::OFFLINE;
        })
        .configurationHandler(
          [&](const std::string& deviceKey, const std::vector<wolkabout::ConfigurationItem>& configuration) {
          })
        .configurationProvider([&](const std::string& deviceKey) -> std::vector<wolkabout::ConfigurationItem> {
            return {};
        })
        .host(appConfiguration.getLocalMqttUri())
        .build();

    for (const auto& device : appConfiguration.getDevices())
    {
        wolk->addDevice(device);
    }

    wolk->connect();

    unsigned interval = appConfiguration.getInterval();

    guint timeout_id = g_timeout_add_seconds(interval, 
                                            timer_scan_publish,
                                            (void*)wolk.get());

    prop_changed = adapter.subscribe_adapter_changed();
    iface_added = adapter.subscribe_device_added(device_appeared);
    iface_removed = adapter.subscribe_device_removed(device_disappeared);

    rc = adapter.set_property("Powered", g_variant_new("b", TRUE));
    if(rc) {
        std::cout<<"Unable to enable the adapter\n";
    }

    rc = adapter.call_method("StartDiscovery", NULL);
    if(rc) {
        std::cout<<"Unable to scan for new devices\n";
    }
    is_scanning = TRUE;

    adapter.run_loop();

    return 0;
}
