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
#include "protocol/json/JsonDto.h"
#include "utilities/FileSystemUtils.h"
#include "utilities/json.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace wolkabout
{
using nlohmann::json;

DeviceConfiguration::DeviceConfiguration(std::string localMqttUri, unsigned interval,
                                         std::vector<wolkabout::Device> devices)
: m_localMqttUri(std::move(localMqttUri)), m_interval(interval), m_devices(std::move(devices))
{
}

const std::string& DeviceConfiguration::getLocalMqttUri() const
{
    return m_localMqttUri;
}

unsigned DeviceConfiguration::getInterval() const
{
    return m_interval;
}

const std::vector<wolkabout::Device>& DeviceConfiguration::getDevices() const
{
    return m_devices;
}

wolkabout::DeviceConfiguration DeviceConfiguration::fromJson(const std::string& deviceConfigurationFile)
{
    if (!FileSystemUtils::isFilePresent(deviceConfigurationFile))
    {
        throw std::logic_error("Given gateway configuration file does not exist.");
    }

    std::string deviceConfigurationJson;
    if (!FileSystemUtils::readFileContent(deviceConfigurationFile, deviceConfigurationJson))
    {
        throw std::logic_error("Unable to read gateway configuration file.");
    }

    auto j = json::parse(deviceConfigurationJson);

    const auto localMqttUri = j.at("host").get<std::string>();
    const auto interval = j.at("readingsInterval").get<unsigned>();

    std::vector<Device> devices;
    for (auto& element : j.at("devices"))
    {
        const auto manifest = element.at("manifest").get<wolkabout::DeviceManifest>();
        const auto name = element.at("name").get<std::string>();
        const auto key = element.at("key").get<std::string>();

        devices.push_back(Device(name, key, manifest));
    }

    return DeviceConfiguration(localMqttUri, interval, devices);
}
}    // namespace wolkabout
