cmake_minimum_required(VERSION 3.19)
project(IMSystem LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(IMSystem
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    common/protocol.h
    common/message.h common/message.cpp
    common/utils.h common/utils.cpp
)

target_link_libraries(IMSystem
    PRIVATE
        Qt::Core
        Qt::Widgets
)

# 添加 common 目录到 include 路径
target_include_directories(IMSystem PRIVATE ${CMAKE_SOURCE_DIR}/common)

install(TARGETS IMSystem
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET IMSystem
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
