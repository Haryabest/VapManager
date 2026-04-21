/****************************************************************************
** Meta object code from reading C++ file 'main_shell_bridge.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.14.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../src/app/internal/main_shell_bridge.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'main_shell_bridge.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.14.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainShellBridge_t {
    QByteArrayData data[20];
    char stringdata0[306];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MainShellBridge_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MainShellBridge_t qt_meta_stringdata_MainShellBridge = {
    {
QT_MOC_LITERAL(0, 0, 15), // "MainShellBridge"
QT_MOC_LITERAL(1, 16, 14), // "sessionChanged"
QT_MOC_LITERAL(2, 31, 0), // ""
QT_MOC_LITERAL(3, 32, 14), // "profileUpdated"
QT_MOC_LITERAL(4, 47, 16), // "loadSystemStatus"
QT_MOC_LITERAL(5, 64, 18), // "loadCalendarEvents"
QT_MOC_LITERAL(6, 83, 5), // "month"
QT_MOC_LITERAL(7, 89, 4), // "year"
QT_MOC_LITERAL(8, 94, 23), // "loadUpcomingMaintenance"
QT_MOC_LITERAL(9, 118, 24), // "unreadNotificationsCount"
QT_MOC_LITERAL(10, 143, 22), // "loadCurrentUserProfile"
QT_MOC_LITERAL(11, 166, 22), // "saveCurrentUserProfile"
QT_MOC_LITERAL(12, 189, 7), // "profile"
QT_MOC_LITERAL(13, 197, 13), // "switchAccount"
QT_MOC_LITERAL(14, 211, 12), // "changeAvatar"
QT_MOC_LITERAL(15, 224, 14), // "changeLanguage"
QT_MOC_LITERAL(16, 239, 15), // "showAboutDialog"
QT_MOC_LITERAL(17, 255, 18), // "showSettingsDialog"
QT_MOC_LITERAL(18, 274, 15), // "currentUsername"
QT_MOC_LITERAL(19, 290, 15) // "currentUserRole"

    },
    "MainShellBridge\0sessionChanged\0\0"
    "profileUpdated\0loadSystemStatus\0"
    "loadCalendarEvents\0month\0year\0"
    "loadUpcomingMaintenance\0"
    "unreadNotificationsCount\0"
    "loadCurrentUserProfile\0saveCurrentUserProfile\0"
    "profile\0switchAccount\0changeAvatar\0"
    "changeLanguage\0showAboutDialog\0"
    "showSettingsDialog\0currentUsername\0"
    "currentUserRole"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainShellBridge[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       2,  102, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   79,    2, 0x06 /* Public */,
       3,    0,   80,    2, 0x06 /* Public */,

 // methods: name, argc, parameters, tag, flags
       4,    0,   81,    2, 0x02 /* Public */,
       5,    2,   82,    2, 0x02 /* Public */,
       8,    2,   87,    2, 0x02 /* Public */,
       9,    0,   92,    2, 0x02 /* Public */,
      10,    0,   93,    2, 0x02 /* Public */,
      11,    1,   94,    2, 0x02 /* Public */,
      13,    0,   97,    2, 0x02 /* Public */,
      14,    0,   98,    2, 0x02 /* Public */,
      15,    0,   99,    2, 0x02 /* Public */,
      16,    0,  100,    2, 0x02 /* Public */,
      17,    0,  101,    2, 0x02 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    QMetaType::QVariantMap,
    QMetaType::QVariantList, QMetaType::Int, QMetaType::Int,    6,    7,
    QMetaType::QVariantList, QMetaType::Int, QMetaType::Int,    6,    7,
    QMetaType::Int,
    QMetaType::QVariantMap,
    QMetaType::Bool, QMetaType::QVariantMap,   12,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags
      18, QMetaType::QString, 0x00495001,
      19, QMetaType::QString, 0x00495001,

 // properties: notify_signal_id
       0,
       0,

       0        // eod
};

void MainShellBridge::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainShellBridge *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->sessionChanged(); break;
        case 1: _t->profileUpdated(); break;
        case 2: { QVariantMap _r = _t->loadSystemStatus();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 3: { QVariantList _r = _t->loadCalendarEvents((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 4: { QVariantList _r = _t->loadUpcomingMaintenance((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariantList*>(_a[0]) = std::move(_r); }  break;
        case 5: { int _r = _t->unreadNotificationsCount();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 6: { QVariantMap _r = _t->loadCurrentUserProfile();
            if (_a[0]) *reinterpret_cast< QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 7: { bool _r = _t->saveCurrentUserProfile((*reinterpret_cast< const QVariantMap(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 8: { bool _r = _t->switchAccount();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 9: _t->changeAvatar(); break;
        case 10: _t->changeLanguage(); break;
        case 11: _t->showAboutDialog(); break;
        case 12: _t->showSettingsDialog(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MainShellBridge::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MainShellBridge::sessionChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (MainShellBridge::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MainShellBridge::profileUpdated)) {
                *result = 1;
                return;
            }
        }
    }
#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<MainShellBridge *>(_o);
        Q_UNUSED(_t)
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->currentUsername(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->currentUserRole(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    }
#endif // QT_NO_PROPERTIES
}

QT_INIT_METAOBJECT const QMetaObject MainShellBridge::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_MainShellBridge.data,
    qt_meta_data_MainShellBridge,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MainShellBridge::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainShellBridge::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainShellBridge.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int MainShellBridge::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 13;
    }
#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 2;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 2;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void MainShellBridge::sessionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void MainShellBridge::profileUpdated()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
