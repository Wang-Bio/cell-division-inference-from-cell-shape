/****************************************************************************
** Meta object code from reading C++ file 'precisionrecallsweepworker.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../source-code/precisionrecallsweepworker.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'precisionrecallsweepworker.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_PrecisionRecallSweepWorker_t {
    uint offsetsAndSizes[34];
    char stringdata0[27];
    char stringdata1[9];
    char stringdata2[1];
    char stringdata3[10];
    char stringdata4[6];
    char stringdata5[10];
    char stringdata6[9];
    char stringdata7[8];
    char stringdata8[37];
    char stringdata9[8];
    char stringdata10[43];
    char stringdata11[17];
    char stringdata12[9];
    char stringdata13[7];
    char stringdata14[9];
    char stringdata15[4];
    char stringdata16[14];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_PrecisionRecallSweepWorker_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_PrecisionRecallSweepWorker_t qt_meta_stringdata_PrecisionRecallSweepWorker = {
    {
        QT_MOC_LITERAL(0, 26),  // "PrecisionRecallSweepWorker"
        QT_MOC_LITERAL(27, 8),  // "progress"
        QT_MOC_LITERAL(36, 0),  // ""
        QT_MOC_LITERAL(37, 9),  // "completed"
        QT_MOC_LITERAL(47, 5),  // "total"
        QT_MOC_LITERAL(53, 9),  // "threshold"
        QT_MOC_LITERAL(63, 8),  // "finished"
        QT_MOC_LITERAL(72, 7),  // "success"
        QT_MOC_LITERAL(80, 36),  // "QList<PrecisionRecallSweepRes..."
        QT_MOC_LITERAL(117, 7),  // "results"
        QT_MOC_LITERAL(125, 42),  // "QList<PrecisionRecallSweepThr..."
        QT_MOC_LITERAL(168, 16),  // "pairsByThreshold"
        QT_MOC_LITERAL(185, 8),  // "warnings"
        QT_MOC_LITERAL(194, 6),  // "errors"
        QT_MOC_LITERAL(201, 8),  // "canceled"
        QT_MOC_LITERAL(210, 3),  // "run"
        QT_MOC_LITERAL(214, 13)   // "requestCancel"
    },
    "PrecisionRecallSweepWorker",
    "progress",
    "",
    "completed",
    "total",
    "threshold",
    "finished",
    "success",
    "QList<PrecisionRecallSweepResultRow>",
    "results",
    "QList<PrecisionRecallSweepThresholdResult>",
    "pairsByThreshold",
    "warnings",
    "errors",
    "canceled",
    "run",
    "requestCancel"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_PrecisionRecallSweepWorker[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    3,   38,    2, 0x06,    1 /* Public */,
       6,    6,   45,    2, 0x06,    5 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      15,    0,   58,    2, 0x0a,   12 /* Public */,
      16,    0,   59,    2, 0x0a,   13 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Double,    3,    4,    5,
    QMetaType::Void, QMetaType::Bool, 0x80000000 | 8, 0x80000000 | 10, QMetaType::QStringList, QMetaType::QStringList, QMetaType::Bool,    7,    9,   11,   12,   13,   14,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject PrecisionRecallSweepWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_PrecisionRecallSweepWorker.offsetsAndSizes,
    qt_meta_data_PrecisionRecallSweepWorker,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_PrecisionRecallSweepWorker_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<PrecisionRecallSweepWorker, std::true_type>,
        // method 'progress'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'finished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVector<PrecisionRecallSweepResultRow> &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVector<PrecisionRecallSweepThresholdResult> &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'run'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'requestCancel'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void PrecisionRecallSweepWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<PrecisionRecallSweepWorker *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->progress((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3]))); break;
        case 1: _t->finished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QList<PrecisionRecallSweepResultRow>>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QList<PrecisionRecallSweepThresholdResult>>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[5])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[6]))); break;
        case 2: _t->run(); break;
        case 3: _t->requestCancel(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<PrecisionRecallSweepResultRow> >(); break;
            case 2:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<PrecisionRecallSweepThresholdResult> >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (PrecisionRecallSweepWorker::*)(int , int , double );
            if (_t _q_method = &PrecisionRecallSweepWorker::progress; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (PrecisionRecallSweepWorker::*)(bool , const QVector<PrecisionRecallSweepResultRow> & , const QVector<PrecisionRecallSweepThresholdResult> & , const QStringList & , const QStringList & , bool );
            if (_t _q_method = &PrecisionRecallSweepWorker::finished; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *PrecisionRecallSweepWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PrecisionRecallSweepWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_PrecisionRecallSweepWorker.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int PrecisionRecallSweepWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void PrecisionRecallSweepWorker::progress(int _t1, int _t2, double _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void PrecisionRecallSweepWorker::finished(bool _t1, const QVector<PrecisionRecallSweepResultRow> & _t2, const QVector<PrecisionRecallSweepThresholdResult> & _t3, const QStringList & _t4, const QStringList & _t5, bool _t6)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t5))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t6))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
