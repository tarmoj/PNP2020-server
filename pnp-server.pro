#-------------------------------------------------
#
# Project created by QtCreator 2015-02-04T15:01:47
#
#-------------------------------------------------

QT       += core gui websockets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = PNP-server
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \ # since copied from echoservers
    wsserver.cpp \
    qosc/qoscclient.cpp \
    qosc/qoscserver.cpp \
    qosc/qosctypes.cpp \

  

HEADERS  += mainwindow.h \
    wsserver.h \
    qosc/qoscclient.h \
    qosc/qoscserver.h \
    qosc/qosctypes.h \

  
FORMS    += mainwindow.ui

<<<<<<< Updated upstream
RESOURCES += \
    resource.qrc
=======
macx {
    installer.path = $$PWD
    installer.commands = $$[QT_INSTALL_PREFIX]/bin/macdeployqt $$OUT_PWD/$$DESTDIR/$${TARGET}.app -qmldir=$$PWD -dmg # deployment
    INSTALLS += installer

}
>>>>>>> Stashed changes
