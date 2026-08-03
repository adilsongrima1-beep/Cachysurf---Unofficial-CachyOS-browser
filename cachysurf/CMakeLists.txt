cmake_minimum_required(VERSION 3.22)
project(CachySurf VERSION 0.4.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)

find_package(Qt6 6.7 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebChannel
    WebEngineCore
    WebEngineWidgets
)
find_package(Qt6Keychain REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi-hidraw)

qt_add_executable(cachysurf
    src/main.cpp
    src/browserwindow.cpp
    src/browserwindow.h
    src/adaptivetabbar.cpp
    src/adaptivetabbar.h
    src/animatedaddressbar.cpp
    src/animatedaddressbar.h
    src/passwordstore.cpp
    src/hidbridge.cpp
    src/hidbridge.h
    src/passwordstore.h
    src/webview.cpp
    src/webview.h
    src/trackerblocker.cpp
    src/trackerblocker.h
    resources.qrc
)

target_link_libraries(cachysurf PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebChannel
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
    Qt6Keychain::Qt6Keychain
    PkgConfig::HIDAPI
)

target_compile_definitions(cachysurf PRIVATE
    CACHYSURF_VERSION="${PROJECT_VERSION}"
)

install(TARGETS cachysurf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/io.cachyos.CachySurf.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES assets/cachysurf.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
install(PROGRAMS install-webhid-access.sh
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cachysurf)
