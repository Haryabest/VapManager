.pragma library

function validateEmail(email) {
    var re = /^[^\s@]+@[^\s@]+\.[^\s@]+$/
    return re.test(email)
}

function register(email, login, password, confirmPassword) {
    // Валидация email
    if (email === "") {
        return { success: false, message: "Введите email" }
    }
    if (!validateEmail(email)) {
        return { success: false, message: "Некорректный email" }
    }

    // Валидация логина
    if (login === "") {
        return { success: false, message: "Введите логин" }
    }

    // Валидация пароля
    if (password === "") {
        return { success: false, message: "Введите пароль" }
    }
    if (password.length < 6) {
        return { success: false, message: "Пароль должен быть не менее 6 символов" }
    }

    // Проверка совпадения паролей
    if (password !== confirmPassword) {
        return { success: false, message: "Пароли не совпадают" }
    }

    // Здесь будет вызов backend для регистрации
    console.log("Регистрация пользователя:")
    console.log("  Email:", email)
    console.log("  Логин:", login)
    console.log("  Пароль:", password)

    // TODO: Вызов API для регистрации
    // Например: httpClient.post("/api/register", {email, login, password})

    return { success: true, message: "Регистрация успешна!" }
}
