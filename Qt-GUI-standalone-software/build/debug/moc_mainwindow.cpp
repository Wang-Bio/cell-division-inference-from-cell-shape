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
    uint offsetsAndSizes[132];
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
    char stringdata25[9];
    char stringdata26[16];
    char stringdata27[9];
    char stringdata28[16];
    char stringdata29[14];
    char stringdata30[14];
    char stringdata31[12];
    char stringdata32[15];
    char stringdata33[20];
    char stringdata34[10];
    char stringdata35[13];
    char stringdata36[17];
    char stringdata37[13];
    char stringdata38[16];
    char stringdata39[20];
    char stringdata40[18];
    char stringdata41[18];
    char stringdata42[16];
    char stringdata43[19];
    char stringdata44[22];
    char stringdata45[29];
    char stringdata46[34];
    char stringdata47[39];
    char stringdata48[37];
    char stringdata49[35];
    char stringdata50[40];
    char stringdata51[44];
    char stringdata52[26];
    char stringdata53[44];
    char stringdata54[23];
    char stringdata55[21];
    char stringdata56[24];
    char stringdata57[29];
    char stringdata58[29];
    char stringdata59[13];
    char stringdata60[11];
    char stringdata61[14];
    char stringdata62[24];
    char stringdata63[26];
    char stringdata64[21];
    char stringdata65[11];
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
        QT_MOC_LITERAL(344, 8),  // "onImport"
        QT_MOC_LITERAL(353, 15),  // "onImportAllData"
        QT_MOC_LITERAL(369, 8),  // "onExport"
        QT_MOC_LITERAL(378, 15),  // "onExportAllData"
        QT_MOC_LITERAL(394, 13),  // "onDeleteImage"
        QT_MOC_LITERAL(408, 13),  // "onEditSetting"
        QT_MOC_LITERAL(422, 11),  // "onAddVertex"
        QT_MOC_LITERAL(434, 14),  // "onDeleteVertex"
        QT_MOC_LITERAL(449, 19),  // "onDeleteAllVertices"
        QT_MOC_LITERAL(469, 9),  // "onAddLine"
        QT_MOC_LITERAL(479, 12),  // "onDeleteLine"
        QT_MOC_LITERAL(492, 16),  // "onDeleteAllLines"
        QT_MOC_LITERAL(509, 12),  // "onAddPolygon"
        QT_MOC_LITERAL(522, 15),  // "onDeletePolygon"
        QT_MOC_LITERAL(538, 19),  // "onDeleteAllPolygons"
        QT_MOC_LITERAL(558, 17),  // "onSkeletonization"
        QT_MOC_LITERAL(576, 17),  // "onVertexDetection"
        QT_MOC_LITERAL(594, 15),  // "onLineDetection"
        QT_MOC_LITERAL(610, 18),  // "onPolygonDetection"
        QT_MOC_LITERAL(629, 21),  // "onDetectNeighborPairs"
        QT_MOC_LITERAL(651, 28),  // "onGeometryCalculationSetting"
        QT_MOC_LITERAL(680, 33),  // "onNeighborPairGeometryCalcula..."
        QT_MOC_LITERAL(714, 38),  // "onBatchNeighborPairGeometryCa..."
        QT_MOC_LITERAL(753, 36),  // "onBatchSingleCellGeometryCalc..."
        QT_MOC_LITERAL(790, 34),  // "onEstimateDivisionBySingleGeo..."
        QT_MOC_LITERAL(825, 39),  // "onBatchEstimateDivisionBySing..."
        QT_MOC_LITERAL(865, 43),  // "onBatchEstimateDivisionByDesi..."
        QT_MOC_LITERAL(909, 25),  // "onCompareWithRealDivision"
        QT_MOC_LITERAL(935, 43),  // "onPrecisionAndRecallCurveOver..."
        QT_MOC_LITERAL(979, 22),  // "onVertexDisplaySetting"
        QT_MOC_LITERAL(1002, 20),  // "onLineDisplaySetting"
        QT_MOC_LITERAL(1023, 23),  // "onPolygonDisplaySetting"
        QT_MOC_LITERAL(1047, 28),  // "onNeighborPairDisplaySetting"
        QT_MOC_LITERAL(1076, 28),  // "onDivisionPairDisplaySetting"
        QT_MOC_LITERAL(1105, 12),  // "onFindVertex"
        QT_MOC_LITERAL(1118, 10),  // "onFindLine"
        QT_MOC_LITERAL(1129, 13),  // "onFindPolygon"
        QT_MOC_LITERAL(1143, 23),  // "onGenerateRandomNetwork"
        QT_MOC_LITERAL(1167, 25),  // "onCompareBatchEstimations"
        QT_MOC_LITERAL(1193, 20),  // "onExportFeatureNames"
        QT_MOC_LITERAL(1214, 10)   // "onDebugAll"
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
    "onImport",
    "onImportAllData",
    "onExport",
    "onExportAllData",
    "onDeleteImage",
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
    "onLineDetection",
    "onPolygonDetection",
    "onDetectNeighborPairs",
    "onGeometryCalculationSetting",
    "onNeighborPairGeometryCalculation",
    "onBatchNeighborPairGeometryCalculation",
    "onBatchSingleCellGeometryCalculation",
    "onEstimateDivisionBySingleGeometry",
    "onBatchEstimateDivisionBySingleGeometry",
    "onBatchEstimateDivisionByDesignatedGeometry",
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
      55,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  344,    2, 0x08,    1 /* Private */,
       4,    1,  347,    2, 0x08,    3 /* Private */,
       6,    0,  350,    2, 0x08,    5 /* Private */,
       7,    0,  351,    2, 0x08,    6 /* Private */,
       8,    0,  352,    2, 0x08,    7 /* Private */,
       9,    1,  353,    2, 0x08,    8 /* Private */,
      12,    2,  356,    2, 0x08,   10 /* Private */,
      14,    1,  361,    2, 0x08,   13 /* Private */,
      17,    1,  364,    2, 0x08,   15 /* Private */,
      18,    1,  367,    2, 0x08,   17 /* Private */,
      21,    1,  370,    2, 0x08,   19 /* Private */,
      22,    0,  373,    2, 0x08,   21 /* Private */,
      23,    0,  374,    2, 0x08,   22 /* Private */,
      24,    0,  375,    2, 0x08,   23 /* Private */,
      25,    0,  376,    2, 0x08,   24 /* Private */,
      26,    0,  377,    2, 0x08,   25 /* Private */,
      27,    0,  378,    2, 0x08,   26 /* Private */,
      28,    0,  379,    2, 0x08,   27 /* Private */,
      29,    0,  380,    2, 0x08,   28 /* Private */,
      30,    0,  381,    2, 0x08,   29 /* Private */,
      31,    0,  382,    2, 0x08,   30 /* Private */,
      32,    0,  383,    2, 0x08,   31 /* Private */,
      33,    0,  384,    2, 0x08,   32 /* Private */,
      34,    0,  385,    2, 0x08,   33 /* Private */,
      35,    0,  386,    2, 0x08,   34 /* Private */,
      36,    0,  387,    2, 0x08,   35 /* Private */,
      37,    0,  388,    2, 0x08,   36 /* Private */,
      38,    0,  389,    2, 0x08,   37 /* Private */,
      39,    0,  390,    2, 0x08,   38 /* Private */,
      40,    0,  391,    2, 0x08,   39 /* Private */,
      41,    0,  392,    2, 0x08,   40 /* Private */,
      42,    0,  393,    2, 0x08,   41 /* Private */,
      43,    0,  394,    2, 0x08,   42 /* Private */,
      44,    0,  395,    2, 0x08,   43 /* Private */,
      45,    0,  396,    2, 0x08,   44 /* Private */,
      46,    0,  397,    2, 0x08,   45 /* Private */,
      47,    0,  398,    2, 0x08,   46 /* Private */,
      48,    0,  399,    2, 0x08,   47 /* Private */,
      49,    0,  400,    2, 0x08,   48 /* Private */,
      50,    0,  401,    2, 0x08,   49 /* Private */,
      51,    0,  402,    2, 0x08,   50 /* Private */,
      52,    0,  403,    2, 0x08,   51 /* Private */,
      53,    0,  404,    2, 0x08,   52 /* Private */,
      54,    0,  405,    2, 0x08,   53 /* Private */,
      55,    0,  406,    2, 0x08,   54 /* Private */,
      56,    0,  407,    2, 0x08,   55 /* Private */,
      57,    0,  408,    2, 0x08,   56 /* Private */,
      58,    0,  409,    2, 0x08,   57 /* Private */,
      59,    0,  410,    2, 0x08,   58 /* Private */,
      60,    0,  411,    2, 0x08,   59 /* Private */,
      61,    0,  412,    2, 0x08,   60 /* Private */,
      62,    0,  413,    2, 0x08,   61 /* Private */,
      63,    0,  414,    2, 0x08,   62 /* Private */,
      64,    0,  415,    2, 0x08,   63 /* Private */,
      65,    0,  416,    2, 0x08,   64 /* Private */,

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
        // method 'onEstimateDivisionBySingleGeometry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onBatchEstimateDivisionBySingleGeometry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onBatchEstimateDivisionByDesignatedGeometry'
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
        case 14: _t->onImport(); break;
        case 15: _t->onImportAllData(); break;
        case 16: _t->onExport(); break;
        case 17: _t->onExportAllData(); break;
        case 18: _t->onDeleteImage(); break;
        case 19: _t->onEditSetting(); break;
        case 20: _t->onAddVertex(); break;
        case 21: _t->onDeleteVertex(); break;
        case 22: _t->onDeleteAllVertices(); break;
        case 23: _t->onAddLine(); break;
        case 24: _t->onDeleteLine(); break;
        case 25: _t->onDeleteAllLines(); break;
        case 26: _t->onAddPolygon(); break;
        case 27: _t->onDeletePolygon(); break;
        case 28: _t->onDeleteAllPolygons(); break;
        case 29: _t->onSkeletonization(); break;
        case 30: _t->onVertexDetection(); break;
        case 31: _t->onLineDetection(); break;
        case 32: _t->onPolygonDetection(); break;
        case 33: _t->onDetectNeighborPairs(); break;
        case 34: _t->onGeometryCalculationSetting(); break;
        case 35: _t->onNeighborPairGeometryCalculation(); break;
        case 36: _t->onBatchNeighborPairGeometryCalculation(); break;
        case 37: _t->onBatchSingleCellGeometryCalculation(); break;
        case 38: _t->onEstimateDivisionBySingleGeometry(); break;
        case 39: _t->onBatchEstimateDivisionBySingleGeometry(); break;
        case 40: _t->onBatchEstimateDivisionByDesignatedGeometry(); break;
        case 41: _t->onCompareWithRealDivision(); break;
        case 42: _t->onPrecisionAndRecallCurveOverSingleGeometry(); break;
        case 43: _t->onVertexDisplaySetting(); break;
        case 44: _t->onLineDisplaySetting(); break;
        case 45: _t->onPolygonDisplaySetting(); break;
        case 46: _t->onNeighborPairDisplaySetting(); break;
        case 47: _t->onDivisionPairDisplaySetting(); break;
        case 48: _t->onFindVertex(); break;
        case 49: _t->onFindLine(); break;
        case 50: _t->onFindPolygon(); break;
        case 51: _t->onGenerateRandomNetwork(); break;
        case 52: _t->onCompareBatchEstimations(); break;
        case 53: _t->onExportFeatureNames(); break;
        case 54: _t->onDebugAll(); break;
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
        if (_id < 55)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 55;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 55)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 55;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
