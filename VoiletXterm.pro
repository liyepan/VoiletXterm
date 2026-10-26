QT += core gui widgets network serialport

VERSION = 0.1.0
TEMPLATE = app

# Linux/Windows keep the lowercase command name; macOS needs a proper bundle.
macx {
    TARGET = VoiletXterm
    ICON = packaging/voiletxterm.icns
    QMAKE_TARGET_BUNDLE_PREFIX = com.liyepan
    QMAKE_INFO_PLIST = packaging/macos/Info.plist
} else {
    TARGET = voiletxterm
}

win32: RC_ICONS = packaging/voiletxterm.ico

QMAKE_TARGET_COMPANY = liyepan
QMAKE_TARGET_PRODUCT = VoiletXterm
QMAKE_TARGET_DESCRIPTION = "SSH / Telnet / serial terminal"
QMAKE_TARGET_COPYRIGHT = MIT

CONFIG += c++17 warn_on

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += src

RESOURCES += resources/fonts.qrc

SOURCES += \
    src/main.cpp \
    src/core/CharWidth.cpp \
    src/core/Palette.cpp \
    src/core/Screen.cpp \
    src/core/VtParser.cpp \
    src/term/BundledFont.cpp \
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
    src/ui/Style.cpp \
    src/ui/ThemeManager.cpp

HEADERS += \
    src/core/Cell.h \
    src/core/CharWidth.h \
    src/core/Palette.h \
    src/core/Screen.h \
    src/core/VtParser.h \
    src/term/BundledFont.h \
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
    src/ui/Style.h \
    src/ui/ThemeManager.h

unix: LIBS += -lutil

# Linux packaging: make install INSTALL_ROOT=<dir> stages /usr/bin + desktop entry.
unix:!macx {
    target.path = /usr/bin
    desktop.files = packaging/voiletxterm.desktop
    desktop.path = /usr/share/applications
    icon.files = packaging/voiletxterm.png
    icon.path = /usr/share/icons/hicolor/256x256/apps
    docfile.files = resources/fonts/OFL.txt
    docfile.path = /usr/share/doc/voiletxterm
    INSTALLS += target desktop icon docfile
}
