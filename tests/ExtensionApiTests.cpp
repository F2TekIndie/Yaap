#include "extension_api/ApiVersion.hpp"
#include "extension_api/ModPermission.hpp"

#include <catch2/catch_test_macros.hpp>

namespace yaap {

TEST_CASE("Extension API versions parse and compare without accepting loose syntax")
{
    CHECK(extensionApiVersion == ApiVersion{1, 1});
    REQUIRE(ApiVersion::parse("1.0") == ApiVersion{1, 0});
    CHECK(ApiVersion::parse("01.0") == std::nullopt);
    CHECK(ApiVersion::parse("1") == std::nullopt);
    CHECK(ApiVersion{1, 2} < ApiVersion{2, 0});
}

TEST_CASE("Mod permission vocabulary rejects undeclared capability names")
{
    CHECK(permission::isKnown(permission::uiSlot("nowPlaying.aboveTransport")));
    CHECK(permission::isKnown(permission::providerNetworkHost("music.example")));
    CHECK_FALSE(permission::isKnown("filesystem.everything"));
}

} // namespace yaap
