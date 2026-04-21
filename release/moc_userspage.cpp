/****************************************************************************
** Meta object code from reading C++ file 'userspage.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.14.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../src/features/users/ui/userspage.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'userspage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.14.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_UsersPage_t {
    QByteArrayData data[10];
    char stringdata0[135];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_UsersPage_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_UsersPage_t qt_meta_stringdata_UsersPage = {
    {
QT_MOC_LITERAL(0, 0, 9), // "UsersPage"
QT_MOC_LITERAL(1, 10, 13), // "backRequested"
QT_MOC_LITERAL(2, 24, 0), // ""
QT_MOC_LITERAL(3, 25, 24), // "openUserDetailsRequested"
QT_MOC_LITERAL(4, 50, 8), // "username"
QT_MOC_LITERAL(5, 59, 16), // "addUserRequested"
QT_MOC_LITERAL(6, 76, 11), // "userUpdated"
QT_MOC_LITERAL(7, 88, 19), // "deleteUserRequested"
QT_MOC_LITERAL(8, 108, 9), // "loadUsers"
QT_MOC_LITERAL(9, 118, 16) // "updateUserInList"

    },
    "UsersPage\0backRequested\0\0"
    "openUserDetailsRequested\0username\0"
    "addUserRequested\0userUpdated\0"
    "deleteUserRequested\0loadUsers\0"
    "updateUserInList"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_UsersPage[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x06 /* Public */,
       3,    1,   50,    2, 0x06 /* Public */,
       5,    0,   53,    2, 0x06 /* Public */,
       6,    1,   54,    2, 0x06 /* Public */,
       7,    1,   57,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       8,    0,   60,    2, 0x0a /* Public */,
       9,    1,   61,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    4,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    4,
    QMetaType::Void, QMetaType::QString,    4,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    4,

       0        // eod
};

void UsersPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<UsersPage *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->backRequested(); break;
        case 1: _t->openUserDetailsRequested((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->addUserRequested(); break;
        case 3: _t->userUpdated((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 4: _t->deleteUserRequested((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 5: _t->loadUsers(); break;
        case 6: _t->updateUserInList((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (UsersPage::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&UsersPage::backRequested)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (UsersPage::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&UsersPage::openUserDetailsRequested)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (UsersPage::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&UsersPage::addUserRequested)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (UsersPage::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&UsersPage::userUpdated)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (UsersPage::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&UsersPage::deleteUserRequested)) {
                *result = 4;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject UsersPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_UsersPage.data,
    qt_meta_data_UsersPage,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *UsersPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UsersPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_UsersPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int UsersPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void UsersPage::backRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void UsersPage::openUserDetailsRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void UsersPage::addUserRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void UsersPage::userUpdated(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void UsersPage::deleteUserRequested(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
