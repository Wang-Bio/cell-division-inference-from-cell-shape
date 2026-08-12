/****************************************************************************
** Meta object code from reading C++ file 'polygonitem.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../source-code/polygonitem.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'polygonitem.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_PolygonItem_t {
    uint offsetsAndSizes[24];
    char stringdata0[12];
    char stringdata1[9];
    char stringdata2[1];
    char stringdata3[13];
    char stringdata4[5];
    char stringdata5[16];
    char stringdata6[14];
    char stringdata7[12];
    char stringdata8[2];
    char stringdata9[4];
    char stringdata10[18];
    char stringdata11[4];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_PolygonItem_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_PolygonItem_t qt_meta_stringdata_PolygonItem = {
    {
        QT_MOC_LITERAL(0, 11),  // "PolygonItem"
        QT_MOC_LITERAL(12, 8),  // "selected"
        QT_MOC_LITERAL(21, 0),  // ""
        QT_MOC_LITERAL(22, 12),  // "PolygonItem*"
        QT_MOC_LITERAL(35, 4),  // "self"
        QT_MOC_LITERAL(40, 15),  // "geometryChanged"
        QT_MOC_LITERAL(56, 13),  // "onVertexMoved"
        QT_MOC_LITERAL(70, 11),  // "VertexItem*"
        QT_MOC_LITERAL(82, 1),  // "v"
        QT_MOC_LITERAL(84, 3),  // "pos"
        QT_MOC_LITERAL(88, 17),  // "onVertexDestroyed"
        QT_MOC_LITERAL(106, 3)   // "obj"
    },
    "PolygonItem",
    "selected",
    "",
    "PolygonItem*",
    "self",
    "geometryChanged",
    "onVertexMoved",
    "VertexItem*",
    "v",
    "pos",
    "onVertexDestroyed",
    "obj"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_PolygonItem[] = {

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
       1,    1,   38,    2, 0x06,    1 /* Public */,
       5,    1,   41,    2, 0x06,    3 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       6,    2,   44,    2, 0x08,    5 /* Private */,
      10,    1,   49,    2, 0x08,    8 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3,    4,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 7, QMetaType::QPointF,    8,    9,
    QMetaType::Void, QMetaType::QObjectStar,   11,

       0        // eod
};

Q_CONSTINIT const QMetaObject PolygonItem::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_PolygonItem.offsetsAndSizes,
    qt_meta_data_PolygonItem,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_PolygonItem_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<PolygonItem, std::true_type>,
        // method 'selected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<PolygonItem *, std::false_type>,
        // method 'geometryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<PolygonItem *, std::false_type>,
        // method 'onVertexMoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<VertexItem *, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPointF &, std::false_type>,
        // method 'onVertexDestroyed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QObject *, std::false_type>
    >,
    nullptr
} };

void PolygonItem::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<PolygonItem *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->selected((*reinterpret_cast< std::add_pointer_t<PolygonItem*>>(_a[1]))); break;
        case 1: _t->geometryChanged((*reinterpret_cast< std::add_pointer_t<PolygonItem*>>(_a[1]))); break;
        case 2: _t->onVertexMoved((*reinterpret_cast< std::add_pointer_t<VertexItem*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 3: _t->onVertexDestroyed((*reinterpret_cast< std::add_pointer_t<QObject*>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< PolygonItem* >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< PolygonItem* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (PolygonItem::*)(PolygonItem * );
            if (_t _q_method = &PolygonItem::selected; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (PolygonItem::*)(PolygonItem * );
            if (_t _q_method = &PolygonItem::geometryChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *PolygonItem::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *PolygonItem::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_PolygonItem.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QGraphicsPolygonItem"))
        return static_cast< QGraphicsPolygonItem*>(this);
    return QObject::qt_metacast(_clname);
}

int PolygonItem::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void PolygonItem::selected(PolygonItem * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void PolygonItem::geometryChanged(PolygonItem * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
