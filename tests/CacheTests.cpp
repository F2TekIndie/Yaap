#include "provider_runtime/ProviderCache.hpp"
#include <catch2/catch_test_macros.hpp>
#include <QTemporaryDir>
#include <QThread>

namespace yaap {
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

}
