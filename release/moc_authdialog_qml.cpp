/****************************************************************************
** Meta object code from reading C++ file 'authdialog_qml.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.14.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../src/features/account/ui/authdialog_qml.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'authdialog_qml.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.14.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_AuthDialogQml_t {
    QByteArrayData data[25];
    char stringdata0[307];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_AuthDialogQml_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_AuthDialogQml_t qt_meta_stringdata_AuthDialogQml = {
    {
QT_MOC_LITERAL(0, 0, 13), // "AuthDialogQml"
QT_MOC_LITERAL(1, 14, 14), // "loginRequested"
QT_MOC_LITERAL(2, 29, 0), // ""
QT_MOC_LITERAL(3, 30, 5), // "login"
QT_MOC_LITERAL(4, 36, 8), // "password"
QT_MOC_LITERAL(5, 45, 17), // "registerRequested"
QT_MOC_LITERAL(6, 63, 15), // "confirmPassword"
QT_MOC_LITERAL(7, 79, 4), // "role"
QT_MOC_LITERAL(8, 84, 8), // "adminKey"
QT_MOC_LITERAL(9, 93, 7), // "techKey"
QT_MOC_LITERAL(10, 101, 17), // "recoveryRequested"
QT_MOC_LITERAL(11, 119, 11), // "recoveryKey"
QT_MOC_LITERAL(12, 131, 25), // "recoveryFromFileRequested"
QT_MOC_LITERAL(13, 157, 7), // "onLogin"
QT_MOC_LITERAL(14, 165, 10), // "onRegister"
QT_MOC_LITERAL(15, 176, 10), // "onRecovery"
QT_MOC_LITERAL(16, 187, 18), // "onRecoveryFromFile"
QT_MOC_LITERAL(17, 206, 11), // "onAppClosed"
QT_MOC_LITERAL(18, 218, 13), // "setLoginError"
QT_MOC_LITERAL(19, 232, 5), // "error"
QT_MOC_LITERAL(20, 238, 16), // "setRegisterError"
QT_MOC_LITERAL(21, 255, 16), // "setRecoveryError"
QT_MOC_LITERAL(22, 272, 21), // "showRecoveryKeyDialog"
QT_MOC_LITERAL(23, 294, 8), // "username"
QT_MOC_LITERAL(24, 303, 3) // "key"

    },
    "AuthDialogQml\0loginRequested\0\0login\0"
    "password\0registerRequested\0confirmPassword\0"
    "role\0adminKey\0techKey\0recoveryRequested\0"
    "recoveryKey\0recoveryFromFileRequested\0"
    "onLogin\0onRegister\0onRecovery\0"
    "onRecoveryFromFile\0onAppClosed\0"
    "setLoginError\0error\0setRegisterError\0"
    "setRecoveryError\0showRecoveryKeyDialog\0"
    "username\0key"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AuthDialogQml[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    2,   79,    2, 0x06 /* Public */,
       5,    6,   84,    2, 0x06 /* Public */,
      10,    1,   97,    2, 0x06 /* Public */,
      12,    0,  100,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      13,    2,  101,    2, 0x0a /* Public */,
      14,    6,  106,    2, 0x0a /* Public */,
      15,    1,  119,    2, 0x0a /* Public */,
      16,    0,  122,    2, 0x0a /* Public */,
      17,    0,  123,    2, 0x0a /* Public */,
      18,    1,  124,    2, 0x0a /* Public */,
      20,    1,  127,    2, 0x0a /* Public */,
      21,    1,  130,    2, 0x0a /* Public */,
      22,    2,  133,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    3,    4,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,    3,    4,    6,    7,    8,    9,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    3,    4,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,    3,    4,    6,    7,    8,    9,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   19,
    QMetaType::Void, QMetaType::QString,   19,
    QMetaType::Void, QMetaType::QString,   19,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   23,   24,

       0        // eod
};

void AuthDialogQml::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AuthDialogQml *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->loginRequested((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 1: _t->registerRequested((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QString(*)>(_a[4])),(*reinterpret_cast< const QString(*)>(_a[5])),(*reinterpret_cast< const QString(*)>(_a[6]))); break;
        case 2: _t->recoveryRequested((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->recoveryFromFileRequested(); break;
        case 4: _t->onLogin((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 5: _t->onRegister((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])),(*reinterpret_cast< const QString(*)>(_a[4])),(*reinterpret_cast< const QString(*)>(_a[5])),(*reinterpret_cast< const QString(*)>(_a[6]))); break;
        case 6: _t->onRecovery((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 7: _t->onRecoveryFromFile(); break;
        case 8: _t->onAppClosed(); break;
        case 9: _t->setLoginError((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 10: _t->setRegisterError((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 11: _t->setRecoveryError((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 12: _t->showRecoveryKeyDialog((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AuthDialogQml::*)(const QString & , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AuthDialogQml::loginRequested)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (AuthDialogQml::*)(const QString & , const QString & , const QString & , const QString & , const QString & , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AuthDialogQml::registerRequested)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (AuthDialogQml::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AuthDialogQml::recoveryRequested)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (AuthDialogQml::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AuthDialogQml::recoveryFromFileRequested)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject AuthDialogQml::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_AuthDialogQml.data,
    qt_meta_data_AuthDialogQml,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *AuthDialogQml::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AuthDialogQml::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AuthDialogQml.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AuthDialogQml::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
    return _id;
}

// SIGNAL 0
void AuthDialogQml::loginRequested(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void AuthDialogQml::registerRequested(const QString & _t1, const QString & _t2, const QString & _t3, const QString & _t4, const QString & _t5, const QString & _t6)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t5))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t6))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void AuthDialogQml::recoveryRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void AuthDialogQml::recoveryFromFileRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
