#-------------------------------------------------
#
# Project created by QtCreator 2013-11-07T00:02:33
#
#-------------------------------------------------

QT        += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = MinesweeperPlayer
TEMPLATE = app


SOURCES  += main.cpp\
            mainwindow.cpp \
            minesweeperplayer.cpp

HEADERS  += mainwindow.h \
            minesweeperplayer.h

FORMS    += mainwindow.ui

QMAKE_CXXFLAGS  += -std=c++11

QMAKE_CXXFLAGS_WARN_ON    += -Wno-parentheses

QMAKE_LFLAGS += -Wl,--stack,4194304

