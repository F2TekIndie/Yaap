#include "provider_protocol/ProviderProtocolCodec.hpp"
#include "extension_api/ProviderProtocol.hpp"

#include <QByteArray>

#include <cstddef>
#include <cstdint>
#include <algorithm>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, const std::size_t size)
{
    yaap::ProviderProtocolCodec codec;
    const auto boundedSize = static_cast<qsizetype>(
        std::min<std::size_t>(size, yaap::provider_protocol::maximumMessageBytes + 8));
    codec.append(QByteArray{reinterpret_cast<const char*>(data), boundedSize});
    return 0;
}
