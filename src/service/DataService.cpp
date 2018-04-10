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

#include "service/DataService.h"
#include "connectivity/ConnectivityService.h"
#include "model/ActuatorGetCommand.h"
#include "model/ActuatorSetCommand.h"
#include "model/ConfigurationSetCommand.h"
#include "model/Message.h"
#include "model/SensorReading.h"
#include "persistence/Persistence.h"
#include "protocol/DataProtocol.h"
#include "utilities/Logger.h"

#include <algorithm>

namespace wolkabout
{
const std::string DataService::PERSISTENCE_KEY_DELIMITER = "+";

DataService::DataService(DataProtocol& protocol, Persistence& persistence, ConnectivityService& connectivityService,
                         const ActuatorSetHandler& actuatorSetHandler, const ActuatorGetHandler& actuatorGetHandler,
                         const ConfigurationSetHandler& configurationSetHandler,
                         const ConfigurationGetHandler& configurationGetHandler)
: m_protocol{protocol}
, m_persistence{persistence}
, m_connectivityService{connectivityService}
, m_actuatorSetHandler{actuatorSetHandler}
, m_actuatorGetHandler{actuatorGetHandler}
, m_configurationSetHandler{configurationSetHandler}
, m_configurationGetHandler{configurationGetHandler}
{
}

void DataService::messageReceived(std::shared_ptr<Message> message)
{
    const std::string deviceKey = m_protocol.extractDeviceKeyFromChannel(message->getChannel());
    if (deviceKey.empty())
    {
        LOG(WARN) << "Unable to extract device key from channel: " << message->getChannel();
        return;
    }

    if (m_protocol.isActuatorGetMessage(message->getChannel()))
    {
        auto command = m_protocol.makeActuatorGetCommand(message);
        if (!command)
        {
            LOG(WARN) << "Unable to parse message contents: " << message->getContent();
            return;
        }

        if (m_actuatorGetHandler)
        {
            m_actuatorGetHandler(deviceKey, command->getReference());
        }
    }
    else if (m_protocol.isActuatorSetMessage(message->getChannel()))
    {
        auto command = m_protocol.makeActuatorSetCommand(message);
        if (!command)
        {
            LOG(WARN) << "Unable to parse message contents: " << message->getContent();
            return;
        }

        if (m_actuatorSetHandler)
        {
            m_actuatorSetHandler(deviceKey, command->getReference(), command->getValue());
        }
    }
    else if (m_protocol.isConfigurationGetMessage(message->getChannel()))
    {
        if (m_configurationGetHandler)
        {
            m_configurationGetHandler(deviceKey);
        }
    }
    else if (m_protocol.isConfigurationSetMessage(message->getChannel()))
    {
        auto command = m_protocol.makeConfigurationSetCommand(message);
        if (!command)
        {
            LOG(WARN) << "Unable to parse message contents: " << message->getContent();
            return;
        }

        if (m_configurationSetHandler)
        {
            m_configurationSetHandler(deviceKey, command->getValues());
        }
    }
    else
    {
        LOG(WARN) << "Unable to parse message channel: " << message->getChannel();
    }
}

const Protocol& DataService::getProtocol()
{
    return m_protocol;
}

void DataService::addSensorReading(const std::string& deviceKey, const std::string& reference, const std::string& value,
                                   unsigned long long int rtc)
{
    auto sensorReading = std::make_shared<SensorReading>(value, reference, rtc);

    m_persistence.putSensorReading(makePersistenceKey(deviceKey, reference), sensorReading);
}

void DataService::addAlarm(const std::string& deviceKey, const std::string& reference, const std::string& value,
                           unsigned long long int rtc)
{
    auto alarm = std::make_shared<Alarm>(value, reference, rtc);

    m_persistence.putAlarm(makePersistenceKey(deviceKey, reference), alarm);
}

void DataService::addActuatorStatus(const std::string& deviceKey, const std::string& reference,
                                    const std::string& value, ActuatorStatus::State state)
{
    auto actuatorStatusWithRef = std::make_shared<ActuatorStatus>(value, reference, state);

    m_persistence.putActuatorStatus(makePersistenceKey(deviceKey, reference), actuatorStatusWithRef);
}

void DataService::addConfiguration(const std::string& deviceKey,
                                   const std::map<std::string, std::string>& configuration)
{
    m_persistence.putConfiguration(deviceKey, configuration);
}

void DataService::publishSensorReadings()
{
    for (const auto& key : m_persistence.getSensorReadingsKeys())
    {
        const auto sensorReadings = m_persistence.getSensorReadings(key, PUBLISH_BATCH_ITEMS_COUNT);

        auto pair = parsePersistenceKey(key);
        if (pair.first.empty() || pair.second.empty())
        {
            LOG(ERROR) << "Unable to parse persistence key: " << key;
            m_persistence.removeSensorReadings(key, PUBLISH_BATCH_ITEMS_COUNT);
            break;
        }

        const std::shared_ptr<Message> outboundMessage = m_protocol.makeMessage(pair.first, sensorReadings);

        if (outboundMessage && m_connectivityService.publish(outboundMessage))
        {
            m_persistence.removeSensorReadings(key, PUBLISH_BATCH_ITEMS_COUNT);
        }
    }

    if (!m_persistence.getSensorReadingsKeys().empty())
    {
        publishSensorReadings();
    }
}

void DataService::publishSensorReadings(const std::string& deviceKey)
{
    const auto& keys = m_persistence.getSensorReadingsKeys();

    auto searchForKey = [&] {
        return std::find_if(keys.begin(), keys.end(), [&](const std::string& key) {
            auto pair = parsePersistenceKey(key);
            return pair.first == deviceKey;
        });
    };

    auto it = searchForKey();
    if (it == keys.end())
    {
        return;
    }

    const auto key = *it;

    const auto sensorReadings = m_persistence.getSensorReadings(key, PUBLISH_BATCH_ITEMS_COUNT);

    auto pair = parsePersistenceKey(key);
    if (pair.first.empty() || pair.second.empty())
    {
        LOG(ERROR) << "Unable to parse persistence key: " << key;
        m_persistence.removeSensorReadings(key, PUBLISH_BATCH_ITEMS_COUNT);
        return;
    }

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeMessage(pair.first, sensorReadings);

    if (outboundMessage && m_connectivityService.publish(outboundMessage))
    {
        m_persistence.removeSensorReadings(key, PUBLISH_BATCH_ITEMS_COUNT);
    }

    const auto& newKeys = m_persistence.getSensorReadingsKeys();
    if (auto tmp = searchForKey() != newKeys.end())
    {
        publishSensorReadings(deviceKey);
    }
}

void DataService::publishAlarms()
{
    for (const auto& key : m_persistence.getAlarmsKeys())
    {
        const auto alarms = m_persistence.getAlarms(key, PUBLISH_BATCH_ITEMS_COUNT);

        auto pair = parsePersistenceKey(key);
        if (pair.first.empty() || pair.second.empty())
        {
            LOG(ERROR) << "Unable to parse persistence key: " << key;
            m_persistence.removeAlarms(key, PUBLISH_BATCH_ITEMS_COUNT);
            break;
        }

        const std::shared_ptr<Message> outboundMessage = m_protocol.makeMessage(pair.first, alarms);

        if (outboundMessage && m_connectivityService.publish(outboundMessage))
        {
            m_persistence.removeAlarms(key, PUBLISH_BATCH_ITEMS_COUNT);
        }
    }

    if (!m_persistence.getAlarmsKeys().empty())
    {
        publishAlarms();
    }
}

void DataService::publishAlarms(const std::string& deviceKey)
{
    const auto& keys = m_persistence.getAlarmsKeys();

    auto searchForKey = [&] {
        return std::find_if(keys.begin(), keys.end(), [&](const std::string& key) {
            auto pair = parsePersistenceKey(key);
            return pair.first == deviceKey;
        });
    };

    auto it = searchForKey();
    if (it == keys.end())
    {
        return;
    }

    const auto key = *it;

    const auto alarms = m_persistence.getAlarms(key, PUBLISH_BATCH_ITEMS_COUNT);

    auto pair = parsePersistenceKey(key);
    if (pair.first.empty() || pair.second.empty())
    {
        LOG(ERROR) << "Unable to parse persistence key: " << key;
        m_persistence.removeAlarms(key, PUBLISH_BATCH_ITEMS_COUNT);
        return;
    }

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeMessage(pair.first, alarms);

    if (outboundMessage && m_connectivityService.publish(outboundMessage))
    {
        m_persistence.removeAlarms(key, PUBLISH_BATCH_ITEMS_COUNT);
    }

    const auto& newKeys = m_persistence.getAlarmsKeys();
    if (auto tmp = searchForKey() != newKeys.end())
    {
        publishAlarms(deviceKey);
    }
}

void DataService::publishActuatorStatuses()
{
    for (const auto& key : m_persistence.getActuatorStatusesKeys())
    {
        const auto actuatorStatus = m_persistence.getActuatorStatus(key);

        auto pair = parsePersistenceKey(key);
        if (pair.first.empty() || pair.second.empty())
        {
            LOG(ERROR) << "Unable to parse persistence key: " << key;
            m_persistence.removeActuatorStatus(key);
            break;
        }

        const std::shared_ptr<Message> outboundMessage = m_protocol.makeMessage(pair.first, {actuatorStatus});

        if (outboundMessage && m_connectivityService.publish(outboundMessage))
        {
            m_persistence.removeActuatorStatus(key);
        }
    }

    if (!m_persistence.getActuatorStatusesKeys().empty())
    {
        publishActuatorStatuses();
    }
}
void DataService::publishActuatorStatuses(const std::string& deviceKey)
{
    const auto& keys = m_persistence.getActuatorStatusesKeys();

    auto searchForKey = [&] {
        return std::find_if(keys.begin(), keys.end(), [&](const std::string& key) {
            auto pair = parsePersistenceKey(key);
            return pair.first == deviceKey;
        });
    };

    auto it = searchForKey();
    if (it == keys.end())
    {
        return;
    }

    const auto key = *it;

    const auto actuatorStatus = m_persistence.getActuatorStatus(key);

    auto pair = parsePersistenceKey(key);
    if (pair.first.empty() || pair.second.empty())
    {
        LOG(ERROR) << "Unable to parse persistence key: " << key;
        m_persistence.removeActuatorStatus(key);
        return;
    }

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeMessage(pair.first, {actuatorStatus});

    if (outboundMessage && m_connectivityService.publish(outboundMessage))
    {
        m_persistence.removeActuatorStatus(key);
    }

    const auto& newKeys = m_persistence.getActuatorStatusesKeys();
    if (auto tmp = searchForKey() != newKeys.end())
    {
        publishActuatorStatuses(deviceKey);
    }
}

void DataService::publishConfiguration()
{
    const auto& keys = m_persistence.getConfigurationKeys();

    for (const auto& key : keys)
    {
        publishConfiguration(key);
    }
}

void DataService::publishConfiguration(const std::string& deviceKey)
{
    const auto& keys = m_persistence.getConfigurationKeys();

    auto searchForKey = [&] { return std::find(keys.begin(), keys.end(), deviceKey); };

    auto it = searchForKey();
    if (it == keys.end())
    {
        return;
    }

    const auto configuration = m_persistence.getConfiguration(deviceKey);

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeFromConfiguration(deviceKey, *configuration);

    if (outboundMessage && m_connectivityService.publish(outboundMessage))
    {
        m_persistence.removeConfiguration(deviceKey);
    }
}

std::string DataService::makePersistenceKey(const std::string& deviceKey, const std::string& reference)
{
    return deviceKey + PERSISTENCE_KEY_DELIMITER + reference;
}

std::pair<std::string, std::string> DataService::parsePersistenceKey(const std::string& key)
{
    auto pos = key.find(PERSISTENCE_KEY_DELIMITER);
    if (pos == std::string::npos)
    {
        return std::make_pair("", "");
    }

    auto deviceKey = key.substr(0, pos);
    auto reference = key.substr(pos + PERSISTENCE_KEY_DELIMITER.size(), std::string::npos);

    return std::make_pair(deviceKey, reference);
}
}    // namespace wolkabout
