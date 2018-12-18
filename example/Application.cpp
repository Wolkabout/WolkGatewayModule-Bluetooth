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
    auto logger = std::unique_ptr<wolkabout::ConsoleLogger>(new wolkabout::ConsoleLogger());
    logger->setLogLevel(wolkabout::LogLevel::DEBUG);
    wolkabout::Logger::setInstance(std::move(logger));

    if (argc < 2)
    {
        LOG(ERROR) << "WolkGatewayModule Application: Usage -  " << argv[0] << " [configurationFilePath]";
        return -1;
    }

    wolkabout::DeviceConfiguration appConfiguration;
    try
    {
        appConfiguration = wolkabout::DeviceConfiguration::fromJson(argv[1]);
    }
    catch (std::logic_error& e)
    {
        LOG(ERROR) << "WolkGatewayModule Application: Unable to parse configuration file. Reason: " << e.what();
        return -1;
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

    std::unique_ptr<wolkabout::Wolk> wolk =
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

    while (true)
    {
        for (const auto& device : appConfiguration.getDevices())
        {
            for (const auto& sensor : device.getManifest().getSensors())
            {
                std::vector<int> values;

                if (appConfiguration.getValueGenerator() == wolkabout::ValueGenerator::INCEREMENTAL)
                {
                    static int value = 0;
                    for (size_t i = 0; i < sensor.getSize(); ++i)
                    {
                        values.push_back(++value);
                    }
                }
                else
                {
                    std::uniform_int_distribution<int> dist(sensor.getMinimum(), sensor.getMaximum());

                    for (size_t i = 0; i < sensor.getSize(); ++i)
                    {
                        int rand_num = dist(mt);
                        values.push_back(rand_num);
                    }
                }

                wolk->addSensorReading(device.getKey(), sensor.getReference(), values);
            }
        }

        wolk->publish();

        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
    return 0;
}
