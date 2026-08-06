#include "pairing/PairingManager.h"
#include "utils.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {
constexpr std::size_t kMaximumFrameBytes = 4096;
constexpr unsigned long kWriteTimeoutMs = 3000;
constexpr unsigned long kWriteRetryDelayMs = 2;

enum class VarintDecodeResult : std::uint8_t {
    Complete,
    Incomplete,
    Invalid,
};

std::size_t encodeVarint(std::uint32_t value, std::uint8_t* output) {
    std::size_t length = 0;
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7FU);
        value >>= 7U;
        if (value != 0) {
            byte = static_cast<std::uint8_t>(byte | 0x80U);
        }
        output[length++] = byte;
    } while (value != 0 && length < 5);
    return length;
}

VarintDecodeResult decodeVarint(
    const std::vector<std::uint8_t>& data,
    std::uint32_t& value,
    std::size_t& prefixLength
) {
    value = 0;
    prefixLength = 0;

    const std::size_t maximum = data.size() < 5 ? data.size() : 5;
    for (std::size_t index = 0; index < maximum; ++index) {
        const std::uint8_t byte = data[index];
        value |= static_cast<std::uint32_t>(byte & 0x7FU)
            << static_cast<std::uint32_t>(7U * index);
        prefixLength = index + 1;

        if ((byte & 0x80U) == 0) {
            return VarintDecodeResult::Complete;
        }
    }

    return data.size() < 5
        ? VarintDecodeResult::Incomplete
        : VarintDecodeResult::Invalid;
}

bool writeFully(
    const std::uint8_t* data,
    std::size_t length,
    const char* phase
) {
    if (data == nullptr || length == 0) {
        Serial.printf("[PAIR ERROR]: %s produced an empty frame\n", phase);
        return false;
    }

    std::size_t offset = 0;
    unsigned long lastProgressMs = millis();

    while (offset < length) {
        if (!ssl_connected()) {
            Serial.printf(
                "[PAIR ERROR]: TLS closed while sending %s at %u/%u bytes\n",
                phase,
                static_cast<unsigned int>(offset),
                static_cast<unsigned int>(length)
            );
            return false;
        }

        const int remaining = static_cast<int>(length - offset);
        const std::uint8_t sent = ssl_send(
            reinterpret_cast<const char*>(data + offset),
            remaining
        );

        if (sent > 0) {
            offset += sent;
            lastProgressMs = millis();
            continue;
        }

        if (millis() - lastProgressMs >= kWriteTimeoutMs) {
            Serial.printf(
                "[PAIR ERROR]: timed out sending %s at %u/%u bytes\n",
                phase,
                static_cast<unsigned int>(offset),
                static_cast<unsigned int>(length)
            );
            return false;
        }

        delay(kWriteRetryDelayMs);
    }

    Serial.printf(
        "[PAIR TX]: %s bytes=%u\n",
        phase,
        static_cast<unsigned int>(length)
    );
    return true;
}

bool sendMessage(Pairing__PairingMessage& message, const char* phase) {
    const std::size_t payloadLength =
        pairing__pairing_message__get_packed_size(&message);
    if (payloadLength == 0 || payloadLength > kMaximumFrameBytes) {
        Serial.printf(
            "[PAIR ERROR]: invalid %s payload length=%u\n",
            phase,
            static_cast<unsigned int>(payloadLength)
        );
        return false;
    }

    std::uint8_t prefix[5] = {};
    const std::size_t prefixLength = encodeVarint(
        static_cast<std::uint32_t>(payloadLength),
        prefix
    );

    std::vector<std::uint8_t> frame(prefixLength + payloadLength);
    for (std::size_t index = 0; index < prefixLength; ++index) {
        frame[index] = prefix[index];
    }

    const std::size_t packedLength = pairing__pairing_message__pack(
        &message,
        frame.data() + prefixLength
    );
    if (packedLength != payloadLength) {
        Serial.printf(
            "[PAIR ERROR]: %s packed %u bytes instead of %u\n",
            phase,
            static_cast<unsigned int>(packedLength),
            static_cast<unsigned int>(payloadLength)
        );
        return false;
    }

    return writeFully(frame.data(), frame.size(), phase);
}

bool sendPairingRequest(const char* clientName) {
    Pairing__PairingMessage message = PAIRING__PAIRING_MESSAGE__INIT;
    Pairing__PairingRequest request = PAIRING__PAIRING_REQUEST__INIT;

    request.service_name = const_cast<char*>("atvremote");
    request.client_name = const_cast<char*>(
        clientName == nullptr || clientName[0] == '\0'
            ? "GlobalController"
            : clientName
    );

    message.protocol_version = 2;
    message.status = PAIRING__PAIRING_MESSAGE__STATUS__STATUS_OK;
    message.pairing_request = &request;

    return sendMessage(message, "pairing_request");
}

bool sendPairingOptions() {
    Pairing__PairingMessage message = PAIRING__PAIRING_MESSAGE__INIT;
    Pairing__PairingOption options = PAIRING__PAIRING_OPTION__INIT;
    Pairing__PairingEncoding encoding = PAIRING__PAIRING_ENCODING__INIT;
    Pairing__PairingEncoding* encodings[] = {&encoding};

    encoding.type =
        PAIRING__PAIRING_ENCODING__ENCODING_TYPE__ENCODING_TYPE_HEXADECIMAL;
    encoding.symbol_length = 6;

    options.preferred_role = PAIRING__ROLE_TYPE__ROLE_TYPE_INPUT;
    options.input_encodings = encodings;
    options.n_input_encodings = 1;

    message.protocol_version = 2;
    message.status = PAIRING__PAIRING_MESSAGE__STATUS__STATUS_OK;
    message.pairing_option = &options;

    return sendMessage(message, "pairing_options");
}

bool sendPairingConfiguration() {
    Pairing__PairingMessage message = PAIRING__PAIRING_MESSAGE__INIT;
    Pairing__PairingConfiguration configuration =
        PAIRING__PAIRING_CONFIGURATION__INIT;
    Pairing__PairingEncoding encoding = PAIRING__PAIRING_ENCODING__INIT;

    encoding.type =
        PAIRING__PAIRING_ENCODING__ENCODING_TYPE__ENCODING_TYPE_HEXADECIMAL;
    encoding.symbol_length = 6;

    configuration.client_role = PAIRING__ROLE_TYPE__ROLE_TYPE_INPUT;
    configuration.encoding = &encoding;

    message.protocol_version = 2;
    message.status = PAIRING__PAIRING_MESSAGE__STATUS__STATUS_OK;
    message.pairing_configuration = &configuration;

    return sendMessage(message, "pairing_configuration");
}

bool sendPairingSecret(const std::uint8_t* secret) {
    Pairing__PairingMessage message = PAIRING__PAIRING_MESSAGE__INIT;
    Pairing__PairingSecret secretMessage = PAIRING__PAIRING_SECRET__INIT;

    secretMessage.secret.data = const_cast<std::uint8_t*>(secret);
    secretMessage.secret.len = 32;

    message.protocol_version = 2;
    message.status = PAIRING__PAIRING_MESSAGE__STATUS__STATUS_OK;
    message.pairing_secret = &secretMessage;

    return sendMessage(message, "pairing_secret");
}

bool addModulusAndExponent(WOLFSSL_X509* certificate, Sha256& sha256) {
    if (certificate == nullptr) {
        Serial.println("[PAIR ERROR]: certificate is unavailable");
        return false;
    }

    WOLFSSL_EVP_PKEY* publicKey = wolfSSL_X509_get_pubkey(certificate);
    if (publicKey == nullptr) {
        Serial.println("[PAIR ERROR]: unable to read certificate public key");
        return false;
    }

    if (wolfSSL_EVP_PKEY_id(publicKey) != EVP_PKEY_RSA) {
        Serial.println("[PAIR ERROR]: certificate public key is not RSA");
        wolfSSL_EVP_PKEY_free(publicKey);
        return false;
    }

    WOLFSSL_RSA* rsaKey = publicKey->rsa;
    if (
        rsaKey == nullptr ||
        rsaKey->n == nullptr ||
        rsaKey->e == nullptr
    ) {
        Serial.println("[PAIR ERROR]: incomplete RSA public key");
        wolfSSL_EVP_PKEY_free(publicKey);
        return false;
    }

    const int modulusBytesLength = mp_unsigned_bin_size(
        reinterpret_cast<mp_int*>(rsaKey->n->internal)
    );
    const int exponentBytesLength = mp_unsigned_bin_size(
        reinterpret_cast<mp_int*>(rsaKey->e->internal)
    );

    if (modulusBytesLength <= 0 || exponentBytesLength <= 0) {
        Serial.println("[PAIR ERROR]: invalid RSA modulus or exponent length");
        wolfSSL_EVP_PKEY_free(publicKey);
        return false;
    }

    std::vector<std::uint8_t> modulus(
        static_cast<std::size_t>(modulusBytesLength)
    );
    std::vector<std::uint8_t> exponent(
        static_cast<std::size_t>(exponentBytesLength)
    );

    if (
        mp_to_unsigned_bin(
            reinterpret_cast<mp_int*>(rsaKey->n->internal),
            modulus.data()
        ) != MP_OKAY ||
        mp_to_unsigned_bin(
            reinterpret_cast<mp_int*>(rsaKey->e->internal),
            exponent.data()
        ) != MP_OKAY
    ) {
        Serial.println("[PAIR ERROR]: unable to encode RSA public key");
        wolfSSL_EVP_PKEY_free(publicKey);
        return false;
    }

    const int modulusResult = wc_Sha256Update(
        &sha256,
        modulus.data(),
        static_cast<word32>(modulus.size())
    );
    const int exponentResult = wc_Sha256Update(
        &sha256,
        exponent.data(),
        static_cast<word32>(exponent.size())
    );

    wolfSSL_EVP_PKEY_free(publicKey);
    return modulusResult == 0 && exponentResult == 0;
}
}  // namespace

bool PairingManager::sendCode(const String& code) {
    if (code.length() != 6) {
        Serial.println("[PAIR ERROR]: pairing code must contain 6 characters");
        return false;
    }

    WOLFSSL_X509* serverCertificate = ssl_get_peer_certificate();
    WOLFSSL_X509* clientCertificate = ssl_get_certificate();
    if (serverCertificate == nullptr || clientCertificate == nullptr) {
        Serial.println("[PAIR ERROR]: pairing certificates are unavailable");
        return false;
    }

    std::uint8_t codeBytes[3] = {};
    for (std::size_t index = 0; index < 6; index += 2) {
        char pair[3] = {code[index], code[index + 1], '\0'};
        char* end = nullptr;
        const long parsed = std::strtol(pair, &end, 16);
        if (end == pair || *end != '\0' || parsed < 0 || parsed > 255) {
            Serial.println("[PAIR ERROR]: pairing code is not hexadecimal");
            return false;
        }
        codeBytes[index / 2] = static_cast<std::uint8_t>(parsed);
    }

    Sha256 sha256[1];
    if (wc_InitSha256(sha256) != 0) {
        Serial.println("[PAIR ERROR]: SHA-256 initialization failed");
        return false;
    }

    if (
        !addModulusAndExponent(clientCertificate, *sha256) ||
        !addModulusAndExponent(serverCertificate, *sha256) ||
        wc_Sha256Update(sha256, codeBytes + 1, 2) != 0
    ) {
        Serial.println("[PAIR ERROR]: unable to build pairing secret");
        return false;
    }

    std::uint8_t hash[32] = {};
    if (wc_Sha256Final(sha256, hash) != 0) {
        Serial.println("[PAIR ERROR]: SHA-256 finalization failed");
        return false;
    }

    if (hash[0] != codeBytes[0]) {
        Serial.println("[PAIR ERROR]: pairing code checksum failed");
        return false;
    }

    Serial.println("[PAIR]: pairing code validated locally; sending secret");
    return sendPairingSecret(hash);
}

bool PairingManager::connected() {
    return ssl_connected();
}

void PairingManager::begin(
    IPAddress host,
    std::uint16_t port,
    char* clientName
) {
    chunks.clear();
    isSecure = false;

    if (ssl_connect(host, port) < 0) {
        Serial.println("[PAIR ERROR]: TLS connection failed");
        return;
    }

    Serial.printf(
        "[PAIR]: TLS connected to %s:%u\n",
        host.toString().c_str(),
        port
    );

    if (!sendPairingRequest(clientName)) {
        Serial.println("[PAIR ERROR]: initial pairing request was not sent");
        ssl_stop();
        return;
    }

    Serial.println("[PAIR]: waiting for pairing_request_ack");
}

void PairingManager::loop() {
    std::uint8_t buffer[512] = {};

    for (std::uint8_t readAttempt = 0; readAttempt < 4; ++readAttempt) {
        if (ssl_available() <= 0) {
            break;
        }

        const int length = ssl_read(
            reinterpret_cast<char*>(buffer),
            sizeof(buffer)
        );
        if (length < 0) {
            Serial.println("[PAIR ERROR]: TLS read failed");
            ssl_stop();
            chunks.clear();
            return;
        }
        if (length == 0) {
            break;
        }

        if (chunks.size() + static_cast<std::size_t>(length) > kMaximumFrameBytes) {
            Serial.println("[PAIR ERROR]: pairing frame buffer exceeded safety limit");
            ssl_stop();
            chunks.clear();
            return;
        }

        chunks.insert(chunks.end(), buffer, buffer + length);
        Serial.printf(
            "[PAIR RX]: chunk=%d buffered=%u\n",
            length,
            static_cast<unsigned int>(chunks.size())
        );
    }

    while (!chunks.empty()) {
        std::uint32_t payloadLength = 0;
        std::size_t prefixLength = 0;
        const VarintDecodeResult decoded = decodeVarint(
            chunks,
            payloadLength,
            prefixLength
        );

        if (decoded == VarintDecodeResult::Incomplete) {
            return;
        }
        if (
            decoded == VarintDecodeResult::Invalid ||
            payloadLength == 0 ||
            payloadLength > kMaximumFrameBytes
        ) {
            Serial.println("[PAIR ERROR]: invalid protobuf length prefix");
            ssl_stop();
            chunks.clear();
            return;
        }

        const std::size_t frameLength =
            prefixLength + static_cast<std::size_t>(payloadLength);
        if (chunks.size() < frameLength) {
            return;
        }

        Pairing__PairingMessage* response =
            pairing__pairing_message__unpack(
                nullptr,
                payloadLength,
                chunks.data() + prefixLength
            );
        if (response == nullptr) {
            Serial.println("[PAIR ERROR]: unable to decode pairing protobuf");
            ssl_stop();
            chunks.clear();
            return;
        }

        Serial.printf(
            "[PAIR RX]: message bytes=%u status=%d protocol=%ld\n",
            static_cast<unsigned int>(payloadLength),
            static_cast<int>(response->status),
            static_cast<long>(response->protocol_version)
        );

        handleResponse(response);
        pairing__pairing_message__free_unpacked(response, nullptr);
        chunks.erase(chunks.begin(), chunks.begin() + frameLength);

        if (!ssl_connected() && !isSecure) {
            chunks.clear();
            return;
        }
    }
}

void PairingManager::handleResponse(Pairing__PairingMessage* message) {
    if (message == nullptr) {
        Serial.println("[PAIR ERROR]: null pairing response");
        return;
    }

    if (
        message->status !=
        PAIRING__PAIRING_MESSAGE__STATUS__STATUS_OK
    ) {
        Serial.printf(
            "[PAIR ERROR]: television returned pairing status=%d\n",
            static_cast<int>(message->status)
        );
        ssl_stop();
        isSecure = false;
        return;
    }

    if (message->pairing_request_ack != nullptr) {
        Serial.printf(
            "[PAIR]: pairing_request_ack server=%s\n",
            message->pairing_request_ack->server_name == nullptr
                ? "unknown"
                : message->pairing_request_ack->server_name
        );
        if (!sendPairingOptions()) {
            Serial.println("[PAIR ERROR]: pairing options were not sent");
            ssl_stop();
            return;
        }
        Serial.println("[PAIR]: waiting for pairing options response");
        return;
    }

    if (message->pairing_option != nullptr) {
        Serial.println("[PAIR]: television returned pairing options");
        if (!sendPairingConfiguration()) {
            Serial.println("[PAIR ERROR]: pairing configuration was not sent");
            ssl_stop();
            return;
        }
        Serial.println("[PAIR]: waiting for configuration_ack");
        return;
    }

    if (message->pairing_configuration_ack != nullptr) {
        isSecure = true;
        Serial.println(
            "[PAIR]: configuration_ack received; TV should display the 6-character code now"
        );
        return;
    }

    if (message->pairing_secret_ack != nullptr) {
        isSecure = false;
        Serial.println("[PAIR]: pairing_secret_ack received; pairing completed");
        ssl_stop();
        return;
    }

    Serial.println("[PAIR ERROR]: television returned an unsupported pairing message");
    ssl_stop();
    isSecure = false;
}
