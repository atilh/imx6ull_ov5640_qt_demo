cmake_minimum_required(VERSION 4.1)
project(imx6ull_ov5640_qt_demo)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(imx6ull_ov5640_qt_demo
        cameraworker.cpp
        cameraworker.h
        facedetectorbackend.h
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mjpegserver.h
        mockvisionpipeline.cpp
        mockvisionpipeline.h
        offlinevisiontest.cpp
        offlinevisiontest.h
        previewserver.cpp
        previewserver.h
        realvisionpipeline.cpp
        realvisionpipeline.h
        simplefacedetectorbackend.cpp
        simplefacedetectorbackend.h
        visionpipeline.h)
