#pragma once

#include <QString>

namespace ChatMessageCrypto {

QString encrypt(const QString &plainText);
QString decrypt(const QString &storedText);

}
