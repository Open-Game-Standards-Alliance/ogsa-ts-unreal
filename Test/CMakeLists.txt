cmake_minimum_required(VERSION 3.16)
project(ogs_conformance CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(ogs_conformance main.cpp)
target_include_directories(ogs_conformance PRIVATE
    ../Source/OgsTelemetryCore/Public)
