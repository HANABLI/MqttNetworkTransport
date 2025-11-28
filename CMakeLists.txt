# CMakeLists.txt for MqttNetworkTransport
#
# © 2025 by Hatem Nabli


cmake_minimum_required(VERSION 3.8)
set(this MqttNetworkTransport )

set(Headers
    include/MqttNetworkTransport/MqttNetworkTransport.hpp
)

set(Sources
    src/MqttNetworkTransport.cpp
)

add_library(${this} STATIC ${Sources} ${Headers})
set_target_properties(${this} PROPERTIES
    FOLDER Libraries
)

target_include_directories(${this} PUBLIC include)

add_subdirectory(test)