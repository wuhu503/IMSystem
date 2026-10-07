cmake_minimum_required(VERSION 3.19)
project(IMSystem LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Sql)

qt_standard_project_setup()

# 打开常见告警，避免回归（当前代码在 MinGW 下为 0 告警）
if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ========== 公共模块（静态库） ==========
add_library(common STATIC
    common/Protocol.h
    common/Constants.h
    common/Message.h
    common/Message.cpp
    common/Utils.h
    common/Utils.cpp
)

# QPasswordDigestor（PBKDF2）位于 QtNetwork 模块
target_link_libraries(common PUBLIC Qt::Core Qt::Network)
target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR}/common)

# ========== 服务端可执行文件 ==========
qt_add_executable(IMServer
    server/main.cpp
    server/TcpServer.h
    server/TcpServer.cpp
    server/ClientHandler.h
    server/ClientHandler.cpp
    server/UserManager.h
    server/UserManager.cpp
    server/database/DbManager.h
    server/database/DbManager.cpp
    server/database/DbTypes.h
    server/database/UserRepository.h
    server/database/UserRepository.cpp
    server/database/FriendRepository.h
    server/database/FriendRepository.cpp
    server/database/MessageRepository.h
    server/database/MessageRepository.cpp
    server/service/AuthService.h
    server/service/AuthService.cpp
    server/service/FriendService.h
    server/service/FriendService.cpp
    server/service/ChatService.h
    server/service/ChatService.cpp
    server/service/Reply.h
    server/threading/TaskRunner.h
    server/threading/TaskRunner.cpp
    server/threading/DbConnectionHelper.h
    server/threading/DbConnectionHelper.cpp
)

target_link_libraries(IMServer PRIVATE common Qt::Core Qt::Network Qt::Sql)

target_include_directories(IMServer PRIVATE 
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/server/database
    ${CMAKE_SOURCE_DIR}/server/service
    ${CMAKE_SOURCE_DIR}/server/threading
)

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
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
    client/TcpClient.h
    client/TcpClient.cpp
    client/ChatSession.h
    client/ChatSession.cpp
    client/FriendStore.h
    client/FriendStore.cpp
    client/ui/UiKit.h
    client/ui/UiKit.cpp
    client/ui/ClientDialogs.h
    client/ui/ClientDialogs.cpp
    client/ui/LoginDialog.h
    client/ui/LoginDialog.cpp
    client/ui/LoginDialog.ui
    client/ui/RegisterDialog.h
    client/ui/RegisterDialog.cpp
    client/ui/RegisterDialog.ui
)

target_link_libraries(IMClient PRIVATE common Qt::Core Qt::Widgets Qt::Network)

target_include_directories(IMClient PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/IMClient_autogen/include
    ${CMAKE_SOURCE_DIR}/client
    ${CMAKE_SOURCE_DIR}/client/ui
)

install(TARGETS IMClient IMServer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 服务端启动时需要读取建表脚本
install(FILES server/database/init.sql
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET IMClient
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

qt_generate_deploy_app_script(
    TARGET IMServer
    OUTPUT_SCRIPT deploy_script_server
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script_server})
