/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../source-code/mainwindow.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_MainWindow_t {
    uint offsetsAndSizes[138];
    char stringdata0[11];
    char stringdata1[14];
    char stringdata2[1];
    char stringdata3[12];
    char stringdata4[23];
    char stringdata5[9];
    char stringdata6[23];
    char stringdata7[21];
    char stringdata8[24];
    char stringdata9[17];
    char stringdata10[12];
    char stringdata11[2];
    char stringdata12[14];
    char stringdata13[4];
    char stringdata14[15];
    char stringdata15[10];
    char stringdata16[2];
    char stringdata17[22];
    char stringdata18[18];
    char stringdata19[13];
    char stringdata20[5];
    char stringdata21[25];
    char stringdata22[15];
    char stringdata23[15];
    char stringdata24[17];
    char stringdata25[18];
    char stringdata26[9];
    char stringdata27[16];
    char stringdata28[9];
    char stringdata29[16];
    char stringdata30[14];
    char stringdata31[11];
    char stringdata32[14];
    char stringdata33[12];
    char stringdata34[15];
    char stringdata35[20];
    char stringdata36[10];
    char stringdata37[13];
    char stringdata38[17];
    char stringdata39[13];
    char stringdata40[16];
    char stringdata41[20];
    char stringdata42[18];
    char stringdata43[18];
    char stringdata44[29];
    char stringdata45[16];
    char stringdata46[19];
    char stringdata47[22];
    char stringdata48[29];
    char stringdata49[34];
    char stringdata50[39];
    char stringdata51[37];
    char stringdata52[42];
    char stringdata53[35];
    char stringdata54[40];
    char stringdata55[26];
    char stringdata56[44];
    char stringdata57[23];
    char stringdata58[21];
    char stringdata59[24];
    char stringdata60[29];
    char stringdata61[29];
    char stringdata62[13];
    char stringdata63[11];
    char stringdata64[14];
    char stringdata65[24];
    char stringdata66[26];
    char stringdata67[21];
    char stringdata68[11];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_MainWindow_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
        QT_MOC_LITERAL(0, 10),  // "MainWindow"
        QT_MOC_LITERAL(11, 13),  // "onZoomChanged"
        QT_MOC_LITERAL(25, 0),  // ""
        QT_MOC_LITERAL(26, 11),  // "zoomPercent"
        QT_MOC_LITERAL(38, 22),  // "onMousePositionChanged"
        QT_MOC_LITERAL(61, 8),  // "scenePos"
        QT_MOC_LITERAL(70, 22),  // "updateVertexCountLabel"
        QT_MOC_LITERAL(93, 20),  // "updateLineCountLabel"
        QT_MOC_LITERAL(114, 23),  // "updatePolygonCountLabel"
        QT_MOC_LITERAL(138, 16),  // "onVertexSelected"
        QT_MOC_LITERAL(155, 11),  // "VertexItem*"
        QT_MOC_LITERAL(167, 1),  // "v"
        QT_MOC_LITERAL(169, 13),  // "onVertexMoved"
        QT_MOC_LITERAL(183, 3),  // "pos"
        QT_MOC_LITERAL(187, 14),  // "onLineSelected"
        QT_MOC_LITERAL(202, 9),  // "LineItem*"
        QT_MOC_LITERAL(212, 1),  // "L"
        QT_MOC_LITERAL(214, 21),  // "onLineGeometryChanged"
        QT_MOC_LITERAL(236, 17),  // "onPolygonSelected"
        QT_MOC_LITERAL(254, 12),  // "PolygonItem*"
        QT_MOC_LITERAL(267, 4),  // "poly"
        QT_MOC_LITERAL(272, 24),  // "onPolygonGeometryChanged"
        QT_MOC_LITERAL(297, 14),  // "onCreateCanvas"
        QT_MOC_LITERAL(312, 14),  // "onOpenRawImage"
        QT_MOC_LITERAL(327, 16),  // "onOpenBackground"
        QT_MOC_LITERAL(344, 17),  // "onShowSourceImage"
        QT_MOC_LITERAL(362, 8),  // "onImport"
        QT_MOC_LITERAL(371, 15),  // "onImportAllData"
        QT_MOC_LITERAL(387, 8),  // "onExport"
        QT_MOC_LITERAL(396, 15),  // "onExportAllData"
        QT_MOC_LITERAL(412, 13),  // "onDeleteImage"
        QT_MOC_LITERAL(426, 10),  // "onResetAll"
        QT_MOC_LITERAL(437, 13),  // "onEditSetting"
        QT_MOC_LITERAL(451, 11),  // "onAddVertex"
        QT_MOC_LITERAL(463, 14),  // "onDeleteVertex"
        QT_MOC_LITERAL(478, 19),  // "onDeleteAllVertices"
        QT_MOC_LITERAL(498, 9),  // "onAddLine"
        QT_MOC_LITERAL(508, 12),  // "onDeleteLine"
        QT_MOC_LITERAL(521, 16),  // "onDeleteAllLines"
        QT_MOC_LITERAL(538, 12),  // "onAddPolygon"
        QT_MOC_LITERAL(551, 15),  // "onDeletePolygon"
        QT_MOC_LITERAL(567, 19),  // "onDeleteAllPolygons"
        QT_MOC_LITERAL(587, 17),  // "onSkeletonization"
        QT_MOC_LITERAL(605, 17),  // "onVertexDetection"
        QT_MOC_LITERAL(623, 28),  // "onDevelopmentVertexDetection"
        QT_MOC_LITERAL(652, 15),  // "onLineDetection"
        QT_MOC_LITERAL(668, 18),  // "onPolygonDetection"
        QT_MOC_LITERAL(687, 21),  // "onDetectNeighborPairs"
        QT_MOC_LITERAL(709, 28),  // "onGeometryCalculationSetting"
        QT_MOC_LITERAL(738, 33),  // "onNeighborPairGeometryCalcula..."
        QT_MOC_LITERAL(772, 38),  // "onBatchNeighborPairGeometryCa..."
        QT_MOC_LITERAL(811, 36),  // "onBatchSingleCellGeometryCalc..."
        QT_MOC_LITERAL(848, 41),  // "onMixtureModelingForSingleGeo..."
        QT_MOC_LITERAL(890, 34),  // "onEstimateDivisionBySingleGeo..."
        QT_MOC_LITERAL(925, 39),  // "onBatchEstimateDivisionBySing..."
        QT_MOC_LITERAL(965, 25),  // "onCompareWithRealDivision"
        QT_MOC_LITERAL(991, 43),  // "onPrecisionAndRecallCurveOver..."
        QT_MOC_LITERAL(1035, 22),  // "onVertexDisplaySetting"
        QT_MOC_LITERAL(1058, 20),  // "onLineDisplaySetting"
        QT_MOC_LITERAL(1079, 23),  // "onPolygonDisplaySetting"
        QT_MOC_LITERAL(1103, 28),  // "onNeighborPairDisplaySetting"
        QT_MOC_LITERAL(1132, 28),  // "onDivisionPairDisplaySetting"
        QT_MOC_LITERAL(1161, 12),  // "onFindVertex"
        QT_MOC_LITERAL(1174, 10),  // "onFindLine"
        QT_MOC_LITERAL(1185, 13),  // "onFindPolygon"
        QT_MOC_LITERAL(1199, 23),  // "onGenerateRandomNetwork"
        QT_MOC_LITERAL(1223, 25),  // "onCompareBatchEstimations"
        QT_MOC_LITERAL(1249, 20),  // "onExportFeatureNames"
        QT_MOC_LITERAL(1270, 10)   // "onDebugAll"
    },
    "MainWindow",
    "onZoomChanged",
    "",
    "zoomPercent",
    "onMousePositionChanged",
    "scenePos",
    "updateVertexCountLabel",
    "updateLineCountLabel",
    "updatePolygonCountLabel",
    "onVertexSelected",
    "VertexItem*",
    "v",
    "onVertexMoved",
    "pos",
    "onLineSelected",
    "LineItem*",
    "L",
    "onLineGeometryChanged",
    "onPolygonSelected",
    "PolygonItem*",
    "poly",
    "onPolygonGeometryChanged",
    "onCreateCanvas",
    "onOpenRawImage",
    "onOpenBackground",
    "onShowSourceImage",
    "onImport",
    "onImportAllData",
    "onExport",
    "onExportAllData",
    "onDeleteImage",
    "onResetAll",
    "onEditSetting",
    "onAddVertex",
    "onDeleteVertex",
    "onDeleteAllVertices",
    "onAddLine",
    "onDeleteLine",
    "onDeleteAllLines",
    "onAddPolygon",
    "onDeletePolygon",
    "onDeleteAllPolygons",
    "onSkeletonization",
    "onVertexDetection",
    "onDevelopmentVertexDetection",
    "onLineDetection",
    "onPolygonDetection",
    "onDetectNeighborPairs",
    "onGeometryCalculationSetting",
    "onNeighborPairGeometryCalculation",
    "onBatchNeighborPairGeometryCalculation",
    "onBatchSingleCellGeometryCalculation",
    "onMixtureModelingForSingleGeometryFeature",
    "onEstimateDivisionBySingleGeometry",
    "onBatchEstimateDivisionBySingleGeometry",
    "onCompareWithRealDivision",
    "onPrecisionAndRecallCurveOverSingleGeometry",
    "onVertexDisplaySetting",
    "onLineDisplaySetting",
    "onPolygonDisplaySetting",
    "onNeighborPairDisplaySetting",
    "onDivisionPairDisplaySetting",
    "onFindVertex",
    "onFindLine",
    "onFindPolygon",
    "onGenerateRandomNetwork",
    "onCompareBatchEstimations",
    "onExportFeatureNames",
    "onDebugAll"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_MainWindow[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      58,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  362,    2, 0x08,    1 /* Private */,
       4,    1,  365,    2, 0x08,    3 /* Private */,
       6,    0,  368,    2, 0x08,    5 /* Private */,
       7,    0,  369,    2, 0x08,    6 /* Private */,
       8,    0,  370,    2, 0x08,    7 /* Private */,
       9,    1,  371,    2, 0x08,    8 /* Private */,
      12,    2,  374,    2, 0x08,   10 /* Private */,
      14,    1,  379,    2, 0x08,   13 /* Private */,
      17,    1,  382,    2, 0x08,   15 /* Private */,
      18,    1,  385,    2, 0x08,   17 /* Private */,
      21,    1,  388,    2, 0x08,   19 /* Private */,
      22,    0,  391,    2, 0x08,   21 /* Private */,
      23,    0,  392,    2, 0x08,   22 /* Private */,
      24,    0,  393,    2, 0x08,   23 /* Private */,
      25,    0,  394,    2, 0x08,   24 /* Private */,
      26,    0,  395,    2, 0x08,   25 /* Private */,
      27,    0,  396,    2, 0x08,   26 /* Private */,
      28,    0,  397,    2, 0x08,   27 /* Private */,
      29,    0,  398,    2, 0x08,   28 /* Private */,
      30,    0,  399,    2, 0x08,   29 /* Private */,
      31,    0,  400,    2, 0x08,   30 /* Private */,
      32,    0,  401,    2, 0x08,   31 /* Private */,
      33,    0,  402,    2, 0x08,   32 /* Private */,
      34,    0,  403,    2, 0x08,   33 /* Private */,
      35,    0,  404,    2, 0x08,   34 /* Private */,
      36,    0,  405,    2, 0x08,   35 /* Private */,
      37,    0,  406,    2, 0x08,   36 /* Private */,
      38,    0,  407,    2, 0x08,   37 /* Private */,
      39,    0,  408,    2, 0x08,   38 /* Private */,
      40,    0,  409,    2, 0x08,   39 /* Private */,
      41,    0,  410,    2, 0x08,   40 /* Private */,
      42,    0,  411,    2, 0x08,   41 /* Private */,
      43,    0,  412,    2, 0x08,   42 /* Private */,
      44,    0,  413,    2, 0x08,   43 /* Private */,
      45,    0,  414,    2, 0x08,   44 /* Private */,
      46,    0,  415,    2, 0x08,   45 /* Private */,
      47,    0,  416,    2, 0x08,   46 /* Private */,
      48,    0,  417,    2, 0x08,   47 /* Private */,
      49,    0,  418,    2, 0x08,   48 /* Private */,
      50,    0,  419,    2, 0x08,   49 /* Private */,
      51,    0,  420,    2, 0x08,   50 /* Private */,
      52,    0,  421,    2, 0x08,   51 /* Private */,
      53,    0,  422,    2, 0x08,   52 /* Private */,
      54,    0,  423,    2, 0x08,   53 /* Private */,
      55,    0,  424,    2, 0x08,   54 /* Private */,
      56,    0,  425,    2, 0x08,   55 /* Private */,
      57,    0,  426,    2, 0x08,   56 /* Private */,
      58,    0,  427,    2, 0x08,   57 /* Private */,
      59,    0,  428,    2, 0x08,   58 /* Private */,
      60,    0,  429,    2, 0x08,   59 /* Private */,
      61,    0,  430,    2, 0x08,   60 /* Private */,
      62,    0,  431,    2, 0x08,   61 /* Private */,
      63,    0,  432,    2, 0x08,   62 /* Private */,
      64,    0,  433,    2, 0x08,   63 /* Private */,
      65,    0,  434,    2, 0x08,   64 /* Private */,
      66,    0,  435,    2, 0x08,   65 /* Private */,
      67,    0,  436,    2, 0x08,   66 /* Private */,
      68,    0,  437,    2, 0x08,   67 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Double,    3,
    QMetaType::Void, QMetaType::QPointF,    5,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 10,   11,
    QMetaType::Void, 0x80000000 | 10, QMetaType::QPointF,   11,   13,
    QMetaType::Void, 0x80000000 | 15,   16,
    QMetaType::Void, 0x80000000 | 15,   16,
    QMetaType::Void, 0x80000000 | 19,   20,
    QMetaType::Void, 0x80000000 | 19,   20,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.offsetsAndSizes,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_MainWindow_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'onZoomChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'onMousePositionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPointF &, std::false_type>,
        // method 'updateVertexCountLabel'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateLineCountLabel'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updatePolygonCountLabel'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onVertexSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<VertexItem *, std::false_type>,
        // method 'onVertexMoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<VertexItem *, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPointF &, std::false_type>,
        // method 'onLineSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<LineItem *, std::false_type>,
        // method 'onLineGeometryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<LineItem *, std::false_type>,
        // method 'onPolygonSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<PolygonItem *, std::false_type>,
        // method 'onPolygonGeometryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<PolygonItem *, std::false_type>,
        // method 'onCreateCanvas'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onOpenRawImage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onOpenBackground'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onShowSourceImage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onImport'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onImportAllData'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onExport'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onExportAllData'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDeleteImage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onResetAll'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onEditSetting'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onAddVertex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDeleteVertex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDeleteAllVertices'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onAddLine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDeleteLine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDeleteAllLines'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onAddPolygon'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDeletePolygon'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDeleteAllPolygons'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSkeletonization'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onVertexDetection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDevelopmentVertexDetection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLineDetection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onPolygonDetection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDetectNeighborPairs'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onGeometryCalculationSetting'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNeighborPairGeometryCalculation'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onBatchNeighborPairGeometryCalculation'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onBatchSingleCellGeometryCalculation'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onMixtureModelingForSingleGeometryFeature'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onEstimateDivisionBySingleGeometry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onBatchEstimateDivisionBySingleGeometry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCompareWithRealDivision'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onPrecisionAndRecallCurveOverSingleGeometry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onVertexDisplaySetting'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLineDisplaySetting'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onPolygonDisplaySetting'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNeighborPairDisplaySetting'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDivisionPairDisplaySetting'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onFindVertex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onFindLine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onFindPolygon'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onGenerateRandomNetwork'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCompareBatchEstimations'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onExportFeatureNames'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDebugAll'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onZoomChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 1: _t->onMousePositionChanged((*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[1]))); break;
        case 2: _t->updateVertexCountLabel(); break;
        case 3: _t->updateLineCountLabel(); break;
        case 4: _t->updatePolygonCountLabel(); break;
        case 5: _t->onVertexSelected((*reinterpret_cast< std::add_pointer_t<VertexItem*>>(_a[1]))); break;
        case 6: _t->onVertexMoved((*reinterpret_cast< std::add_pointer_t<VertexItem*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 7: _t->onLineSelected((*reinterpret_cast< std::add_pointer_t<LineItem*>>(_a[1]))); break;
        case 8: _t->onLineGeometryChanged((*reinterpret_cast< std::add_pointer_t<LineItem*>>(_a[1]))); break;
        case 9: _t->onPolygonSelected((*reinterpret_cast< std::add_pointer_t<PolygonItem*>>(_a[1]))); break;
        case 10: _t->onPolygonGeometryChanged((*reinterpret_cast< std::add_pointer_t<PolygonItem*>>(_a[1]))); break;
        case 11: _t->onCreateCanvas(); break;
        case 12: _t->onOpenRawImage(); break;
        case 13: _t->onOpenBackground(); break;
        case 14: _t->onShowSourceImage(); break;
        case 15: _t->onImport(); break;
        case 16: _t->onImportAllData(); break;
        case 17: _t->onExport(); break;
        case 18: _t->onExportAllData(); break;
        case 19: _t->onDeleteImage(); break;
        case 20: _t->onResetAll(); break;
        case 21: _t->onEditSetting(); break;
        case 22: _t->onAddVertex(); break;
        case 23: _t->onDeleteVertex(); break;
        case 24: _t->onDeleteAllVertices(); break;
        case 25: _t->onAddLine(); break;
        case 26: _t->onDeleteLine(); break;
        case 27: _t->onDeleteAllLines(); break;
        case 28: _t->onAddPolygon(); break;
        case 29: _t->onDeletePolygon(); break;
        case 30: _t->onDeleteAllPolygons(); break;
        case 31: _t->onSkeletonization(); break;
        case 32: _t->onVertexDetection(); break;
        case 33: _t->onDevelopmentVertexDetection(); break;
        case 34: _t->onLineDetection(); break;
        case 35: _t->onPolygonDetection(); break;
        case 36: _t->onDetectNeighborPairs(); break;
        case 37: _t->onGeometryCalculationSetting(); break;
        case 38: _t->onNeighborPairGeometryCalculation(); break;
        case 39: _t->onBatchNeighborPairGeometryCalculation(); break;
        case 40: _t->onBatchSingleCellGeometryCalculation(); break;
        case 41: _t->onMixtureModelingForSingleGeometryFeature(); break;
        case 42: _t->onEstimateDivisionBySingleGeometry(); break;
        case 43: _t->onBatchEstimateDivisionBySingleGeometry(); break;
        case 44: _t->onCompareWithRealDivision(); break;
        case 45: _t->onPrecisionAndRecallCurveOverSingleGeometry(); break;
        case 46: _t->onVertexDisplaySetting(); break;
        case 47: _t->onLineDisplaySetting(); break;
        case 48: _t->onPolygonDisplaySetting(); break;
        case 49: _t->onNeighborPairDisplaySetting(); break;
        case 50: _t->onDivisionPairDisplaySetting(); break;
        case 51: _t->onFindVertex(); break;
        case 52: _t->onFindLine(); break;
        case 53: _t->onFindPolygon(); break;
        case 54: _t->onGenerateRandomNetwork(); break;
        case 55: _t->onCompareBatchEstimations(); break;
        case 56: _t->onExportFeatureNames(); break;
        case 57: _t->onDebugAll(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 5:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< VertexItem* >(); break;
            }
            break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< VertexItem* >(); break;
            }
            break;
        case 7:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< LineItem* >(); break;
            }
            break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< LineItem* >(); break;
            }
            break;
        case 9:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< PolygonItem* >(); break;
            }
            break;
        case 10:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< PolygonItem* >(); break;
            }
            break;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 58)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 58;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 58)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 58;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
