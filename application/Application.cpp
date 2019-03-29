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

#define DEFAULT_TIME 15
#define BT_ADDRESS_STRING_SIZE 18

GDBusConnection *con;
gboolean is_scanning;
std::map<std::string, int> device_status;
wolkabout::DeviceConfiguration appConfiguration;

void free_properties(GVariantIter* properties, GVariant* value)
{
    if(properties != NULL)
        g_variant_iter_free(properties);
    if(value != NULL)
        g_variant_unref(value);

    return;
}

static int adapter_call_method(const char *method, GVariant *param)
{
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(con,
                         "org.bluez",
                         "/org/bluez/hci0",
                         "org.bluez.Adapter1",
                         method,
                         param,
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         &error);
    if(error != NULL)
        return 1;

    g_variant_unref(result);
    return 0;
}

static int adapter_set_property(const char *prop, GVariant *value)
{
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(con,
                         "org.bluez",
                         "/org/bluez/hci0",
                         "org.freedesktop.DBus.Properties",
                         "Set",
                         g_variant_new("(ssv)", "org.bluez.Adapter1", prop, value),
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         &error);
    if(error != NULL)
        return 1;

    g_variant_unref(result);
    return 0;
}

static void signal_adapter_changed(GDBusConnection *conn,
                    const gchar *sender,
                    const gchar *path,
                    const gchar *interface,
                    const gchar *signal,
                    GVariant *params,
                    void *userdata)
{
    (void)conn;
    (void)sender;
    (void)path;
    (void)interface;
    (void)userdata;

    GVariantIter *properties = NULL;
    GVariantIter *unknown = NULL;
    const char *iface;
    const char *key;
    GVariant *value = NULL;
    const gchar *signature = g_variant_get_type_string(params);

    if(g_strcmp0(signature, "(sa{sv}as)") != 0) {
        std::cout<<"Invalid signature for "<<signal<<":"<<signature<<"!= (sa{sv}as)\n";
        free_properties(properties, value);
        return;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while(g_variant_iter_next(properties, "{&sv}", &key, &value)) {
        if(!g_strcmp0(key, "Powered")) {
            if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                std::cout<<"Invalid argument type for "<<key<<":"<<g_variant_get_type_string(value)<< "!= b\n";
                free_properties(properties, value);
                return;
            }
            std::cout<<"Adapter is Powered "<<(g_variant_get_boolean(value) ? "on" : "off")<<"\n";
        }
        if(!g_strcmp0(key, "Discovering")) {
            if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                std::cout<<"Invalid argument type for "<<key<<":"<<g_variant_get_type_string(value)<< "!= b\n";
                free_properties(properties, value);
                return;
            }
            std::cout<<"Adapter scan "<< (g_variant_get_boolean(value) ? "on" : "off")<<"\n";
        }
    }

}

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
                         rc = adapter_call_method("RemoveDevice", g_variant_new("(o)", object));
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
        int rc = adapter_call_method("StopDiscovery", NULL);
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
        int rc = adapter_call_method("StartDiscovery", NULL);
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

    GMainLoop *loop;
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

    std::random_device rd;
    std::mt19937 mt(rd());

    unsigned interval = appConfiguration.getInterval();

    con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if(con == NULL) {
        std::cout<<"Unable to get connection to system bus\n";
        return 1;
    }

    loop = g_main_loop_new(NULL, FALSE);
    if(interval <= 0)
        interval = DEFAULT_TIME;

    guint timeout_id = g_timeout_add_seconds(interval, 
                                            timer_scan_publish,
                                            (void*)wolk.get());

    prop_changed = g_dbus_connection_signal_subscribe(con,
                        "org.bluez",
                        "org.freedesktop.DBus.Properties",
                        "PropertiesChanged",
                        NULL,
                        "org.bluez.Adapter1",
                        G_DBUS_SIGNAL_FLAGS_NONE,
                        signal_adapter_changed,
                        NULL,
                        NULL);

    iface_added = g_dbus_connection_signal_subscribe(con,
                            "org.bluez",
                            "org.freedesktop.DBus.ObjectManager",
                            "InterfacesAdded",
                            NULL,
                            NULL,
                            G_DBUS_SIGNAL_FLAGS_NONE,
                            device_appeared,
                            loop,
                            NULL);

    iface_removed = g_dbus_connection_signal_subscribe(con,
                            "org.bluez",
                            "org.freedesktop.DBus.ObjectManager",
                            "InterfacesRemoved",
                            NULL,
                            NULL,
                            G_DBUS_SIGNAL_FLAGS_NONE,
                            device_disappeared,
                            loop,
                            NULL);

    rc = adapter_set_property("Powered", g_variant_new("b", TRUE));
    if(rc) {
        std::cout<<"Unable to enable the adapter\n";
    }

    rc = adapter_call_method("StartDiscovery", NULL);
    if(rc) {
        std::cout<<"Unable to scan for new devices\n";
    }
    is_scanning = TRUE;

    g_main_loop_run(loop);

    return 0;
}
