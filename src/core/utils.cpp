#include <string>

#include <openssl/evp.h>
#include <vector>
#include "utils.h"

std::string decodeBase64(const std::string& b64) {
    std::string result;
    std::vector<unsigned char> buffer((b64.size() * 3) / 4 + 1);

    const int len = EVP_DecodeBlock(buffer.data(),
                              reinterpret_cast<const unsigned char*>(b64.data()),
                              static_cast<int>(b64.size()));

    if (len > 0) {
        result.assign(buffer.begin(), buffer.begin() + len);
    }
    return result;
}
