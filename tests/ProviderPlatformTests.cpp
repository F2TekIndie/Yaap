#include "mods/ExtensionRegistry.hpp"
#include "mods/ModManager.hpp"
#include "mods/PermissionStore.hpp"
#include "mods/ThemeManager.hpp"
#include "provider_runtime/ProviderCache.hpp"
#include "provider_runtime/ProviderExtensionManager.hpp"
#include "provider_runtime/ProviderGateway.hpp"
#include "providers/ProviderAccountStore.hpp"
#include "security/CredentialHandleBroker.hpp"
#include "security/CredentialStore.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QSettings>

#include <functional>

namespace yaap {
namespace {

class MemoryCredentialStore final : public CredentialStore {
public:
    bool save(const QString& key, const QByteArray& secret, QString&) override
    {
        values.insert(key, secret);
        return true;
    }
    std::optional<QByteArray> load(const QString& key, QString&) const override
    {
        const auto iterator = values.constFind(key);
        return iterator == values.cend()
            ? std::nullopt : std::optional<QByteArray>{iterator.value()};
    }
    bool remove(const QString& key, QString&) override
    {
        values.remove(key);
        return true;
    }
    QHash<QString, QByteArray> values;
};

void write(const QString& path, const QByteArray& contents)
{
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QFile file{path};
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(contents) == contents.size());
}

bool spinUntil(const std::function<bool()>& condition, const int timeoutMilliseconds = 5'000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(5);
    }
    return condition();
}

} // namespace

TEST_CASE("Credential handles are provider scoped expiring and one shot")
{
    MemoryCredentialStore store;
    QString error;
    REQUIRE(store.save("secret-key", "correct horse", error));
    CredentialHandleBroker broker{store};
    const auto handle = broker.issue("account-a", "provider-a", "secret-key", 60);
    REQUIRE_FALSE(handle.isEmpty());
    CHECK_FALSE(broker.consume(handle, "account-a", "provider-b", error));
    error.clear();
    const auto secret = broker.consume(handle, "account-a", "provider-a", error);
    REQUIRE(secret);
    CHECK(*secret == "correct horse");
    CHECK_FALSE(broker.consume(handle, "account-a", "provider-a", error));
}

TEST_CASE("Provider accounts persist metadata separately from platform credentials")
{
    QSettings{}.remove("providers/accounts-v1");
    MemoryCredentialStore store;
    CredentialHandleBroker broker{store};
    ProviderAccountStore accounts{store, broker};
    REQUIRE(accounts.addAccount("opensubsonic", "Home", "https://music.example",
        "listener", "not-in-settings"));
    REQUIRE(accounts.count() == 1);
    const auto accountId = accounts.data(accounts.index(0),
        ProviderAccountStore::AccountIdRole).toString();
    CHECK_FALSE(accountId.isEmpty());
    const auto persisted = QSettings{}.value("providers/accounts-v1").toByteArray();
    CHECK_FALSE(persisted.contains("not-in-settings"));
    CHECK(persisted.contains("listener"));
    const auto handle = accounts.issueCredentialHandle(accountId, "opensubsonic");
    REQUIRE_FALSE(handle.isEmpty());
    QString error;
    const auto secret = broker.consume(handle, accountId, "opensubsonic", error);
    REQUIRE(secret);
    CHECK(*secret == "not-in-settings");
    REQUIRE(accounts.removeAccount(accountId));
    CHECK(accounts.count() == 0);
    CHECK(store.values.isEmpty());
}

TEST_CASE("Provider cache is namespaced bounded and supports stale offline reads")
{
    QTemporaryDir directory;
    ProviderCache cache;
    QString error;
    REQUIRE(cache.open(directory.filePath("providers.sqlite3"), error));
    REQUIRE(cache.put("account-a", "search:jazz", "{\"items\":[]}", "etag-a", 1, error));
    REQUIRE(cache.get("account-a", "search:jazz", false, error));
    QThread::msleep(1'100);
    CHECK_FALSE(cache.get("account-a", "search:jazz", false, error));
    const auto stale = cache.get("account-a", "search:jazz", true, error);
    REQUIRE(stale);
    CHECK(stale->stale);
    CHECK(cache.namespaces(error) == QStringList{"account-a"});
    REQUIRE(cache.prune(0, error));
    CHECK_FALSE(cache.get("account-a", "search:jazz", true, error));
}

TEST_CASE("Sample provider searches resolves and produces playable local media through gateway")
{
    QTemporaryDir directory;
    const auto packageRoot = directory.filePath("mods/org.yaap.sample-provider");
#ifdef _WIN32
    const auto platform = QStringLiteral("windows-x64");
    const auto relativeExecutable = QStringLiteral("bin/windows-x64/YaapSampleProvider.exe");
#elif defined(__APPLE__)
    const auto platform = QStringLiteral("macos-x64");
    const auto relativeExecutable = QStringLiteral("bin/macos-x64/YaapSampleProvider");
#else
    const auto platform = QStringLiteral("linux-x64");
    const auto relativeExecutable = QStringLiteral("bin/linux-x64/YaapSampleProvider");
#endif
    const auto executablePath = packageRoot + '/' + relativeExecutable;
    QDir{}.mkpath(QFileInfo{executablePath}.absolutePath());
    REQUIRE(QFile::copy(QString::fromUtf8(YAAP_SAMPLE_PROVIDER_PATH), executablePath));
    auto manifest = QByteArray{R"json({
      "schemaVersion":1,
      "id":"org.yaap.sample-provider",
      "name":"Sample Provider",
      "version":"1.0.0",
      "api":{"minimum":"1.0","maximumExclusive":"2.0"},
      "kind":["provider"],
      "permissions":[],
      "provider":{"id":"org.yaap.sample-provider","executables":{
        "@PLATFORM@":"@EXECUTABLE@"}}
    })json"};
    manifest.replace("@PLATFORM@", platform.toUtf8());
    manifest.replace("@EXECUTABLE@", relativeExecutable.toUtf8());
    write(packageRoot + "/manifest.json", manifest);

    PermissionStore permissions;
    ThemeManager themes;
    ExtensionRegistry extensions{permissions};
    ModManager mods{permissions, themes, extensions, {directory.filePath("mods")}};
    REQUIRE(mods.grantDeclared("org.yaap.sample-provider"));
    REQUIRE(mods.setEnabled("org.yaap.sample-provider", true));
    MemoryCredentialStore credentialStore;
    CredentialHandleBroker handles{credentialStore};
    ProviderAccountStore accounts{credentialStore, handles};
    ProviderExtensionManager providerExtensions{mods, permissions, accounts, handles};
    REQUIRE(spinUntil([&] { return !providerExtensions.readyProviderIds().isEmpty(); }));

    ProviderCache cache;
    QString error;
    REQUIRE(cache.open(directory.filePath("cache.sqlite3"), error));
    ProviderGateway gateway{providerExtensions, cache, accounts};
    gateway.search("gateway");
    REQUIRE(spinUntil([&] { return !gateway.loading() && gateway.count() == 1; }));
    CHECK(gateway.data(gateway.index(0), ProviderGateway::TitleRole).toString()
        == "Result for gateway");

    QUrl playbackUrl;
    QObject::connect(&gateway, &ProviderGateway::playbackRequested,
        [&playbackUrl](const QUrl& url, const QString&) { playbackUrl = url; });
    gateway.play(0);
    REQUIRE(spinUntil([&] { return playbackUrl.isValid(); }));
    CHECK(playbackUrl.isLocalFile());
    CHECK(QFileInfo::exists(playbackUrl.toLocalFile()));
}

} // namespace yaap
