cmake_minimum_required(VERSION 3.25)

project(YaapSampleProvider LANGUAGES CXX)

find_package(YaapProviderSdk 1 CONFIG REQUIRED)

add_executable(YaapSampleProvider main.cpp)
target_link_libraries(YaapSampleProvider PRIVATE Yaap::ProviderSdk)
target_compile_features(YaapSampleProvider PRIVATE cxx_std_20)

