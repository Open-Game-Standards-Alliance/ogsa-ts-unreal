cmake_minimum_required(VERSION 3.16)
project(ogsa_conformance CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(ogsa_conformance main.cpp)
target_include_directories(ogsa_conformance PRIVATE
    ../Source/OgsaTelemetryCore/Public)
