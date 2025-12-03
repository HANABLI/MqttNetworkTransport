/**
 * @file MqttClientNetworkTransport.hpp
 *
 * This module implements the MqttClientNetworkTransport::MqttClientNetworkTransport
 * class
 *
 * © 2025 by Hatem Nabli
 */

#include "MqttNetworkTransport/MqttClientNetworkTransport.hpp"
#include <mutex>

namespace
{
    struct ConnectionDelegates
    {
        /**
         * This is used to synchronize access to the delegates.
         */
        std::recursive_mutex mutex;

        /**
         * This is the delegate to call whenever data is received
         * from the remote peer.
         */
        MqttV5::Connection::DataReceivedDelegate dataReceivedDelegate;

        /**
         * This is the delegate to call whenever the connection has
         * been broken.
         */
        MqttV5::Connection::BrokenDelegate brokenDelegate;
    };

    struct ConnectionAdapter : public MqttV5::Connection
    {
        /**
         * This is the object wish implementing the network connection
         * in terms of the operating system's network API.
         */
        std::shared_ptr<SystemUtils::INetworkConnection> networkConnectionadaptee;

        /**
         * This holds onto the user's delegate and makes their setting
         * and usage thread-safe.
         */
        std::shared_ptr<ConnectionDelegates> connectionDelegates =
            std::make_shared<ConnectionDelegates>();

        // Mqtt::Connection Methods

        virtual std::string GetPeerId() override {
            return StringUtils::sprintf(
                "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ":%" PRIu16,
                (uint8_t)((networkConnectionadaptee->GetPeerAddress() >> 24) & 0xFF),
                (uint8_t)((networkConnectionadaptee->GetPeerAddress() >> 16) & 0xFF),
                (uint8_t)((networkConnectionadaptee->GetPeerAddress() >> 8) & 0xFF),
                (uint8_t)(networkConnectionadaptee->GetPeerAddress() & 0xFF),
                networkConnectionadaptee->GetPeerPort());
        }

        virtual void SetDataReceivedDelegate(
            DataReceivedDelegate newDataReceivedDelegate) override {
            std::lock_guard<decltype(connectionDelegates->mutex)> lock(connectionDelegates->mutex);
            connectionDelegates->dataReceivedDelegate = newDataReceivedDelegate;
        }

        virtual void SetConnectionBrokenDelegate(BrokenDelegate brokenDelegate) override {
            std::lock_guard<decltype(connectionDelegates->mutex)> lock(connectionDelegates->mutex);
            connectionDelegates->brokenDelegate = brokenDelegate;
        }

        virtual void SendData(const std::vector<uint8_t>& data) override {
            networkConnectionadaptee->SendMessage(data);
        }

        virtual void Break(const bool clean) override { networkConnectionadaptee->Close(clean); }
    };
}  // namespace

namespace MqttNetworkTransport
{
    struct MqttClientNetworkTransport::Impl
    {
        /**
         * This is a helper object used to generate and publish diagnostics messages.
         */
        std::shared_ptr<SystemUtils::DiagnosticsSender> diagnosticsSender;

        /**
         * This function is used to create a new connection.
         */
        ConnectionFactoryFunction connectionFactory;

        /**
         * This is the constructor for the structure.
         */

        Impl() :
            diagnosticsSender(
                std::make_shared<SystemUtils::DiagnosticsSender>("MqttClientNetworkTransport")),
            connectionFactory(
                [](const std::string&, const std::string&)
                {
                    const auto connection = std::make_shared<SystemUtils::NetworkConnection>();
                    return connection;
                }) {}
    };

    MqttClientNetworkTransport::~MqttClientNetworkTransport() noexcept = default;
    MqttClientNetworkTransport::MqttClientNetworkTransport() : impl_(new Impl) {}

    SystemUtils::DiagnosticsSender::UnsubscribeDelegate
    MqttClientNetworkTransport::SubscribeTodiagnostics(
        SystemUtils::DiagnosticsSender::DiagnosticMessageDelegate delegate, size_t minLevel) {
        return impl_->diagnosticsSender->SubscribeToDiagnostics(delegate, minLevel);
    }

    // void MqttClientNetworkTransport::SetConnectionFactory(
    //     ConnectionFactoryFunction connectionFactory) {
    //     impl_->connectionFactory = connectionFactory;
    // }

    std::shared_ptr<MqttV5::Connection> MqttClientNetworkTransport::Connect(
        const std::string& scheme, const std::string& hostNameOrAdrress, uint16_t port,
        MqttV5::Connection::DataReceivedDelegate dataReceivedDelegate,
        MqttV5::Connection::BrokenDelegate brokenDelegate) {
        const auto adapter = std::make_shared<ConnectionAdapter>();
        const auto peerId = StringUtils::sprintf("%s:%" PRIu16, hostNameOrAdrress.c_str(), port);
        adapter->networkConnectionadaptee = impl_->connectionFactory(scheme, hostNameOrAdrress);
        if (adapter->networkConnectionadaptee == nullptr)
        {
            impl_->diagnosticsSender->SendDiagnosticInformationFormatted(
                SystemUtils::DiagnosticsSender::Levels::ERROR,
                "Unabale to create connection to '%s'", peerId.c_str());
            return nullptr;
        }
        auto diagnosticsSender = impl_->diagnosticsSender;
        adapter->networkConnectionadaptee->SubscribeToDiagnostics(
            [diagnosticsSender, peerId](std::string senderName, size_t level, std::string message)
            { diagnosticsSender->SendDiagnosticInformationString(level, peerId + ": " + message); },
            1);
        const uint32_t address =
            SystemUtils::NetworkConnection::GetAddressOfHost(hostNameOrAdrress);
        if (address == 0)
        {
            impl_->diagnosticsSender->SendDiagnosticInformationFormatted(
                SystemUtils::DiagnosticsSender::Levels::ERROR,
                "There is no address to get from '%s'", hostNameOrAdrress.c_str());
            return nullptr;
        }
        if (!adapter->networkConnectionadaptee->Connect(address, port))
        {
            impl_->diagnosticsSender->SendDiagnosticInformationFormatted(
                SystemUtils::DiagnosticsSender::Levels::ERROR, "Unable to connect to '%s'",
                peerId.c_str());
            return nullptr;
        }
        adapter->connectionDelegates->dataReceivedDelegate = dataReceivedDelegate;
        adapter->connectionDelegates->brokenDelegate = brokenDelegate;
        const auto delegatesCopy = adapter->connectionDelegates;
        if (!adapter->networkConnectionadaptee->Process(
                [delegatesCopy](const std::vector<uint8_t>& message)
                {
                    MqttV5::Connection::DataReceivedDelegate dataReceivedDelegate;
                    {
                        std::lock_guard<decltype(delegatesCopy->mutex)> lock(delegatesCopy->mutex);
                        dataReceivedDelegate = delegatesCopy->dataReceivedDelegate;
                    }
                    if (dataReceivedDelegate != nullptr)
                    { dataReceivedDelegate(message); }
                },
                [delegatesCopy](bool graceful)
                {
                    MqttV5::Connection::BrokenDelegate brokenDelegate;
                    {
                        std::lock_guard<decltype(delegatesCopy->mutex)> lock(delegatesCopy->mutex);
                        brokenDelegate = delegatesCopy->brokenDelegate;
                    }
                    if (brokenDelegate != nullptr)
                    { brokenDelegate(graceful); }
                }))
        {
            impl_->diagnosticsSender->SendDiagnosticInformationString(
                SystemUtils::DiagnosticsSender::Levels::ERROR,
                " Error to start to process listening for incoming and sending outgoing "
                "messages. ");
            return nullptr;
        }
        return adapter;
    }
}  // namespace MqttNetworkTransport