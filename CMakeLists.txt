cmake_minimum_required(VERSION 2.8)
project("MyFS file system")

add_definitions("-std=c++1y -Wall -pedantic")
add_executable(fs src/main.cpp src/fs.cpp)

