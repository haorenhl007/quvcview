#-------------------------------------------------
#
# Project created by QtCreator 2015-12-20T13:57:22
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = quvcview
TEMPLATE = app


SOURCES += main.cpp\
        quvcview.cpp \
    v4l2.c \
    yuv422_rgb.c

HEADERS  += quvcview.h \
    v4l2.h \
    yuv422_rgb.h

FORMS    += quvcview.ui
