import QtQuick 2.12
pragma Singleton

QtObject {
    // Основные фоны - светлая тема
    property color bg: '#FFFFFF'
    property color bgSecondary: '#F3F4F6'
    property color bgTertiary: '#F8FAFC'
    
    // Текст - светлая тема
    property color text: '#1A1A1A'
    property color textSecondary: '#6B7280'
    property color textMuted: '#9CA3AF'
    
    // Границы
    property color border: '#E5E7EB'
    property color borderLight: '#F3F4F6'
    
    // Акцентные цвета
    property color primary: '#0F00DB'
    property color primaryHover: '#1A4ACD'
    property color error: '#FF3B30'
    property color success: '#18CF00'
    
    // Цвета статусов
    property color statusActive: '#18CF00'
    property color statusMaintenance: '#FF8800'
    property color statusError: '#FF0000'
    property color statusDisabled: '#6B7280'
    property color statusCompleted: '#00E5FF'
    
    // Навигация - светлая тема
    property color navBg: '#FFFFFF'
    property color navText: '#1A1A1A'
    property color navSelected: '#0F00DB'
    property color navHover: '#E6E6FA'
    
    // Карточки - светлая тема
    property color cardBg: '#FFFFFF'
    property color cardBorder: '#E5E7EB'
    
    // Заголовок - светлая тема
    property color headerBg: '#FFFFFF'
    property color headerText: '#1A1A1A'
    
    // Календарь - светлая тема
    property color calendarHeaderBg: '#555555'
    property color calendarHeaderText: '#FFFFFF'
    property color calendarCellBg: '#FFFFFF'
    property color calendarCellBorder: '#E5E7EB'
    property color calendarToday: '#E6E6FA'
}
