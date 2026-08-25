QT       += core gui

TEMPLATE = app
TARGET = "Cell Division Inference"

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Boost.Graph
BOOST_ROOT = C:/tool/boost_1_92_0
BOOST_INCLUDE_DIR = $$BOOST_ROOT

# Prefer bundled Boost if present
exists($$PWD/third_party/boost/boost/graph/adjacency_list.hpp) {
    BOOST_INCLUDE_DIR = $$PWD/third_party/boost
}

INCLUDEPATH += $$quote($$BOOST_INCLUDE_DIR)

message("Boost root: $$BOOST_INCLUDE_DIR")

!exists($$BOOST_INCLUDE_DIR/boost/graph/adjacency_list.hpp) {
    error("Boost.Graph headers were not found at $$BOOST_INCLUDE_DIR")
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    canvasmanager.cpp \
    debugmanager.cpp \
    networkdebugger.cpp \
    divisionestimator.cpp \
    weightedmatching.cpp \
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
    precisionrecallsweepworker.cpp \
    singlefeaturemixtureanalysis.cpp \
    overlapindexanalysis.cpp

HEADERS += \
    canvasmanager.h \
    debugmanager.h \
    networkdebugger.h \
    divisionestimator.h \
    weightedmatching.h \
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
    precisionrecallsweepworker.h \
    singlefeaturemixtureanalysis.h \
    overlapindexanalysis.h

FORMS += \
    mainwindow.ui

INCLUDEPATH += C:\\tool\opencv\\release\\install\\include

LIBS += C:\\tool\\opencv\\release\\lib\\libopencv_*.a


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
