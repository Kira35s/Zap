QT += core gui network concurrent qml quick testlib
CONFIG += c++17 testcase
TARGET = zap_tests
TEMPLATE = app
LIBS += -lz

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/catalog.h ../src/downloader.h ../src/entrymodel.h ../src/guide.h \
           ../src/theme.h ../src/translator.h
SOURCES += zap_tests.cpp ../src/backend.cpp ../src/catalog.cpp ../src/downloader.cpp ../src/entrymodel.cpp \
           ../src/guide.cpp ../src/theme.cpp ../src/translator.cpp
