cmake_minimum_required(VERSION 3.10)
project(rtpserver_h265)

set(CMAKE_C_STANDARD 99)

add_executable(rtpserver_h265 main.cpp rtpenc.cpp rtpenc.h utils.cpp utils.h avc.cpp avc.h network.cpp network.h)
