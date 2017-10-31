#-------------------------------------------------
#
# Project created by QtCreator 2017-10-16T15:11:25
#
#-------------------------------------------------

QT       += core gui
QT       +=network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = imdbFetch
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui
CONFIG   += static

RESOURCES += \
    imdb_rsrc.qrc

