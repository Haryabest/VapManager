#include "chat_message_crypto.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

namespace {

constexpr char kPrefix[] = "enc1:";
constexpr char kMasterPepper[] = "VapManager.Chat.AtRest.v1";

QByteArray masterKey()
{
    return QCryptographicHash::hash(QByteArray(kMasterPepper), QCryptographicHash::Sha256);
}

QByteArray streamXor(const QByteArray &key, const QByteArray &nonce, const QByteArray &input)
{
    QByteArray out;
    out.resize(input.size());

    int outPos = 0;
    int blockIdx = 0;
    while (outPos < input.size()) {
        const QByteArray block = QCryptographicHash::hash(
            key + nonce + QByteArray::number(blockIdx++),
            QCryptographicHash::Sha256);
        for (int i = 0; i < block.size() && outPos < input.size(); ++i, ++outPos)
            out[outPos] = input[outPos] ^ block[i];
    }
    return out;
}

} // namespace

namespace ChatMessageCrypto {

QString encrypt(const QString &plainText)
{
    const QByteArray plain = plainText.toUtf8();
    if (plain.isEmpty())
        return plainText;

    QByteArray nonce(16, 0);
    for (int i = 0; i < nonce.size(); ++i)
        nonce[i] = char(QRandomGenerator::global()->bounded(256));

    const QByteArray cipher = streamXor(masterKey(), nonce, plain);
    const QByteArray packed = nonce + cipher;
    return QString::fromLatin1(kPrefix)
           + QString::fromLatin1(packed.toBase64(QByteArray::Base64UrlEncoding));
}

QString decrypt(const QString &storedText)
{
    if (!storedText.startsWith(QLatin1String(kPrefix)))
        return storedText;

    const QByteArray packed = QByteArray::fromBase64(
        storedText.mid(int(sizeof(kPrefix) - 1)).toLatin1(),
        QByteArray::Base64UrlEncoding);
    if (packed.size() <= 16)
        return storedText;

    const QByteArray nonce = packed.left(16);
    const QByteArray cipher = packed.mid(16);
    const QByteArray plain = streamXor(masterKey(), nonce, cipher);
    return QString::fromUtf8(plain);
}

}
