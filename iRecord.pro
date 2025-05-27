#QT -= gui
QT += websockets
QT += network
QT += gui
QT += sql
CONFIG += c++11 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
#DEFINES += QT_NO_DEBUG_OUTPUT

SOURCES += \
        ChatServer.cpp \
        FileDownloader.cpp \
        GPIOClass.cpp \
        GetInputEvent.cpp \
        I2CReadWrite.cpp \
        MAX31760.cpp \
        NetworkMng.cpp \
        SPI.cpp \
        SPIOLED.cpp \
        SocketClient.cpp \
        database.cpp \
        infoDisplay.cpp \
        linux_spi.cpp \
        main.cpp \
        mainwindows.cpp \
        max9850.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    ChatServer.h \
    FileDownloader.h \
    GPIOClass.h \
    GetInputEvent.h \
    I2CReadWrite.h \
    MAX31760.h \
    NetworkMng.h \
    SPI.h \
    SPIOLED.h \
    SocketClient.h \
    database.h \
    font16.h \
    infoDisplay.h \
    linux_spi.h \
    mainwindows.h \
    max9850.h

LIBS += -lboost_system -lboost_chrono -lboost_thread -ludev
