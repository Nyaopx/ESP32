#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <U8g2lib.h>
#include <sstream>

// BLE Configuration
const char* elmMacAddress = "00:10:CC:4F:36:03"; // Replace with your ELM327 MAC address
bool deviceConnected = false;
int connectionAttempt = 0;
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

// Display Configuration
U8G2_SSD1309_128X64_NONAME0_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/ 19, /* dc=*/ 21, /* reset=*/ 22);

// Function to display waiting messages
void displayWaiting(int attempt) {
    String message = "Waiting " + String(attempt);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor((128 - u8g2.getStrWidth(message.c_str())) / 2, 32); // Center the text
    u8g2.print(message);
    u8g2.sendBuffer();
    Serial.println("Display Waiting: " + message);
}

// Function to display connected message
void displayConnected() {
    String message = "Connected!";
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor((128 - u8g2.getStrWidth(message.c_str())) / 2, 32); // Center the text
    u8g2.print(message);
    u8g2.sendBuffer();
    Serial.println("Display Connected Message");
}

// Function to parse RPM
void parseRPM(const std::string& response) {
    if (response.size() >= 4 && response.substr(0, 4) == "41 0C") {
        int highByte = std::stoi(response.substr(5, 2), nullptr, 16);
        int lowByte = std::stoi(response.substr(8, 2), nullptr, 16);
        int rpm = ((highByte * 256) + lowByte) / 4;
        Serial.printf("Parsed RPM: %d\n", rpm);

        // Display RPM on the screen
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        String rpmMessage = "RPM: " + String(rpm);
        u8g2.setCursor((128 - u8g2.getStrWidth(rpmMessage.c_str())) / 2, 32);
        u8g2.print(rpmMessage);
        u8g2.sendBuffer();
    } else {
        Serial.println("Invalid RPM Response: " + response.c_str());
    }
}

// Notification handler
void onNotification(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    std::string response((char*)pData, length);
    Serial.println("Received CAN Response: " + response);
    parseRPM(response);
}

// Configure the ELM327
void configureProtocol() {
    Serial.println("Configuring ELM327 to use OBD-II over CAN...");
    std::string commands[] = {"AT Z", "AT E0", "AT SP 6"};

    for (const auto& command : commands) {
        pRemoteCharacteristic->writeValue(command, true);
        delay(100); // Allow the device to process the command
        Serial.printf("Sent command: %s\n", command.c_str());
    }
}

// Request RPM
void requestRPM() {
    std::string command = "010C";
    pRemoteCharacteristic->writeValue(command, true);
    Serial.println("Sent CAN request for RPM: " + command);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE Client...");

    // Initialize Display
    u8g2.begin();
    displayWaiting(0);

    // Initialize BLE
    BLEDevice::init("ESP32_BLE_Client");
    pClient = BLEDevice::createClient();
}

void loop() {
    if (!deviceConnected) {
        connectionAttempt++;
        Serial.printf("Connection attempt #%d...\n", connectionAttempt);

        // Update Display
        displayWaiting(connectionAttempt);

        // Attempt Connection
        if (pClient->connect(BLEAddress(elmMacAddress))) {
            Serial.println("Connection successful.");
            pRemoteCharacteristic = pClient->getService("0000fff0-0000-1000-8000-00805f9b34fb")
                                      ->getCharacteristic("00002902-0000-1000-8000-00805f9b34fb");

            if (pRemoteCharacteristic->canNotify()) {
                pRemoteCharacteristic->registerForNotify(onNotification);
                deviceConnected = true;
                displayConnected();
                configureProtocol();
            } else {
                Serial.println("Characteristic does not support notifications.");
                pClient->disconnect();
            }
        } else {
            Serial.println("Connection failed.");
            delay(5000); // Wait 5 seconds before retrying
        }
    } else {
        requestRPM();
        delay(2000); // Query RPM every 2 seconds
    }
}
