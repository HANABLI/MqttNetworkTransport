#ifndef MQTT_CLIENT_NETWORK_TRANSPORT_HPP
#define MQTT_CLIENT_NETWORK_TRANSPORT_HPP
/**
 * @file MqttClientNetworkTransport.hpp
 *
 * This module declare the MqttClientNetworkTransport::MqttClientNetworkTransport
 * class
 *
 * © 2025 by Hatem Nabli
 */
#include <functional>
#include <memory>
#include <MqttV5/ClientTransportLayer.hpp>
#include <SystemUtils/INetworkConnection.hpp>
#include <SystemUtils/NetworkConnection.hpp>
#include <SystemUtils/DiagnosticsSender.hpp>
#include <StringUtils/StringUtils.hpp>
namespace MqttNetworkTransport
{
    class MqttClientNetworkTransport : public MqttV5::ClientTransportLayer
    {
    public:
        /**
         * This is the type of function used to create new network connections.
         *
         * @param[in] scheme
         *      This is the scheme indicated in the URI of the target
         *      to which to establish a connection.
         * @param[in] serverName
         *      This is the name of the server to which the transport
         *      wishes to connect.
         * @return
         *      The new connection object is returned.
         */
        typedef std::function<std::shared_ptr<SystemUtils::INetworkConnection>(
            const std::string& scheme, const std::string& serverName)>
            ConnectionFactoryFunction;

        // Lifecycle management
    public:
        ~MqttClientNetworkTransport() noexcept;
        MqttClientNetworkTransport(const MqttClientNetworkTransport&) = delete;
        MqttClientNetworkTransport(MqttClientNetworkTransport&&) noexcept = delete;
        MqttClientNetworkTransport& operator=(const MqttClientNetworkTransport&) = delete;
        MqttClientNetworkTransport& operator=(MqttClientNetworkTransport&&) noexcept = delete;

        // Public methods
    public:
        /**
         * This is the default constructor.
         */
        MqttClientNetworkTransport();

        /**
         * This method forms a new subscruption to diagnostic messages published by the transport.
         *
         * @param[in] delegate
         *      This is the function to call to deliver messages
         *      to the subscriber.
         * @param[in] minLevel
         *      This is the minimum level of message that this subscriber
         *      desires to receive.
         * @return
         *      A function is returned which may be called
         *      to terminate the subscription.
         */
        SystemUtils::DiagnosticsSender::UnsubscribeDelegate SubscribeTodiagnostics(
            SystemUtils::DiagnosticsSender::DiagnosticMessageDelegate delegate,
            size_t minLevel = 0);

    public:
        virtual std::shared_ptr<MqttV5::Connection> Connect(
            const std::string& sheme, const std::string& hostNameOrAddress, uint16_t port,
            MqttV5::Connection::DataReceivedDelegate dataReceivedDelegate,
            MqttV5::Connection::BrokenDelegate brokenDelegate) override;
        // Private properties
    private:
        /**
         * This is the type of structure that contains the private properties of the instatnce. It
         * is defined in the implementation and declared here to ensure that it is scoped inside the
         * class.
         */
        struct Impl;

        /**
         * This contains the private properties of the instance.
         */
        std::unique_ptr<Impl> impl_;
    };
}  // namespace MqttNetworkTransport

#endif /** MQTT_CLIENT_NETWORK_TRANSPORT_HPP */