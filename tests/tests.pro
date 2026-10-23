QT = core
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = vx_tests
TEMPLATE = app
INCLUDEPATH += ../src
SOURCES += test_core.cpp \
    ../src/core/Screen.cpp \
    ../src/core/CharWidth.cpp \
    ../src/core/Palette.cpp \
    ../src/core/VtParser.cpp
HEADERS += ../src/core/Screen.h \
    ../src/core/VtParser.h
