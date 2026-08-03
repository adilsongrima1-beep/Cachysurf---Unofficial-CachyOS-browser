/****************************************************************************
** Meta object code from reading C++ file 'hidbridge.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/hidbridge.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'hidbridge.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN13HidPageBridgeE_t {};
} // unnamed namespace

template <> constexpr inline auto HidPageBridge::qt_create_metaobjectdata<qt_meta_tag_ZN13HidPageBridgeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "HidPageBridge",
        "inputReport",
        "",
        "deviceId",
        "reportId",
        "QVariantList",
        "data",
        "deviceDisconnected",
        "requestDevices",
        "QVariantMap",
        "optionsJson",
        "getDevices",
        "openDevice",
        "closeDevice",
        "sendReport",
        "sendFeatureReport",
        "receiveFeatureReport",
        "length",
        "forgetDevice"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'inputReport'
        QtMocHelpers::SignalData<void(const QString &, int, const QVariantList &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::Int, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'deviceDisconnected'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Method 'requestDevices'
        QtMocHelpers::MethodData<QVariantMap(const QString &)>(8, 2, QMC::AccessPublic, 0x80000000 | 9, {{
            { QMetaType::QString, 10 },
        }}),
        // Method 'getDevices'
        QtMocHelpers::MethodData<QVariantMap()>(11, 2, QMC::AccessPublic, 0x80000000 | 9),
        // Method 'openDevice'
        QtMocHelpers::MethodData<QVariantMap(const QString &)>(12, 2, QMC::AccessPublic, 0x80000000 | 9, {{
            { QMetaType::QString, 3 },
        }}),
        // Method 'closeDevice'
        QtMocHelpers::MethodData<QVariantMap(const QString &)>(13, 2, QMC::AccessPublic, 0x80000000 | 9, {{
            { QMetaType::QString, 3 },
        }}),
        // Method 'sendReport'
        QtMocHelpers::MethodData<QVariantMap(const QString &, int, const QVariantList &)>(14, 2, QMC::AccessPublic, 0x80000000 | 9, {{
            { QMetaType::QString, 3 }, { QMetaType::Int, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Method 'sendFeatureReport'
        QtMocHelpers::MethodData<QVariantMap(const QString &, int, const QVariantList &)>(15, 2, QMC::AccessPublic, 0x80000000 | 9, {{
            { QMetaType::QString, 3 }, { QMetaType::Int, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Method 'receiveFeatureReport'
        QtMocHelpers::MethodData<QVariantMap(const QString &, int, int)>(16, 2, QMC::AccessPublic, 0x80000000 | 9, {{
            { QMetaType::QString, 3 }, { QMetaType::Int, 4 }, { QMetaType::Int, 17 },
        }}),
        // Method 'forgetDevice'
        QtMocHelpers::MethodData<QVariantMap(const QString &)>(18, 2, QMC::AccessPublic, 0x80000000 | 9, {{
            { QMetaType::QString, 3 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<HidPageBridge, qt_meta_tag_ZN13HidPageBridgeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject HidPageBridge::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13HidPageBridgeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13HidPageBridgeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13HidPageBridgeE_t>.metaTypes,
    nullptr
} };

void HidPageBridge::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<HidPageBridge *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->inputReport((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[3]))); break;
        case 1: _t->deviceDisconnected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: { QVariantMap _r = _t->requestDevices((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 3: { QVariantMap _r = _t->getDevices();
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 4: { QVariantMap _r = _t->openDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 5: { QVariantMap _r = _t->closeDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 6: { QVariantMap _r = _t->sendReport((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[3])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 7: { QVariantMap _r = _t->sendFeatureReport((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[3])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 8: { QVariantMap _r = _t->receiveFeatureReport((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 9: { QVariantMap _r = _t->forgetDevice((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (HidPageBridge::*)(const QString & , int , const QVariantList & )>(_a, &HidPageBridge::inputReport, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (HidPageBridge::*)(const QString & )>(_a, &HidPageBridge::deviceDisconnected, 1))
            return;
    }
}

const QMetaObject *HidPageBridge::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *HidPageBridge::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13HidPageBridgeE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int HidPageBridge::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void HidPageBridge::inputReport(const QString & _t1, int _t2, const QVariantList & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3);
}

// SIGNAL 1
void HidPageBridge::deviceDisconnected(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
namespace {
struct qt_meta_tag_ZN9HidBridgeE_t {};
} // unnamed namespace

template <> constexpr inline auto HidBridge::qt_create_metaobjectdata<qt_meta_tag_ZN9HidBridgeE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "HidBridge",
        "inputReport",
        "",
        "deviceId",
        "reportId",
        "QVariantList",
        "data",
        "deviceDisconnected"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'inputReport'
        QtMocHelpers::SignalData<void(const QString &, int, const QVariantList &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::Int, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'deviceDisconnected'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<HidBridge, qt_meta_tag_ZN9HidBridgeE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject HidBridge::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9HidBridgeE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9HidBridgeE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9HidBridgeE_t>.metaTypes,
    nullptr
} };

void HidBridge::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<HidBridge *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->inputReport((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[3]))); break;
        case 1: _t->deviceDisconnected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (HidBridge::*)(const QString & , int , const QVariantList & )>(_a, &HidBridge::inputReport, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (HidBridge::*)(const QString & )>(_a, &HidBridge::deviceDisconnected, 1))
            return;
    }
}

const QMetaObject *HidBridge::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *HidBridge::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9HidBridgeE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int HidBridge::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void HidBridge::inputReport(const QString & _t1, int _t2, const QVariantList & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3);
}

// SIGNAL 1
void HidBridge::deviceDisconnected(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
QT_WARNING_POP
