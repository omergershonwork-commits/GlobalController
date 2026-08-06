#include "RemoteClient.h"
#include "certificate/CertificateGenerator.h"

#include <Arduino.h>
#include <WiFiClient.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

WiFiClient client;

namespace {
constexpr unsigned long kTlsHandshakeTimeoutMs = 15000;
constexpr unsigned long kTlsRetryDelayMs = 2;

WOLFSSL_CTX* ctx = nullptr;
WOLFSSL* ssl = nullptr;
char wolfSslErrorMessage[81] = {};

void copyText(char* destination, std::size_t capacity, const char* source) {
    if (destination == nullptr || capacity == 0) {
        return;
    }

    const char* safeSource = source == nullptr ? "" : source;
    std::strncpy(destination, safeSource, capacity - 1);
    destination[capacity - 1] = '\0';
}

bool looksLikeMacAddress(const char* value) {
    if (value == nullptr || std::strlen(value) != 17) {
        return false;
    }

    for (std::size_t index = 0; index < 17; ++index) {
        if ((index + 1) % 3 == 0) {
            if (value[index] != ':') {
                return false;
            }
            continue;
        }

        const char character = value[index];
        const bool digit = character >= '0' && character <= '9';
        const bool lowerHex = character >= 'a' && character <= 'f';
        const bool upperHex = character >= 'A' && character <= 'F';
        if (!digit && !lowerHex && !upperHex) {
            return false;
        }
    }

    return true;
}

void parseCommonName(
    const char* commonName,
    char* deviceName,
    std::size_t deviceNameCapacity,
    char* macAddress,
    std::size_t macAddressCapacity
) {
    copyText(deviceName, deviceNameCapacity, "Android TV");
    copyText(macAddress, macAddressCapacity, "");

    if (commonName == nullptr || commonName[0] == '\0') {
        return;
    }

    char parts[160];
    copyText(parts, sizeof(parts), commonName);

    char* finalSeparator = std::strrchr(parts, '/');
    if (finalSeparator == nullptr) {
        copyText(deviceName, deviceNameCapacity, parts);
        return;
    }

    const char* finalPart = finalSeparator + 1;
    if (looksLikeMacAddress(finalPart)) {
        copyText(macAddress, macAddressCapacity, finalPart);
    }

    *finalSeparator = '\0';
    char* nameSeparator = std::strrchr(parts, '/');
    const char* namePart = nameSeparator == nullptr ? parts : nameSeparator + 1;
    if (
        namePart[0] != '\0' &&
        std::strcmp(namePart, "atvremote") != 0
    ) {
        copyText(deviceName, deviceNameCapacity, namePart);
    }
}

int EthernetSend(WOLFSSL* sslObject, char* message, int size, void* context) {
    (void)sslObject;
    (void)context;

    if (!client.connected()) {
        return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    }

    const std::size_t sent = client.write(
        reinterpret_cast<const std::uint8_t*>(message),
        static_cast<std::size_t>(size)
    );

    if (sent == 0) {
        return WOLFSSL_CBIO_ERR_WANT_WRITE;
    }

    return static_cast<int>(sent);
}

int EthernetReceive(WOLFSSL* sslObject, char* reply, int size, void* context) {
    (void)sslObject;
    (void)context;

    if (!client.connected()) {
        return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    }

    const int available = client.available();
    if (available <= 0) {
        return WOLFSSL_CBIO_ERR_WANT_READ;
    }

    const int requested = available < size ? available : size;
    const int received = client.read(
        reinterpret_cast<std::uint8_t*>(reply),
        requested
    );

    if (received < 0) {
        return WOLFSSL_CBIO_ERR_GENERAL;
    }
    if (received == 0) {
        return WOLFSSL_CBIO_ERR_WANT_READ;
    }

    return received;
}

void clearTlsObjects() {
    if (ssl != nullptr) {
        wolfSSL_free(ssl);
        ssl = nullptr;
    }

    if (ctx != nullptr) {
        wolfSSL_CTX_free(ctx);
        ctx = nullptr;
    }
}

void logWolfSslError(const char* operation, int error) {
    wc_ErrorString(error, wolfSslErrorMessage);
    Serial.printf(
        "[TLS ERROR]: %s failed: code=%d message=%s\n",
        operation,
        error,
        wolfSslErrorMessage
    );
}
}  // namespace

int setup_wolfssl() {
    const int initResult = wolfSSL_Init();
    if (initResult != WOLFSSL_SUCCESS) {
        logWolfSslError("wolfSSL_Init", initResult);
        return -1;
    }

    // Android TV devices in the field do not all negotiate the same TLS
    // version. Use wolfSSL's version-flexible client method rather than
    // forcing TLS 1.3, matching modern Android TV Remote implementations.
    WOLFSSL_METHOD* method = wolfSSLv23_client_method();
    if (method == nullptr) {
        Serial.println("[TLS ERROR]: unable to create version-flexible TLS client method");
        return -1;
    }

    ctx = wolfSSL_CTX_new(method);
    if (ctx == nullptr) {
        Serial.println("[TLS ERROR]: unable to allocate wolfSSL context");
        return -1;
    }

    wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_NONE, nullptr);
    wolfSSL_SetIOSend(ctx, EthernetSend);
    wolfSSL_SetIORecv(ctx, EthernetReceive);
    return 1;
}

int setup_certificates() {
    if (ctx == nullptr) {
        Serial.println("[TLS ERROR]: certificate setup requested without a TLS context");
        return -1;
    }

    int result = wolfSSL_CTX_use_certificate_buffer(
        ctx,
        client_cert_der,
        client_cert_der_len,
        WOLFSSL_FILETYPE_ASN1
    );
    if (result != WOLFSSL_SUCCESS) {
        logWolfSslError("loading client certificate", result);
        return -1;
    }

    result = wolfSSL_CTX_use_PrivateKey_buffer(
        ctx,
        client_key_der,
        client_key_der_len,
        WOLFSSL_FILETYPE_ASN1
    );
    if (result != WOLFSSL_SUCCESS) {
        logWolfSslError("loading client private key", result);
        return -1;
    }

    return 1;
}

int error_check(int result, const __FlashStringHelper* operation) {
    if (result == WOLFSSL_SUCCESS) {
        return 1;
    }

    Serial.print("[TLS ERROR]: ");
    Serial.println(operation);
    logWolfSslError("wolfSSL operation", result);
    return -1;
}

int error_check_ssl(
    WOLFSSL* sslObject,
    int result,
    const __FlashStringHelper* operation
) {
    if (sslObject == nullptr) {
        Serial.print("[TLS ERROR]: null TLS object during ");
        Serial.println(operation);
        return -1;
    }

    const int error = wolfSSL_get_error(sslObject, result);
    if (result == WOLFSSL_SUCCESS) {
        return WOLFSSL_SUCCESS;
    }

    Serial.print("[TLS ERROR]: ");
    Serial.println(operation);
    logWolfSslError("wolfSSL operation", error);
    return error;
}

int ssl_connect(IPAddress ip, std::uint16_t port) {
    ssl_stop();

    Serial.printf(
        "[TLS]: connecting to %s:%u free-heap=%u\n",
        ip.toString().c_str(),
        port,
        static_cast<unsigned int>(ESP.getFreeHeap())
    );

    if (setup_wolfssl() < 0) {
        ssl_stop();
        return -1;
    }

    if (setup_certificates() < 0) {
        ssl_stop();
        return -1;
    }

    if (!client.connect(ip, port)) {
        Serial.println("[TLS ERROR]: TCP connection failed");
        ssl_stop();
        return -1;
    }

    ssl = wolfSSL_new(ctx);
    if (ssl == nullptr) {
        Serial.printf(
            "[TLS ERROR]: unable to allocate TLS session; free-heap=%u\n",
            static_cast<unsigned int>(ESP.getFreeHeap())
        );
        ssl_stop();
        return -1;
    }

    const unsigned long handshakeStartedMs = millis();
    for (;;) {
        const int result = wolfSSL_connect(ssl);
        if (result == WOLFSSL_SUCCESS) {
            const char* version = wolfSSL_get_version(ssl);
            const char* cipher = wolfSSL_get_cipher(ssl);
            Serial.printf(
                "[TLS]: handshake complete in %lu ms version=%s cipher=%s free-heap=%u\n",
                millis() - handshakeStartedMs,
                version == nullptr ? "unknown" : version,
                cipher == nullptr ? "unknown" : cipher,
                static_cast<unsigned int>(ESP.getFreeHeap())
            );
            return 1;
        }

        const int error = wolfSSL_get_error(ssl, result);
        if (
            error != WOLFSSL_ERROR_WANT_READ &&
            error != WOLFSSL_ERROR_WANT_WRITE &&
            error != WC_PENDING_E
        ) {
            logWolfSslError("TLS handshake", error);
            ssl_stop();
            return -1;
        }

        if (millis() - handshakeStartedMs >= kTlsHandshakeTimeoutMs) {
            Serial.printf(
                "[TLS ERROR]: handshake timed out after %lu ms\n",
                kTlsHandshakeTimeoutMs
            );
            ssl_stop();
            return -1;
        }

        delay(kTlsRetryDelayMs);
    }
}

bool ssl_get_peer_identity(
    char* deviceName,
    std::size_t deviceNameCapacity,
    char* macAddress,
    std::size_t macAddressCapacity
) {
    copyText(deviceName, deviceNameCapacity, "Android TV");
    copyText(macAddress, macAddressCapacity, "");

    if (ssl == nullptr || !client.connected()) {
        return false;
    }

    WOLFSSL_X509* certificate = wolfSSL_get_peer_certificate(ssl);
    if (certificate == nullptr) {
        Serial.println("[TLS ERROR]: Android TV did not provide a peer certificate");
        return false;
    }

    WOLFSSL_X509_NAME* subject = wolfSSL_X509_get_subject_name(certificate);
    char subjectText[256] = {};
    if (subject != nullptr) {
        wolfSSL_X509_NAME_oneline(subject, subjectText, sizeof(subjectText));
    }

    const char* commonName = wolfSSL_X509_get_subjectCN(certificate);
    parseCommonName(
        commonName,
        deviceName,
        deviceNameCapacity,
        macAddress,
        macAddressCapacity
    );

    Serial.printf(
        "[TLS]: peer identity name=%s mac=%s subject=%s\n",
        deviceName,
        macAddress[0] == '\0' ? "unknown" : macAddress,
        subjectText[0] == '\0' ? "unavailable" : subjectText
    );
    return true;
}

bool ssl_probe_identity(
    IPAddress ip,
    std::uint16_t port,
    char* deviceName,
    std::size_t deviceNameCapacity,
    char* macAddress,
    std::size_t macAddressCapacity
) {
    if (ssl_connect(ip, port) < 0) {
        return false;
    }

    const bool identified = ssl_get_peer_identity(
        deviceName,
        deviceNameCapacity,
        macAddress,
        macAddressCapacity
    );
    ssl_stop();
    return identified;
}

std::uint8_t ssl_send(const char* message, int messageSize) {
    if (ssl == nullptr || !client.connected()) {
        return 0;
    }

    const int result = wolfSSL_write(ssl, message, messageSize);
    if (result <= 0) {
        const int error = wolfSSL_get_error(ssl, result);
        if (
            error != WOLFSSL_ERROR_WANT_READ &&
            error != WOLFSSL_ERROR_WANT_WRITE
        ) {
            logWolfSslError("TLS write", error);
        }
        return 0;
    }

    return static_cast<std::uint8_t>(result > 255 ? 255 : result);
}

int ssl_available() {
    return client.connected() ? client.available() : 0;
}

int ssl_read(char* reply, int size) {
    if (ssl == nullptr || !client.connected()) {
        return -1;
    }

    const int result = wolfSSL_read(ssl, reply, size);
    if (result < 0) {
        const int error = wolfSSL_get_error(ssl, result);
        if (
            error == WOLFSSL_ERROR_WANT_READ ||
            error == WOLFSSL_ERROR_WANT_WRITE
        ) {
            return 0;
        }
        logWolfSslError("TLS read", error);
    }

    return result;
}

void ssl_stop() {
    clearTlsObjects();
    client.stop();
}

std::uint8_t ssl_connected() {
    return ssl != nullptr && client.connected();
}

WOLFSSL_X509* ssl_get_peer_certificate() {
    return ssl == nullptr ? nullptr : wolfSSL_get_peer_certificate(ssl);
}

WOLFSSL_X509* ssl_get_certificate() {
    return ssl == nullptr ? nullptr : wolfSSL_get_certificate(ssl);
}
