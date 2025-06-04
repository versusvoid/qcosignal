cmake_minimum_required(VERSION 3.16)
project(qcosignal LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-Wall -Wextra -Wpedantic -DCOSIGNAL_DEBUG=1)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

qt_add_executable(qcosignal WIN32 MACOSX_BUNDLE
    main.cpp
    myobject.cpp
)

target_link_libraries(qcosignal PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
)
