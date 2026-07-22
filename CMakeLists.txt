cmake_minimum_required(VERSION 3.19)
project(IMSystem LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Sql)

qt_standard_project_setup()

# ========== 服务端可执行文件 ==========
qt_add_executable(IMServer
    server/main.cpp
    server/tcpserver.h
    server/tcpserver.cpp
    server/clienthandler.h
    server/clienthandler.cpp
    server/database/dbmanager.h
    server/database/dbmanager.cpp
    server/service/authservice.h
    server/service/authservice.cpp
)

target_link_libraries(IMServer
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Sql
)

target_include_directories(IMServer PRIVATE 
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/server/database
    ${CMAKE_SOURCE_DIR}/server/service
)

# 复制 init.sql 到构建目录
add_custom_command(TARGET IMServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/server/database/init.sql"
        "$<TARGET_FILE_DIR:IMServer>/init.sql"
    COMMENT "复制 init.sql 到输出目录"
)

# ========== 客户端可执行文件 ==========
qt_add_executable(IMClient
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    common/protocol.h
    common/message.h
    common/message.cpp
    common/utils.h
    common/utils.cpp
)

target_link_libraries(IMClient
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)

target_include_directories(IMClient PRIVATE 
    ${CMAKE_SOURCE_DIR}/common
)

install(TARGETS IMClient IMServer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET IMClient
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
