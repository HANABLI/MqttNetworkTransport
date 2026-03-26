/**
 * @file MqttClientNetworkTransport.cpp
 *
 * This module implements the MqttClientNetworkTransport::MqttClientNetworkTransport
 * class as a TCP client endpoint that keeps the connection open.
 *
 * © 2025 by Hatem Nabli
 */

#include "MqttNetworkTransport/MqttClientNetworkTransport.hpp"
#include <SystemUtils/NetworkConnection.hpp>
#include <SystemUtils/DiagnosticsSender.hpp>
#include <StringUtils/StringUtils.hpp>
#include <mutex>

namespace
{
    struct ConnectionDelegates
    {
        std::recursive_mutex mutex;
        MqttV5::Connection::DataReceivedDelegate dataReceivedDelegate;
        MqttV5::Connection::BrokenDelegate brokenDelegate;
    };

    struct ConnectionAdapter : public MqttV5::Connection
    {
        std::shared_ptr<SystemUtils::INetworkConnection> networkConnection;
        std::shared_ptr<ConnectionDelegates> delegates = std::make_shared<ConnectionDelegates>();

        bool WireUp() {
            // Lance la boucle de traitement réseau (lecture/écriture) en tâche de fond.
            return networkConnection->DoWork(
                [delegates = delegates](const std::vector<uint8_t>& message)
                {
                    std::lock_guard<std::recursive_mutex> lock(delegates->mutex);
                    if (delegates->dataReceivedDelegate)
                    { delegates->dataReceivedDelegate(message); }
                },
                [delegates = delegates](bool graceful)
                {
                    std::lock_guard<std::recursive_mutex> lock(delegates->mutex);
                    if (delegates->brokenDelegate)
                    { delegates->brokenDelegate(graceful); }
                });
        }

        // ========== MqttV5::Connection interface ==========

        virtual std::string GetPeerId() override {
            const uint32_t addr = networkConnection->GetPeerAddress();
            const uint16_t port = networkConnection->GetPeerPort();

            const uint8_t a = static_cast<uint8_t>((addr >> 24) & 0xFF);
            const uint8_t b = static_cast<uint8_t>((addr >> 16) & 0xFF);
            const uint8_t c = static_cast<uint8_t>((addr >> 8) & 0xFF);
            const uint8_t d = static_cast<uint8_t>(addr & 0xFF);

            return StringUtils::sprintf("%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ":%" PRIu16, a,
                                        b, c, d, port);
        }

        virtual void SetDataReceivedDelegate(
            DataReceivedDelegate newDataReceivedDelegate) override {
            std::lock_guard<std::recursive_mutex> lock(delegates->mutex);
            delegates->dataReceivedDelegate = newDataReceivedDelegate;
        }

        virtual void SetConnectionBrokenDelegate(BrokenDelegate newBrokenDelegate) override {
            std::lock_guard<std::recursive_mutex> lock(delegates->mutex);
            delegates->brokenDelegate = newBrokenDelegate;
        }

        virtual void SendData(const std::vector<uint8_t>& data) override {
            networkConnection->SendMessage(data);
        }

        virtual void Break(const bool clean) override { networkConnection->Close(clean); }
    };
}  // namespace

namespace MqttNetworkTransport
{
    struct MqttClientNetworkTransport::Impl
    {
        std::shared_ptr<SystemUtils::DiagnosticsSender> diagnosticsSender;

        ConnectionFactoryFunction connectionFactory;

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

    std::shared_ptr<MqttV5::Connection> MqttClientNetworkTransport::Connect(
        const std::string& scheme, const std::string& hostNameOrAdrress, uint16_t port,
        MqttV5::Connection::DataReceivedDelegate dataReceivedDelegate,
        MqttV5::Connection::BrokenDelegate brokenDelegate) {
        (void)scheme;  // pour futur support "mqtts" / TLS

        const auto adapter = std::make_shared<ConnectionAdapter>();
        const auto peerId = StringUtils::sprintf("%s:%" PRIu16, hostNameOrAdrress.c_str(), port);

        adapter->networkConnection = impl_->connectionFactory(scheme, hostNameOrAdrress);
        if (adapter->networkConnection == nullptr)
        {
            impl_->diagnosticsSender->SendDiagnosticInformationFormatted(
                SystemUtils::DiagnosticsSender::Levels::ERROR,
                "Unable to create connection to '%s'", peerId.c_str());
            return nullptr;
        }

        auto diagnosticsSender = impl_->diagnosticsSender;
        adapter->networkConnection->SubscribeToDiagnostics(
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

        if (!adapter->networkConnection->Connect(address, port))
        {
            impl_->diagnosticsSender->SendDiagnosticInformationFormatted(
                SystemUtils::DiagnosticsSender::Levels::ERROR, "Unable to connect to '%s'",
                peerId.c_str());
            return nullptr;
        }

        // Initialise les delegates avant de lancer la boucle Process()
        adapter->delegates->dataReceivedDelegate = dataReceivedDelegate;
        adapter->delegates->brokenDelegate = brokenDelegate;

        if (!adapter->WireUp())
        {
            impl_->diagnosticsSender->SendDiagnosticInformationFormatted(
                SystemUtils::DiagnosticsSender::Levels::ERROR,
                "Failed to start processing on connection to '%s'", peerId.c_str());
            adapter->networkConnection->Close(false);
            return nullptr;
        }

        return adapter;
    }
}  // namespace MqttNetworkTransport
