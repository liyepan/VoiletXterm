QT += core gui widgets testlib
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = vx_widget_test
TEMPLATE = app
INCLUDEPATH += ../src
SOURCES += test_widget.cpp \
    ../src/ui/Style.cpp \
    ../src/core/Screen.cpp \
    ../src/core/CharWidth.cpp \
    ../src/core/Palette.cpp \
    ../src/core/VtParser.cpp \
    ../src/term/TerminalTheme.cpp \
    ../src/term/TerminalWidget.cpp
HEADERS += ../src/core/Screen.h \
    ../src/core/VtParser.h \
    ../src/term/TerminalWidget.h \
    ../src/session/Transport.h

RESOURCES += ../../resources/fonts.qrc
