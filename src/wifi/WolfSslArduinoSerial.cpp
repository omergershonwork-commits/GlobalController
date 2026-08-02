#include <Arduino.h>

extern "C" int wolfSSL_Arduino_Serial_Print(const char* const message) {
    if (message != nullptr) {
        Serial.println(message);
    }

    return 0;
}
