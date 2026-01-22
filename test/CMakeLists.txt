# CMakeLists.txt for StringUtils
#
# © 2025 by Hatem Nabli

cmake_minimum_required(VERSION 3.20)
set(this MqttNetworkTransportTest)

set(Sources
    src/MqttClientNetworkTransportTests.cpp
)

add_executable(${this} ${Sources})
set_target_properties(${this} PROPERTIES
    FOLDER Tests
)

target_include_directories(${this} PRIVATE ..)

target_link_libraries(${this} PUBLIC
    gtest_main
    MqttV5
    StringUtils
    SystemUtils
    MqttNetworkTransport
)

add_test(
    NAME ${this}
    COMMAND ${this}
)