QT += core gui widgets network

CONFIG += c++11
TEMPLATE = app
TARGET = imx6ull_ov5640_qt_demo

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    cameraworker.cpp \
    mockvisionpipeline.cpp \
    realvisionpipeline.cpp \
    simplefacedetectorbackend.cpp \
    previewserver.cpp \
    offlinevisiontest.cpp

HEADERS += \
    mainwindow.h \
    cameraworker.h \
    visionpipeline.h \
    facedetectorbackend.h \
    mockvisionpipeline.h \
    realvisionpipeline.h \
    simplefacedetectorbackend.h \
    previewserver.h \
    offlinevisiontest.h
