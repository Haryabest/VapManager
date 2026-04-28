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
    src/core/logging/diag_logger.cpp \
    src/features/account/ui/accountinfodialog.cpp \
    src/core/session/app_session.cpp \
    src/features/agv/ui/addagvdialog.cpp \
    src/features/agv/ui/agvsettingspage.cpp \
    src/features/agv/ui/internal/listagvinfo_ui_modules.cpp \
    src/features/agv/ui/internal/listagvinfo_agv_item.cpp \
    src/features/agv/ui/internal/listagvinfo_storage.cpp \
    src/features/agv/ui/internal/listagvinfo_undo.cpp \
    src/features/agv/ui/internal/listagvinfo_render.cpp \
    src/features/agv/ui/internal/listagvinfo_constructor.cpp \
    src/features/agv/ui/internal/listagvinfo_constructor_impl.cpp \
    src/features/agv/ui/internal/addagvdialog_impl.cpp \
    src/features/agv/ui/internal/agvsettings/agvsettingspage_constructor.cpp \
    src/features/agv/ui/internal/agvsettings/agvsettingspage_ui.cpp \
    src/features/agv/ui/internal/agvsettings/agvsettingspage_agv_edit.cpp \
    src/features/agv/ui/internal/agvsettings/agvsettingspage_forms.cpp \
    src/features/agv/ui/internal/agvsettings/agvsettingspage_tasks_ops.cpp \
    src/features/agv/ui/internal/agvsettings/agvsettingspage_history.cpp \
    src/app/internal/app_bootstrap.cpp \
    src/features/shell/ui/internal/mainwindow_impl.cpp \
    src/features/common/ui/internal/maintenanceitemwidget_impl.cpp \
    src/features/models/ui/internal/modellistpage_impl.cpp \
    src/features/users/ui/internal/userspage_impl.cpp \
    src/features/users/ui/internal/userspage_constructor.cpp \
    src/features/users/ui/internal/userspage_load_users.cpp \
    src/features/users/ui/internal/userspage_update_user.cpp \
    src/features/users/ui/internal/userspage_collapsible_section.cpp \
    src/features/users/ui/internal/userspage_user_item.cpp \
    src/features/users/ui/internal/userspage_user_types.cpp \
    src/features/chat/ui/internal/taskchatdialog_impl.cpp \
    src/core/events/databus.cpp \
    src/data/db/db.cpp \
    src/data/repositories/db_agv_tasks.cpp \
    src/data/repositories/db_agv_errors.cpp \
    src/data/repositories/db_models.cpp \
    src/data/repositories/db_task_chat.cpp \
    src/data/repositories/db_users.cpp \
    src/data/repositories/db_users_schema.cpp \
    src/data/repositories/db_users_logging.cpp \
    src/data/repositories/db_users_invites.cpp \
    src/data/repositories/db_users_auth.cpp \
    src/data/repositories/db_users_profile.cpp \
    src/data/repositories/db_users_avatar_presence.cpp \
    src/data/repositories/internal/db_users_internal_state.cpp \
    src/features/shell/ui/internal/leftmenu_calendar_utils.cpp \
    src/features/shell/ui/internal/leftmenu_settings_dialogs.cpp \
    src/features/shell/ui/leftmenu.cpp \
    src/features/agv/ui/listagvinfo.cpp \
    src/features/account/ui/logindialog.cpp \
    src/features/account/ui/internal/logindialog_constructor.cpp \
    src/features/account/ui/internal/logindialog_ui_builders.cpp \
    src/features/account/ui/internal/logindialog_wiring.cpp \
    src/features/account/ui/internal/logindialog_slots.cpp \
    src/features/account/ui/internal/logindialog_recovery_dialog.cpp \
    src/features/account/ui/internal/logindialog_events.cpp \
    src/app/main.cpp \
    src/features/common/ui/maintenanceitemwidget.cpp \
    src/features/shell/ui/mainwindow.cpp \
    src/features/models/ui/modellistpage.cpp \
    src/features/common/ui/multisectionwidget.cpp \
    src/features/notifications/ui/notifications_logs.cpp \
    src/features/chat/ui/taskchatdialog.cpp \
    src/core/logging/ui_action_logger.cpp \
    src/features/users/ui/userspage.cpp

HEADERS += \
    src/app/internal/app_bootstrap.h \
    src/core/logging/diag_logger.h \
    src/features/account/ui/accountinfodialog.h \
    src/core/session/app_session.h \
    src/features/agv/ui/addagvdialog.h \
    src/features/agv/ui/agvsettingspage.h \
    src/features/agv/ui/internal/listagvinfo_ui_modules.h \
    src/core/events/databus.h \
    src/data/db/db.h \
    src/data/repositories/db_agv_tasks.h \
    src/data/repositories/db_agv_errors.h \
    src/data/repositories/db_models.h \
    src/data/repositories/db_task_chat.h \
    src/data/repositories/db_users.h \
    src/features/shell/ui/internal/leftmenu_calendar_utils.h \
    src/features/shell/ui/internal/leftmenu_settings_dialogs.h \
    src/features/shell/ui/leftmenu.h \
    src/features/agv/ui/listagvinfo.h \
    src/features/account/ui/logindialog.h \
    src/features/common/ui/maintenanceitemwidget.h \
    src/features/shell/ui/mainwindow.h \
    src/features/models/ui/modellistpage.h \
    src/features/common/ui/multisectionwidget.h \
    src/features/notifications/ui/notifications_logs.h \
    src/features/chat/ui/taskchatdialog.h \
    src/core/logging/ui_action_logger.h \
    src/features/users/ui/userspage.h

FORMS +=

INCLUDEPATH += \
    src/app \
    src/features/account/ui \
    src/features/agv/ui \
    src/features/common/ui \
    src/features/models/ui \
    src/features/notifications/ui \
    src/features/chat/ui \
    src/features/shell/ui \
    src/features/users/ui \
    src/core/session \
    src/core/events \
    src/core/logging \
    src/data/db \
    src/data/repositories

TRANSLATIONS += AgvNewUi_en.ts AgvNewUi_zh.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc

# Иконка приложения для Windows (exe, панель задач, рабочий стол)
win32: RC_ICONS = noback/agvIcon.ico
win32: LIBS += -lwinmm

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
