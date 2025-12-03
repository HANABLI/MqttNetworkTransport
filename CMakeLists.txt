# CMakeLists.txt for MqttNetworkTransport
#
# © 2025 by Hatem Nabli


cmake_minimum_required(VERSION 3.8)
set(this MqttNetworkTransport )

set(Headers
    include/MqttNetworkTransport/MqttClientNetworkTransport.hpp
)

set(Sources
    src/MqttClientNetworkTransport.cpp
)

add_library(${this} STATIC ${Sources} ${Headers})
set_target_properties(${this} PROPERTIES
    FOLDER Libraries
)

target_include_directories(${this} PUBLIC include)

target_link_libraries(${this} PUBLIC
    SystemUtils
    StringUtils
    MqttV5
)

add_subdirectory(test)