QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    canvasmanager.cpp \
    debugmanager.cpp \
    divisionestimator.cpp \
    imageanalysis.cpp \
    interactivegraphicsview.cpp \
    lineitem.cpp \
    main.cpp \
    mainwindow.cpp \
    batchdivisionestimator.cpp \
    neighborgeometrycalculator.cpp \
    neighborpair.cpp \
    geometryio.cpp \
    polygonitem.cpp \
    vertexitem.cpp \
    precisionrecallsweepworker.cpp

HEADERS += \
    canvasmanager.h \
    debugmanager.h \
    divisionestimator.h \
    imageanalysis.h \
    interactivegraphicsview.h \
    lineitem.h \
    mainwindow.h \
    batchdivisionestimator.h \
    neighborgeometrycalculator.h \
    neighborpair.h \
    geometryio.h \
    polygonitem.h \
    vertexitem.h \
    precisionrecallsweepworker.h

FORMS += \
    mainwindow.ui

INCLUDEPATH += C:\\tool\opencv\\release\\install\\include

LIBS += C:\\tool\\opencv\\release\\lib\\libopencv_*.a

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
