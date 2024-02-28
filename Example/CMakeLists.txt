cmake_minimum_required(VERSION 3.5)

project(01_RTS_DTR VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(01_RTS_DTR
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET 01_RTS_DTR APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(01_RTS_DTR SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(01_RTS_DTR
            ${PROJECT_SOURCES}
            ../esptoolqt.h ../esptoolqt.cpp
            ../esp_reset.cpp
            ../esp_serial.cpp
            ../esp_spi.cpp
            ../espdefines.h
            ../targets/esp_base.h
            ../targets/esp8266.h
            ../targets/esp8266.cpp
            ../targets/esp32.h
            ../targets/esp32.cpp
            ../targets/esp32s2.h
            ../targets/esp32s2.cpp
            ../targets/esp32s3.h
            ../targets/esp32s3.cpp
            ../targets/esp32c2.h
            ../targets/esp32c2.cpp
            ../targets/esp32c3.h
            ../targets/esp32c3.cpp
            ../targets/esp32c6.h
            ../targets/esp32c6.cpp
            ../targets/esp32h2.h
            ../targets/esp32h2.cpp
            ../targets/esp32p4.h
            ../targets/esp32p4.cpp
        )
    endif()
endif()

target_link_libraries(01_RTS_DTR PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::SerialPort)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.01_RTS_DTR)
endif()
set_target_properties(01_RTS_DTR PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE FALSE
)

include(GNUInstallDirs)
install(TARGETS 01_RTS_DTR
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(01_RTS_DTR)
endif()
