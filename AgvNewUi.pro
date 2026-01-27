QT       += core gui sql concurrent printsupport widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    addagvdialog.cpp \
    agvdetailinfo.cpp \
    agvsettingspage.cpp \
    leftmenu.cpp \
    listagvinfo.cpp \
    main.cpp \
    maintenanceitemwidget.cpp \
    mainwindow.cpp \
    modellistpage.cpp \
    multisectionwidget.cpp \
    notifications_logs.cpp

HEADERS += \
    addagvdialog.h \
    agvdetailinfo.h \
    agvsettingspage.h \
    leftmenu.h \
    listagvinfo.h \
    maintenanceitemwidget.h \
    mainwindow.h \
    modellistpage.h \
    multisectionwidget.h \
    notifications_logs.h

FORMS +=

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc

DISTFILES += \
    agv_pic/AGV_Background.png \
    agv_pic/IMG_20251201_094429_808.jpg \
    agv_pic/IMG_20251201_094430_103.jpg \
    agv_pic/IMG_20251201_094435_723.jpg \
    agv_pic/IMG_20251201_094435_964.jpg \
    agv_pic/IMG_20251201_094436_469.jpg \
    agv_pic/IMG_20251201_094436_469.png \
    agv_pic/IMG_20251201_094436_559 (1).png \
    agv_pic/VAPManagerLogo.png \
    agv_pic/VAPManagerLogo_ORIG.png \
    agv_pic/YearList.jpg \
    agv_pic/YearList.png \
    agv_pic/agvIcon.png \
    agv_pic/agvSetting.jpg \
    agv_pic/alert.jpg \
    agv_pic/bell.jpg \
    agv_pic/galka.png \
    agv_pic/logs.png \
    agv_pic/logs.webp \
    agv_pic/lupa.jpg \
    agv_pic/print.png \
    agv_pic/setting_icon.png \
    agv_pic/snapedit_1765268826913 (1).png \
    agv_pic/snapedit_1765268826913.png \
    agv_pic/warning.jpg
