#include "security/CredentialStore.hpp"

#include <limits>

#ifdef _WIN32
#include <Windows.h>
#include <wincred.h>
#elif defined(__APPLE__)
#include <Security/Security.h>
#else
#include <QProcess>
#include <QStandardPaths>
#endif

namespace yaap {
namespace {

[[nodiscard]] bool validKey(const QString& key, QString& error)
{
    if (key.trimmed().isEmpty()) {
        error = "Credential key cannot be empty.";
        return false;
    }
    return true;
}

#ifdef _WIN32

class WindowsCredentialStore final : public CredentialStore {
public:
    bool save(const QString& key, const QByteArray& secret, QString& error) override
    {
        if (!validKey(key, error)
            || secret.size() > static_cast<qsizetype>(CRED_MAX_CREDENTIAL_BLOB_SIZE)) {
            if (error.isEmpty()) {
                error = "Credential exceeds the Windows Credential Manager size limit.";
            }
            return false;
        }
        const auto target = QStringLiteral("Yaap/") + key;
        auto targetStorage = target.toStdWString();
        CREDENTIALW credential{};
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = targetStorage.data();
        credential.CredentialBlobSize = static_cast<DWORD>(secret.size());
        credential.CredentialBlob = reinterpret_cast<BYTE*>(const_cast<char*>(secret.constData()));
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        credential.UserName = const_cast<wchar_t*>(L"Yaap");
        if (!CredWriteW(&credential, 0)) {
            error = "Windows Credential Manager could not store the credential (error "
                + QString::number(GetLastError()) + ").";
            return false;
        }
        return true;
    }

    [[nodiscard]] std::optional<QByteArray> load(
        const QString& key,
        QString& error) const override
    {
        if (!validKey(key, error)) {
            return std::nullopt;
        }
        const auto targetStorage = (QStringLiteral("Yaap/") + key).toStdWString();
        PCREDENTIALW credential{};
        if (!CredReadW(targetStorage.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
            const auto code = GetLastError();
            if (code != ERROR_NOT_FOUND) {
                error = "Windows Credential Manager could not read the credential (error "
                    + QString::number(code) + ").";
            }
            return std::nullopt;
        }
        const QByteArray result{
            reinterpret_cast<const char*>(credential->CredentialBlob),
            static_cast<qsizetype>(credential->CredentialBlobSize)};
        CredFree(credential);
        return result;
    }

    bool remove(const QString& key, QString& error) override
    {
        if (!validKey(key, error)) {
            return false;
        }
        const auto targetStorage = (QStringLiteral("Yaap/") + key).toStdWString();
        if (!CredDeleteW(targetStorage.c_str(), CRED_TYPE_GENERIC, 0)) {
            const auto code = GetLastError();
            if (code != ERROR_NOT_FOUND) {
                error = "Windows Credential Manager could not remove the credential (error "
                    + QString::number(code) + ").";
                return false;
            }
        }
        return true;
    }
};

#elif defined(__APPLE__)

class MacCredentialStore final : public CredentialStore {
public:
    bool save(const QString& key, const QByteArray& secret, QString& error) override
    {
        if (!validKey(key, error)) {
            return false;
        }
        const auto service = QByteArray{"Yaap"};
        const auto account = key.toUtf8();
        SecKeychainItemRef item{};
        void* existing{};
        UInt32 existingLength{};
        auto status = SecKeychainFindGenericPassword(
            nullptr, static_cast<UInt32>(service.size()), service.constData(),
            static_cast<UInt32>(account.size()), account.constData(),
            &existingLength, &existing, &item);
        if (status == errSecSuccess) {
            SecKeychainItemFreeContent(nullptr, existing);
            status = SecKeychainItemModifyAttributesAndData(
                item, nullptr, static_cast<UInt32>(secret.size()), secret.constData());
            CFRelease(item);
        } else if (status == errSecItemNotFound) {
            status = SecKeychainAddGenericPassword(
                nullptr, static_cast<UInt32>(service.size()), service.constData(),
                static_cast<UInt32>(account.size()), account.constData(),
                static_cast<UInt32>(secret.size()), secret.constData(), nullptr);
        }
        if (status != errSecSuccess) {
            error = "macOS Keychain could not store the credential (status "
                + QString::number(status) + ").";
            return false;
        }
        return true;
    }

    [[nodiscard]] std::optional<QByteArray> load(
        const QString& key,
        QString& error) const override
    {
        if (!validKey(key, error)) {
            return std::nullopt;
        }
        const auto account = key.toUtf8();
        UInt32 length{};
        void* data{};
        const auto status = SecKeychainFindGenericPassword(
            nullptr, 4, "Yaap", static_cast<UInt32>(account.size()), account.constData(),
            &length, &data, nullptr);
        if (status == errSecItemNotFound) {
            return std::nullopt;
        }
        if (status != errSecSuccess) {
            error = "macOS Keychain could not read the credential (status "
                + QString::number(status) + ").";
            return std::nullopt;
        }
        const QByteArray result{static_cast<const char*>(data), static_cast<qsizetype>(length)};
        SecKeychainItemFreeContent(nullptr, data);
        return result;
    }

    bool remove(const QString& key, QString& error) override
    {
        if (!validKey(key, error)) {
            return false;
        }
        const auto account = key.toUtf8();
        SecKeychainItemRef item{};
        const auto status = SecKeychainFindGenericPassword(
            nullptr, 4, "Yaap", static_cast<UInt32>(account.size()), account.constData(),
            nullptr, nullptr, &item);
        if (status == errSecItemNotFound) {
            return true;
        }
        if (status != errSecSuccess || SecKeychainItemDelete(item) != errSecSuccess) {
            error = "macOS Keychain could not remove the credential.";
            if (item != nullptr) {
                CFRelease(item);
            }
            return false;
        }
        CFRelease(item);
        return true;
    }
};

#else

class SecretServiceCredentialStore final : public CredentialStore {
public:
    bool save(const QString& key, const QByteArray& secret, QString& error) override
    {
        if (!validKey(key, error)) {
            return false;
        }
        // PROTOTYPE: secret-tool is a small adapter to the desktop Secret Service.
        // Replace it with a direct libsecret/Secret Service backend for packaging.
        QProcess process;
        process.start(tool(), {"store", "--label=Yaap", "application", "Yaap", "key", key});
        if (!process.waitForStarted(5'000)) {
            error = "Could not start secret-tool. Install libsecret tools.";
            return false;
        }
        process.write(secret);
        process.closeWriteChannel();
        if (!process.waitForFinished(15'000) || process.exitCode() != 0) {
            error = "Secret Service could not store the credential: "
                + QString::fromUtf8(process.readAllStandardError()).trimmed();
            return false;
        }
        return true;
    }

    [[nodiscard]] std::optional<QByteArray> load(
        const QString& key,
        QString& error) const override
    {
        if (!validKey(key, error)) {
            return std::nullopt;
        }
        QProcess process;
        process.start(tool(), {"lookup", "application", "Yaap", "key", key});
        if (!process.waitForFinished(10'000)) {
            error = "Secret Service credential lookup timed out.";
            return std::nullopt;
        }
        if (process.exitCode() != 0) {
            return std::nullopt;
        }
        auto secret = process.readAllStandardOutput();
        while (secret.endsWith('\n') || secret.endsWith('\r')) {
            secret.chop(1);
        }
        return secret;
    }

    bool remove(const QString& key, QString& error) override
    {
        if (!validKey(key, error)) {
            return false;
        }
        QProcess process;
        process.start(tool(), {"clear", "application", "Yaap", "key", key});
        if (!process.waitForFinished(10'000) || process.exitCode() != 0) {
            error = "Secret Service could not remove the credential.";
            return false;
        }
        return true;
    }

private:
    [[nodiscard]] static QString tool()
    {
        return QStandardPaths::findExecutable("secret-tool");
    }
};

#endif

} // namespace

std::unique_ptr<CredentialStore> CredentialStore::createPlatformStore()
{
#ifdef _WIN32
    return std::make_unique<WindowsCredentialStore>();
#elif defined(__APPLE__)
    return std::make_unique<MacCredentialStore>();
#else
    return std::make_unique<SecretServiceCredentialStore>();
#endif
}

} // namespace yaap
