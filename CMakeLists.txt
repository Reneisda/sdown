cmake_minimum_required(VERSION 3.26)
project(sdown C)

set(CMAKE_C_STANDARD 17)

add_executable(sdown src/main.c
        src/httpcon.c
        src/httpcon.h
        src/httpscon.c
        src/httpscon.h)

set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64/bin")

target_link_libraries(sdown ws2_32 secur32)
