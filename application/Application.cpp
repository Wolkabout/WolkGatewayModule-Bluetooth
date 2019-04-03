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
#include "Scanner.h"

wolkabout::Adapter adapter;
wolkabout::Scanner scanner;

std::map<std::string, int> device_status;
wolkabout::DeviceConfiguration appConfiguration;

gboolean timer_scan_publish(gpointer user_data)
{

    wolkabout::Wolk* wolk = (wolkabout::Wolk*)user_data;

    if(adapter.is_scanning){
        int rc = adapter.stop_scan();
        if(rc){
            std::cout<<"Unable to stop scanning\n";
            return FALSE;
        }

        /*for(auto it = device_status.begin(); it != device_status.end(); it++){
            wolk->addSensorReading(it->first, "T", it->second);
        }*/
        for(auto itr = wolkabout::Scanner::s_addr_found.begin(); itr != wolkabout::Scanner::s_addr_found.end(); itr++) {
            //std::cout<<*itr<<"\n";
            if(device_status.find(*itr) != device_status.end())
            {
                std::cout<<"Found the wanted device\n";
            }
        }

        wolk->publish();

        /*set all to false*/
        for (auto it = device_status.begin(); it != device_status.end(); it++) {
            device_status[it->first] = 0;
        }
    }
    else{
        int rc = adapter.start_scan();
        if(rc){
            std::cout<<"Unable to scan for new devices\n";
            return FALSE;
        }
    }

    return TRUE;
}

int main(int argc, char** argv)
{
    int rc = 0;

    auto logger = std::unique_ptr<wolkabout::ConsoleLogger>(new wolkabout::ConsoleLogger());
    logger->setLogLevel(wolkabout::LogLevel::INFO);
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

    for (const auto& device : appConfiguration.getDevices())
    {
            device_status.insert(std::pair<std::string, int>(device.getKey(), 0));
    }

    unsigned interval = appConfiguration.getInterval();

    guint timeout_id = g_timeout_add_seconds(interval, 
                                            timer_scan_publish,
                                            (void*)wolk.get());

    guint prop_changed = adapter.subscribe_adapter_changed();
    guint iface_added = adapter.subscribe_device_added(wolkabout::Scanner::device_appeared);
    guint iface_removed = adapter.subscribe_device_removed(wolkabout::Scanner::device_disappeared);

    rc = adapter.power_on();
    if(rc) {
        LOG(ERROR)<<"Unable to enable the adapter\n";
    }

    rc = adapter.start_scan();
    if(rc) {
        LOG(ERROR)<<"Unable to scan for new devices\n";
    }

    adapter.run_loop();

    return 0;
}
