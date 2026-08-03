/****************************************************************************
** Meta object code from reading C++ file 'browserwindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/browserwindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'browserwindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13BrowserWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto BrowserWindow::qt_create_metaobjectdata<qt_meta_tag_ZN13BrowserWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "BrowserWindow",
        "closeTab",
        "",
        "index",
        "activateTab",
        "navigateFromAddressBar",
        "updateFromCurrentView",
        "handleDownload",
        "QWebEngineDownloadRequest*",
        "download",
        "showMainMenu",
        "addCurrentBookmark",
        "openSidebarItem",
        "toggleSidebar",
        "toggleDarkWebsites",
        "enabled",
        "toggleTrackerBlocking",
        "togglePasswordAutofill",
        "openPrivateWindow",
        "toggleFullScreen",
        "showPasswordManager",
        "addPasswordForCurrentSite",
        "fillPasswordForCurrentSite",
        "importPasswordsFromCsv",
        "showExtensionManager",
        "showDeviceManager",
        "showHistory",
        "findOnPage",
        "printCurrentPage",
        "clearBrowsingData",
        "reopenClosedTab",
        "updateAddressSuggestions",
        "text"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'closeTab'
        QtMocHelpers::SlotData<void(int)>(1, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Slot 'activateTab'
        QtMocHelpers::SlotData<void(int)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Slot 'navigateFromAddressBar'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateFromCurrentView'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleDownload'
        QtMocHelpers::SlotData<void(QWebEngineDownloadRequest *)>(7, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 8, 9 },
        }}),
        // Slot 'showMainMenu'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'addCurrentBookmark'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openSidebarItem'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'toggleSidebar'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'toggleDarkWebsites'
        QtMocHelpers::SlotData<void(bool)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 15 },
        }}),
        // Slot 'toggleTrackerBlocking'
        QtMocHelpers::SlotData<void(bool)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 15 },
        }}),
        // Slot 'togglePasswordAutofill'
        QtMocHelpers::SlotData<void(bool)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 15 },
        }}),
        // Slot 'openPrivateWindow'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'toggleFullScreen'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showPasswordManager'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'addPasswordForCurrentSite'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'fillPasswordForCurrentSite'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'importPasswordsFromCsv'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showExtensionManager'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showDeviceManager'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showHistory'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'findOnPage'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'printCurrentPage'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'clearBrowsingData'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'reopenClosedTab'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateAddressSuggestions'
        QtMocHelpers::SlotData<void(const QString &)>(31, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 32 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<BrowserWindow, qt_meta_tag_ZN13BrowserWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject BrowserWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13BrowserWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13BrowserWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13BrowserWindowE_t>.metaTypes,
    nullptr
} };

void BrowserWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<BrowserWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->closeTab((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->activateTab((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->navigateFromAddressBar(); break;
        case 3: _t->updateFromCurrentView(); break;
        case 4: _t->handleDownload((*reinterpret_cast<std::add_pointer_t<QWebEngineDownloadRequest*>>(_a[1]))); break;
        case 5: _t->showMainMenu(); break;
        case 6: _t->addCurrentBookmark(); break;
        case 7: _t->openSidebarItem(); break;
        case 8: _t->toggleSidebar(); break;
        case 9: _t->toggleDarkWebsites((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->toggleTrackerBlocking((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 11: _t->togglePasswordAutofill((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 12: _t->openPrivateWindow(); break;
        case 13: _t->toggleFullScreen(); break;
        case 14: _t->showPasswordManager(); break;
        case 15: _t->addPasswordForCurrentSite(); break;
        case 16: _t->fillPasswordForCurrentSite(); break;
        case 17: _t->importPasswordsFromCsv(); break;
        case 18: _t->showExtensionManager(); break;
        case 19: _t->showDeviceManager(); break;
        case 20: _t->showHistory(); break;
        case 21: _t->findOnPage(); break;
        case 22: _t->printCurrentPage(); break;
        case 23: _t->clearBrowsingData(); break;
        case 24: _t->reopenClosedTab(); break;
        case 25: _t->updateAddressSuggestions((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *BrowserWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *BrowserWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13BrowserWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int BrowserWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 26)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 26;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 26)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 26;
    }
    return _id;
}
QT_WARNING_POP
