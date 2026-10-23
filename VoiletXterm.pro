QT += core gui widgets network serialport

TARGET = voiletxterm
TEMPLATE = app
CONFIG += c++17 warn_on
CONFIG -= app_bundle

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/core/CharWidth.cpp \
    src/core/Palette.cpp \
    src/core/Screen.cpp \
    src/core/VtParser.cpp \
    src/term/TerminalTheme.cpp \
    src/term/TerminalWidget.cpp \
    src/session/PtyProcess.cpp \
    src/session/SerialTransport.cpp \
    src/session/SessionFactory.cpp \
    src/session/SessionSpec.cpp \
    src/session/SshTransport.cpp \
    src/session/TelnetTransport.cpp \
    src/ui/MainWindow.cpp \
    src/ui/SessionDialog.cpp \
    src/ui/SessionStore.cpp \
    src/ui/Style.cpp

HEADERS += \
    src/core/Cell.h \
    src/core/CharWidth.h \
    src/core/Palette.h \
    src/core/Screen.h \
    src/core/VtParser.h \
    src/term/TerminalTheme.h \
    src/term/TerminalWidget.h \
    src/session/PtyProcess.h \
    src/session/SerialTransport.h \
    src/session/SessionFactory.h \
    src/session/SessionSpec.h \
    src/session/SshTransport.h \
    src/session/TelnetTransport.h \
    src/session/Transport.h \
    src/ui/MainWindow.h \
    src/ui/SessionDialog.h \
    src/ui/SessionStore.h \
    src/ui/Style.h

unix: LIBS += -lutil
