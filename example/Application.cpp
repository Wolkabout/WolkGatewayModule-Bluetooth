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

#include "Configuration.h"
#include "Wolk.h"
#include "model/DeviceManifest.h"
#include "utilities/ConsoleLogger.h"

#include "model/SensorManifest.h"

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

#define DEFAULT_TIME 15
#define BT_ADDRESS_STRING_SIZE 18


//using namespace std;


GDBusConnection *con;
gboolean is_scanning;
std::map<std::string, int> devices;
wolkabout::DeviceConfiguration appConfiguration;
std::unique_ptr<wolkabout::Wolk> wolk;
/*
bluez functions
*/
static int bluez_adapter_call_method(const char *method, GVariant *param)
{
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(con,
                         "org.bluez",
                    /* TODO Find the adapter path runtime */
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

static void bluez_device_appeared(GDBusConnection *sig,
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
                        g_print("Adress: %s\n", value);
                        if( !(devices.find(value) == devices.end()) ){
                            std::cout<<"Found the device "<<value<<"\n";
                        /*do whatever*/
                            devices[value] = 1;
                        /*remove device*/
                         rc = bluez_adapter_call_method("RemoveDevice", g_variant_new("(o)", object));
                         if(rc)
                            g_print("Not able to remove %s\n", object);
                    }
                }
            g_variant_unref(prop_val);
        }
        g_variant_unref(properties);
    }
    return;
}

static void bluez_device_disappeared(GDBusConnection *sig,
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

static void bluez_signal_adapter_changed(GDBusConnection *conn,
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
        g_print("Invalid signature for %s: %s != %s", signal, signature, "(sa{sv}as)");
        goto done;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while(g_variant_iter_next(properties, "{&sv}", &key, &value)) {
        if(!g_strcmp0(key, "Powered")) {
            if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                g_print("Invalid argument type for %s: %s != %s", key,
                        g_variant_get_type_string(value), "b");
                goto done;
            }
            g_print("Adapter is Powered \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
        }
        if(!g_strcmp0(key, "Discovering")) {
            if(!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN)) {
                g_print("Invalid argument type for %s: %s != %s", key,
                        g_variant_get_type_string(value), "b");
                goto done;
            }
            g_print("Adapter scan \"%s\"\n", g_variant_get_boolean(value) ? "on" : "off");
        }
    }
done:
    if(properties != NULL)
        g_variant_iter_free(properties);
    if(value != NULL)
        g_variant_unref(value);
}

static int bluez_adapter_set_property(const char *prop, GVariant *value)
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

gboolean notify(gpointer user_data)
{
    g_print("Time is up!\n");

    if(is_scanning){
        int rc = bluez_adapter_call_method("StopDiscovery", NULL);
        if(rc){
            g_print("Not able to stop scanning\n");
            return FALSE;
        }
        g_usleep(100);
        is_scanning = FALSE;

        /*publish*/
        /*for (auto it = devices.begin(); it != devices.end(); it++) {
            std::cout<<"device: "<<it->first<<" value: "<<it->second<<"\n";
        }*/

        for(const auto& device : appConfiguration.getDevices()){
            for (const auto& sensor : device.getManifest().getSensors()){
                std::string key = device.getKey();
                wolk->addSensorReading(key, sensor.getReference(), devices[key]);
            }
        }

        wolk->publish();

        /*set all to false*/
        for (auto it = devices.begin(); it != devices.end(); it++) {
            devices[it->first] = 0;
        }
    }
    else{
        int rc = bluez_adapter_call_method("StartDiscovery", NULL);
        if(rc){
            g_print("Not able to scan for new devices\n");
            return FALSE;
        }
        is_scanning = TRUE;
    }

    return TRUE;
}

/**/
class ActuatorHandler
{
public:
    virtual ~ActuatorHandler() = default;
    virtual std::string getValue() = 0;
    virtual void setValue(std::string value) = 0;
};

template <class T> class ActuatorTemplateHandler : public ActuatorHandler
{
public:
    void setValue(std::string value) override
    {
        try
        {
            m_value = std::stod(value);
        }
        catch (...)
        {
        }
    }

    std::string getValue() override { return std::to_string(m_value); }

private:
    T m_value;
};

template <> class ActuatorTemplateHandler<bool> : public ActuatorHandler
{
public:
    void setValue(std::string value) override { m_value = value == "true"; }

    std::string getValue() override { return m_value ? "true" : "false"; }

private:
    bool m_value;
};

template <> class ActuatorTemplateHandler<std::string> : public ActuatorHandler
{
public:
    void setValue(std::string value) override { m_value = value; }

    std::string getValue() override { return m_value; }

private:
    std::string m_value;
};

int main(int argc, char** argv)
{

    GMainLoop *loop;
    int rc;
    guint prop_changed;
    guint iface_added;
    guint iface_removed;
    guint time_in_seconds = 0; 

    auto logger = std::unique_ptr<wolkabout::ConsoleLogger>(new wolkabout::ConsoleLogger());
    logger->setLogLevel(wolkabout::LogLevel::DEBUG);
    wolkabout::Logger::setInstance(std::move(logger));

    if (argc < 2)
    {
        LOG(ERROR) << "WolkGatewayModule Application: Usage -  " << argv[0] << " [configurationFilePath]";
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

    time_in_seconds = std::stoi(argv[2]);

    for (const auto& device : appConfiguration.getDevices())
    {
            devices.insert(std::pair<std::string, int>(device.getKey(), 0));
    }

    std::map<std::string, std::shared_ptr<ActuatorHandler>> handlers;

    for (const auto& device : appConfiguration.getDevices())
    {
        for (const auto& actuator : device.getManifest().getActuators())
        {
            std::shared_ptr<ActuatorHandler> handler;
            switch (actuator.getDataType())
            {
            case wolkabout::DataType::BOOLEAN:
            {
                handler.reset(new ActuatorTemplateHandler<bool>());
                break;
            }
            case wolkabout::DataType::NUMERIC:
            {
                handler.reset(new ActuatorTemplateHandler<double>());
                break;
            }
            case wolkabout::DataType::STRING:
            {
                handler.reset(new ActuatorTemplateHandler<std::string>());
                break;
            }
            }

            handlers[device.getKey() + "_" + actuator.getReference()] = handler;
        }
    }

    static std::map<std::string, std::vector<wolkabout::ConfigurationItem>> localConfiguration;

    static std::map<std::string, std::tuple<int, bool>> m_firmwareStatuses;

    for (const auto& device : appConfiguration.getDevices())
    {
        for (const auto& conf : device.getManifest().getConfigurations())
        {
            localConfiguration[device.getKey()].push_back(wolkabout::ConfigurationItem{
              std::vector<std::string>(conf.getSize(), conf.getDefaultValue()), conf.getReference()});
        }

        m_firmwareStatuses[device.getKey()] = std::make_tuple(1, true);
    }

    class FirmwareInstallerImpl : public wolkabout::FirmwareInstaller
    {
    public:
        void install(const std::string& deviceKey, const std::string& firmwareFile,
                     std::function<void(const std::string& deviceKey)> onSuccess,
                     std::function<void(const std::string& deviceKey)> onFail) override
        {
            LOG(INFO) << "Installing firmware: " << firmwareFile << ", for device " << deviceKey;

            auto it = m_firmwareStatuses.find(deviceKey);
            if (it != m_firmwareStatuses.end() && std::get<1>(it->second))
            {
                ++(std::get<0>(it->second));
                std::this_thread::sleep_for(std::chrono::seconds(10));
                onSuccess(deviceKey);
            }
            else
            {
                onFail(deviceKey);
            }
        }
    };

    class FirmwareVersionProviderImpl : public wolkabout::FirmwareVersionProvider
    {
    public:
        std::string getFirmwareVersion(const std::string& deviceKey)
        {
            auto it = m_firmwareStatuses.find(deviceKey);
            if (it != m_firmwareStatuses.end())
            {
                return std::to_string(std::get<0>(it->second)) + ".0.0";
            }

            return "";
        }
    };

    auto installer = std::make_shared<FirmwareInstallerImpl>();
    auto provider = std::make_shared<FirmwareVersionProviderImpl>();

    wolk =
      wolkabout::Wolk::newBuilder()
        .actuationHandler([&](const std::string& key, const std::string& reference, const std::string& value) -> void {
            std::cout << "Actuation request received - Key: " << key << " Reference: " << reference
                      << " value: " << value << std::endl;

            std::string handlerId = key + "_" + reference;

            auto it = handlers.find(handlerId);
            if (it != handlers.end())
            {
                it->second->setValue(value);
            }
        })
        .actuatorStatusProvider([&](const std::string& key, const std::string& reference) -> wolkabout::ActuatorStatus {
            std::string handlerId = key + "_" + reference;

            auto it = handlers.find(handlerId);
            if (it != handlers.end())
            {
                return wolkabout::ActuatorStatus(it->second->getValue(), wolkabout::ActuatorStatus::State::READY);
            }

            return wolkabout::ActuatorStatus("", wolkabout::ActuatorStatus::State::READY);
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
              auto it = localConfiguration.find(deviceKey);
              if (it != localConfiguration.end())
              {
                  localConfiguration[deviceKey] = configuration;
              }
          })
        .configurationProvider([&](const std::string& deviceKey) -> std::vector<wolkabout::ConfigurationItem> {
            auto it = localConfiguration.find(deviceKey);
            if (it != localConfiguration.end())
            {
                return localConfiguration[deviceKey];
            }

            return {};
        })
        .withFirmwareUpdate(installer, provider)
        .host(appConfiguration.getLocalMqttUri())
        .build();

    for (const auto& device : appConfiguration.getDevices())
    {
        wolk->addDevice(device);
    }

    wolk->connect();

    std::random_device rd;
    std::mt19937 mt(rd());

    const unsigned interval = appConfiguration.getInterval();

    con = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if(con == NULL) {
        g_print("Not able to get connection to system bus\n");
        return 1;
    }

    loop = g_main_loop_new(NULL, FALSE);
    if(time_in_seconds == 0)
        time_in_seconds = DEFAULT_TIME;

    guint timeout_id = g_timeout_add_seconds(time_in_seconds, notify, NULL);

    prop_changed = g_dbus_connection_signal_subscribe(con,
                        "org.bluez",
                        "org.freedesktop.DBus.Properties",
                        "PropertiesChanged",
                        NULL,
                        "org.bluez.Adapter1",
                        G_DBUS_SIGNAL_FLAGS_NONE,
                        bluez_signal_adapter_changed,
                        NULL,
                        NULL);

    iface_added = g_dbus_connection_signal_subscribe(con,
                            "org.bluez",
                            "org.freedesktop.DBus.ObjectManager",
                            "InterfacesAdded",
                            NULL,
                            NULL,
                            G_DBUS_SIGNAL_FLAGS_NONE,
                            bluez_device_appeared,
                            loop,
                            NULL);

    iface_removed = g_dbus_connection_signal_subscribe(con,
                            "org.bluez",
                            "org.freedesktop.DBus.ObjectManager",
                            "InterfacesRemoved",
                            NULL,
                            NULL,
                            G_DBUS_SIGNAL_FLAGS_NONE,
                            bluez_device_disappeared,
                            loop,
                            NULL);

    rc = bluez_adapter_set_property("Powered", g_variant_new("b", TRUE));
    if(rc) {
        g_print("Not able to enable the adapter\n");
        //goto fail;
    }

    rc = bluez_adapter_call_method("StartDiscovery", NULL);
    if(rc) {
        g_print("Not able to scan for new devices\n");
        //goto fail;
    }
    is_scanning = TRUE;

    g_main_loop_run(loop);
    rc = bluez_adapter_call_method("StopDiscovery", NULL);
    if(rc)
        g_print("Not able to stop scanning\n");
    g_usleep(100);

    rc = bluez_adapter_set_property("Powered", g_variant_new("b", FALSE));
    if(rc)
        g_print("Not able to disable the adapter\n");

    fail:
    g_dbus_connection_signal_unsubscribe(con, prop_changed);
    g_dbus_connection_signal_unsubscribe(con, iface_added);
    g_dbus_connection_signal_unsubscribe(con, iface_removed);
    g_object_unref(con);
    return 0;
}
