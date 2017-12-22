cmake_minimum_required(VERSION 2.8.11)

project(dht-explorer)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Find the QtWidgets library
find_package(Qt5Widgets)
find_package(Qt5Network)
find_package(OpenSSL)

include_directories(${OPENSSL_INCLUDE_DIR})

set(SRCS_LIST
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/unix.c
    src/hashvalidator.h
    src/dht/dht.c
    src/dht/dht.h
)

set(UIS_LIST
    src/mainwindow.ui
)

QT5_WRAP_UI(UI_HEADERS ${UIS_LIST})

QT5_ADD_RESOURCES(RESOURCES_LIST
    resources/icons.qrc
)

add_executable(dht-explorer ${SRCS_LIST} ${UI_HEADERS} ${RESOURCES_LIST})
target_link_libraries(dht-explorer Qt5::Widgets Qt5::Network ${OPENSSL_LIBRARIES})
