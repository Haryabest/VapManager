/****************************************************************************
** Meta object code from reading C++ file 'registerpage_controller.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.14.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../src/features/account/ui/registerpage_controller.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'registerpage_controller.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.14.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_RegisterPageController_t {
    QByteArrayData data[23];
    char stringdata0[287];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_RegisterPageController_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_RegisterPageController_t qt_meta_stringdata_RegisterPageController = {
    {
QT_MOC_LITERAL(0, 0, 22), // "RegisterPageController"
QT_MOC_LITERAL(1, 23, 13), // "requestGoBack"
QT_MOC_LITERAL(2, 37, 0), // ""
QT_MOC_LITERAL(3, 38, 22), // "requestShowRecoveryKey"
QT_MOC_LITERAL(4, 61, 8), // "username"
QT_MOC_LITERAL(5, 70, 11), // "recoveryKey"
QT_MOC_LITERAL(6, 82, 19), // "requestLoginSuccess"
QT_MOC_LITERAL(7, 102, 8), // "UserInfo"
QT_MOC_LITERAL(8, 111, 4), // "user"
QT_MOC_LITERAL(9, 116, 17), // "onRegisterClicked"
QT_MOC_LITERAL(10, 134, 5), // "login"
QT_MOC_LITERAL(11, 140, 8), // "password"
QT_MOC_LITERAL(12, 149, 15), // "confirmPassword"
QT_MOC_LITERAL(13, 165, 4), // "role"
QT_MOC_LITERAL(14, 170, 8), // "adminKey"
QT_MOC_LITERAL(15, 179, 7), // "techKey"
QT_MOC_LITERAL(16, 187, 13), // "onBackClicked"
QT_MOC_LITERAL(17, 201, 18), // "onPassword1Changed"
QT_MOC_LITERAL(18, 220, 4), // "text"
QT_MOC_LITERAL(19, 225, 18), // "onLoginTextChanged"
QT_MOC_LITERAL(20, 244, 22), // "onPassword2TextChanged"
QT_MOC_LITERAL(21, 267, 13), // "onRoleChanged"
QT_MOC_LITERAL(22, 281, 5) // "index"

    },
    "RegisterPageController\0requestGoBack\0"
    "\0requestShowRecoveryKey\0username\0"
    "recoveryKey\0requestLoginSuccess\0"
    "UserInfo\0user\0onRegisterClicked\0login\0"
    "password\0confirmPassword\0role\0adminKey\0"
    "techKey\0onBackClicked\0onPassword1Changed\0"
    "text\0onLoginTextChanged\0onPassword2TextChanged\0"
    "onRoleChanged\0index"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_RegisterPageController[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   59,    2, 0x06 /* Public */,
       3,    2,   60,    2, 0x06 /* Public */,
       6,    1,   65,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       9,    6,   68,    2, 0x0a /* Public */,
      16,    0,   81,    2, 0x0a /* Public */,
      17,    1,   82,    2, 0x0a /* Public */,
      19,    1,   85,    2, 0x0a /* Public */,
      20,    1,   88,    2, 0x0a /* Public */,
      21,    1,   91,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    4,    5,
    QMetaType::Void, 0x80000000 | 7,    8,

 // slots: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   10,   11,   12,   13,   14,   15,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   18,
    QMetaType::Void, QMetaType::QString,   18,
    QMetaType::Void, QMetaType::QString,   18,
    QMetaType::Void, QMetaType::Int,   22,

       0        // eod
};

void RegisterPageController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<RegisterPageController *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->requestGoBack(); break;
        case 1: _t->requestShowRecoveryKey((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 2: _t->requestLoginSuccess((*reinterpret_cast< const UserInfo(*)>(_a[1]))); break;
        case 3: _t->onRegisterClicked((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QString(*)>(_a[4])),(*reinterpret_cast< const QString(*)>(_a[5])),(*reinterpret_cast< const QString(*)>(_a[6]))); break;
        case 4: _t->onBackClicked(); break;
        case 5: _t->onPassword1Changed((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 6: _t->onLoginTextChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 7: _t->onPassword2TextChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 8: _t->onRoleChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (RegisterPageController::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&RegisterPageController::requestGoBack)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (RegisterPageController::*)(const QString & , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&RegisterPageController::requestShowRecoveryKey)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (RegisterPageController::*)(const UserInfo & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&RegisterPageController::requestLoginSuccess)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject RegisterPageController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_RegisterPageController.data,
    qt_meta_data_RegisterPageController,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *RegisterPageController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RegisterPageController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_RegisterPageController.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int RegisterPageController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void RegisterPageController::requestGoBack()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void RegisterPageController::requestShowRecoveryKey(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void RegisterPageController::requestLoginSuccess(const UserInfo & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
