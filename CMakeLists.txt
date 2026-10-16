cmake_minimum_required(VERSION 3.20)

project(CppAsioNetworking
    VERSION 1.0.0
    DESCRIPTION "C++ networking examples using Boost.Asio and Linux epoll"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED)

function(add_network_example target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE Threads::Threads Boost::headers)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_network_example(asio_server asio_server.cpp)
add_network_example(asio_client asio_client.cpp)
add_network_example(asio_async_server asio_async_server.cpp)
add_network_example(asio_persistent_client asio_persistent_client.cpp)
add_network_example(asio_push_server asio_push_server.cpp)
add_network_example(asio_udp_server asio_udp_server.cpp)
add_network_example(asio_udp_client asio_udp_client.cpp)

# Linux-specific example using epoll and POSIX sockets.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(epoll_server epoll_server.cpp)
    target_link_libraries(epoll_server PRIVATE Threads::Threads)
    target_compile_options(epoll_server PRIVATE -Wall -Wextra -Wpedantic)

    add_executable(posix_server server.cpp)
    target_link_libraries(posix_server PRIVATE Threads::Threads)
    target_compile_options(posix_server PRIVATE -Wall -Wextra -Wpedantic)
endif()
