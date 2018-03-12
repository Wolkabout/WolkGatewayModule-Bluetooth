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

#ifndef WOLK_H
#define WOLK_H

#include "ActuationHandler.h"
#include "ActuatorStatusProvider.h"
#include "WolkBuilder.h"
#include "model/ActuatorCommand.h"
#include "model/ActuatorGetCommand.h"
#include "model/ActuatorSetCommand.h"
#include "model/ActuatorStatus.h"
#include "model/Device.h"
#include "model/DeviceRegistrationResponseDto.h"
#include "utilities/CommandBuffer.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace wolkabout
{
class ConnectivityService;
class InboundMessageHandler;
class FirmwareUpdateService;
class FileDownloadService;
class OutboundServiceDataHandler;

class Wolk
{
    friend class WolkBuilder;

public:
    virtual ~Wolk() = default;

    /**
     * @brief Initiates wolkabout::WolkBuilder that configures device to connect to WolkAbout IoT Cloud
     * @param device wolkabout::Device
     * @return wolkabout::WolkBuilder instance
     */
    static WolkBuilder newBuilder(Device device);

    /**
     * @brief Publishes sensor reading to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Sensor reference
     * @param value Sensor value<br>
     *              Supported types:<br>
     *               - bool<br>
     *               - float<br>
     *               - double<br>
     *               - signed int<br>
     *               - signed long int<br>
     *               - signed long long int<br>
     *               - unsigned int<br>
     *               - unsigned long int<br>
     *               - unsigned long long int<br>
     *               - string<br>
     *               - char*<br>
     *               - const char*<br>
     * @param rtc Reading POSIX time - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    template <typename T> void addSensorReading(const std::string& reference, T value, unsigned long long int rtc = 0);

    /**
     * @brief Publishes alarm to WolkAbout IoT Cloud<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param reference Alarm reference
     * @param value Alarm value
     * @param rtc POSIX time at which event occurred - Number of seconds since 01/01/1970<br>
     *            If omitted current POSIX time is adopted
     */
    void addAlarm(const std::string& reference, const std::string& value, unsigned long long int rtc = 0);

    /**
     * @brief Invokes ActuatorStatusProvider callback to obtain actuator status<br>
     *        This method is thread safe, and can be called from multiple thread simultaneously
     * @param Actuator reference
     */
    void publishActuatorStatus(const std::string& reference);

    void registerDevice(const Device& device);

    /**
     * @brief connect Establishes connection with WolkAbout IoT platform
     */
    void connect();

    /**
     * @brief disconnect Disconnects from WolkAbout IoT platform
     */
    void disconnect();

    /**
     * @brief publish Publishes data
     */
    void publish();

private:
    static const constexpr unsigned int PUBLISH_BATCH_ITEMS_COUNT = 50;

    Wolk(std::shared_ptr<ConnectivityService> connectivityService, std::shared_ptr<Persistence> persistence,
         std::shared_ptr<InboundMessageHandler> inboundMessageHandler,
         std::shared_ptr<OutboundServiceDataHandler> outboundServiceDataHandler, Device device);

    void addToCommandBuffer(std::function<void()> command);

    static unsigned long long int currentRtc();

    void publishActuatorStatuses();
    void publishAlarms();
    void publishSensorReadings();

    void addActuatorStatus(std::shared_ptr<ActuatorStatus> actuatorStatus);

    void handleGetActuator(const ActuatorGetCommand& actuatorCommand);
    void handleSetActuator(const ActuatorSetCommand& actuatorCommand);

    void handleRegistrationResponse(std::shared_ptr<DeviceRegistrationResponse> response);

    void publishFirmwareVersion();

    std::shared_ptr<ConnectivityService> m_connectivityService;
    std::shared_ptr<Persistence> m_persistence;

    std::shared_ptr<InboundMessageHandler> m_inboundMessageHandler;
    std::shared_ptr<OutboundServiceDataHandler> m_outboundServiceDataHandler;

    std::shared_ptr<FirmwareUpdateService> m_firmwareUpdateService;
    std::shared_ptr<FileDownloadService> m_fileDownloadService;

    Device m_device;

    std::function<void(const std::string&, const std::string&)> m_actuationHandler;

    std::function<ActuatorStatus(const std::string&)> m_actuatorStatusProvider;

    std::function<void(const std::string&, DeviceRegistrationResponse::Result)> m_registrationResponseHandler;

    std::unique_ptr<CommandBuffer> m_commandBuffer;
};
}

#endif
