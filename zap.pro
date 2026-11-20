QT += core gui network concurrent qml quick quickcontrols2 multimedia

CONFIG += c++17 release
TARGET = zap
TEMPLATE = app

# XMLTV guides usually come gzipped; zlib inflates them while they're parsed.
LIBS += -lz

HEADERS += \
    src/backend.h \
    src/catalog.h \
    src/downloader.h \
    src/entrymodel.h \
    src/guide.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/catalog.cpp \
    src/downloader.cpp \
    src/entrymodel.cpp \
    src/guide.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
