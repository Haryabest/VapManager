import QtQuick 2.0
import QtQuick.Controls 2.0

import "pages"

ApplicationWindow {
    visible: true
    width: 600
    height: 600
    title: "Vap Manager"

    // Старая страница (для сравнения)
    // RegistePager {
    //     anchors.fill: parent
    // }

    // Новая страница регистрации
    RegisterPage {
        id: registerPage
        anchors.fill: parent

        // Подключение к C++ будет здесь
        // registerClicked -> C++ slot
        // backClicked -> C++ slot
    }
}
