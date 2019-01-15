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

#include "service/DeviceRegistrationService.h"
#include "connectivity/ConnectivityService.h"
#include "model/DeviceRegistrationRequest.h"
#include "model/Message.h"
#include "protocol/RegistrationProtocol.h"
#include "utilities/Logger.h"

namespace wolkabout
{
DeviceRegistrationService::DeviceRegistrationService(RegistrationProtocol& protocol,
                                                     ConnectivityService& connectivityService,
                                                     const RegistrationResponseHandler& registrationResponseHandler)
: m_protocol{protocol}
, m_connectivityService{connectivityService}
, m_registrationResponseHandler{registrationResponseHandler}
{
}

void DeviceRegistrationService::messageReceived(std::shared_ptr<Message> message)
{
    const std::string deviceKey = m_protocol.extractDeviceKeyFromChannel(message->getChannel());
    if (deviceKey.empty())
    {
        LOG(WARN) << "Unable to extract device key from channel: " << message->getChannel();
        return;
    }

    if (m_protocol.isRegistrationResponseMessage(*message))
    {
        const auto response = m_protocol.makeRegistrationResponse(*message);
        if (!response)
        {
            LOG(ERROR)
              << "DeviceRegistrationService: Device registration response could not be deserialized. Channel: '"
              << message->getChannel() << "' Payload: '" << message->getContent() << "'";
            return;
        }
        m_registrationResponseHandler(deviceKey, response->getResult());
    }
    else
    {
        LOG(WARN) << "Unable to parse message channel: " << message->getChannel();
    }
}

const Protocol& DeviceRegistrationService::getProtocol()
{
    return m_protocol;
}

void DeviceRegistrationService::publishRegistrationRequest(const DetailedDevice& device)
{
    DeviceRegistrationRequest request{device};

    const std::shared_ptr<Message> outboundMessage = m_protocol.makeMessage(device.getKey(), request);

    if (!outboundMessage || !m_connectivityService.publish(outboundMessage))
    {
        LOG(INFO) << "Registration request not published for device: " << device.getKey();
    }
}
}    // namespace wolkabout
