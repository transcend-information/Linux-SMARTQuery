#-------------------------------------------------
#
# Project created by QtCreator 2019-11-27T17:46:57
#
#-------------------------------------------------

QT       += core gui
QT += xml
QT += core
QT += widgets
TARGET = PCIe_Info_Tool
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    ata_io.c \
    nvme_util.cpp \
    showsmartdialog.cpp

HEADERS  += mainwindow.h \
    ata_io.h \
    nvme_util.h \
    linux_nvme_ioctl.h \
    int64.h \
    showsmartdialog.h

FORMS    += mainwindow.ui \
    showsmartdialog.ui

RESOURCES += \
    res.qrc
