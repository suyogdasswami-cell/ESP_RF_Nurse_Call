#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <MD_MAX72xx.h>

// =====================================================
// ESP RF NURSE CALL SYSTEM - V1
// =====================================================

// ---------------- PIN CONFIG ----------------
#define RF_RX_PIN       27
#define RF_TX_PIN       26

#define CALL_BUTTON_PIN 25
#define ACK_BUTTON_PIN  33
#define BUZZER_PIN      32

#define MAX7219_DATA    23
#define MAX7219_CLK     18
#define MAX7219_CS      5

#define MAX_DEVICES     4

// MAX7219 hardware type
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

// -----------------------------------------------------

MD_MAX72XX mx = MD_MAX72XX(
    HARDWARE_TYPE,
    MAX7219_CS,
    MAX_DEVICES
);

WebServer server(80);
Preferences prefs;

// ---------------- DEVICE SETTINGS ----------------

uint16_t deviceID = 1;
uint16_t groupID = 100;

String roomName = "ROOM 01";

String deviceType = "RECEIVER";

// ---------------- RF PACKET ----------------

struct RFPacket {
    uint16_t group;
    uint16_t device;
    uint8_t type;
};

#define PACKET_CALL  1
#define PACKET_ACK   2

// ---------------- CALL QUEUE ----------------

#define MAX_QUEUE 20

uint16_t callQueue[MAX_QUEUE];
uint8_t queueCount = 0;

bool activeCall = false;
uint16_t activeRoom = 0;

unsigned long buzzerTimer = 0;
bool buzzerState = false;

// ---------------- BUTTON DEBOUNCE ----------------

bool lastCallButton = HIGH;
bool lastAckButton = HIGH;

unsigned long lastCallDebounce = 0;
unsigned long lastAckDebounce = 0;

const unsigned long debounceDelay = 50;

// =====================================================
// SETTINGS
// =====================================================

void loadSettings()
{
    prefs.begin("nursecall", true);

    deviceID = prefs.getUShort("deviceID", 1);
    groupID = prefs.getUShort("groupID", 100);

    roomName = prefs.getString("roomName", "ROOM 01");
    deviceType = prefs.getString("type", "RECEIVER");

    prefs.end();

    Serial.println("Settings loaded");
    Serial.print("Device ID: ");
    Serial.println(deviceID);

    Serial.print("Group ID: ");
    Serial.println(groupID);

    Serial.print("Room: ");
    Serial.println(roomName);

    Serial.print("Type: ");
    Serial.println(deviceType);
}


void saveSettings()
{
    prefs.begin("nursecall", false);

    prefs.putUShort("deviceID", deviceID);
    prefs.putUShort("groupID", groupID);
    prefs.putString("roomName", roomName);
    prefs.putString("type", deviceType);

    prefs.end();

    Serial.println("Settings saved");
}

// =====================================================
// DISPLAY
// =====================================================

void clearDisplay()
{
    mx.clear();
}


void showRoom(uint16_t room)
{
    clearDisplay();

    String text = "ROOM ";
    
    if (room < 10)
        text += "0";

    text += String(room);

    Serial.print("DISPLAY: ");
    Serial.println(text);

    // Simple character display
    int pos = 0;

    for (int i = 0; i < text.length(); i++)
    {
        mx.setChar(pos, text[i]);
        pos += 8;

        if (pos >= MAX_DEVICES * 8)
            break;
    }
}


void showCall()
{
    showRoom(activeRoom);
}


void showIdle()
{
    clearDisplay();

    String text = "READY";

    int pos = 0;

    for (int i = 0; i < text.length(); i++)
    {
        mx.setChar(pos, text[i]);
        pos += 8;

        if (pos >= MAX_DEVICES * 8)
            break;
    }
}

// =====================================================
// QUEUE
// =====================================================

bool isRoomInQueue(uint16_t room)
{
    if (activeCall && activeRoom == room)
        return true;

    for (int i = 0; i < queueCount; i++)
    {
        if (callQueue[i] == room)
            return true;
    }

    return false;
}


void addCall(uint16_t room)
{
    if (isRoomInQueue(room))
        return;

    if (!activeCall)
    {
        activeCall = true;
        activeRoom = room;

        showCall();

        Serial.print("ACTIVE CALL ROOM: ");
        Serial.println(room);

        return;
    }

    if (queueCount >= MAX_QUEUE)
    {
        Serial.println("CALL QUEUE FULL");
        return;
    }

    callQueue[queueCount] = room;
    queueCount++;

    Serial.print("CALL QUEUED ROOM: ");
    Serial.println(room);
}


void acknowledgeCall()
{
    Serial.println("ACK pressed");

    if (!activeCall)
        return;

    activeCall = false;

    digitalWrite(BUZZER_PIN, LOW);

    if (queueCount > 0)
    {
        activeRoom = callQueue[0];

        for (int i = 0; i < queueCount - 1; i++)
        {
            callQueue[i] = callQueue[i + 1];
        }

        queueCount--;

        activeCall = true;

        showCall();

        Serial.print("NEXT CALL ROOM: ");
        Serial.println(activeRoom);
    }
    else
    {
        activeRoom = 0;

        showIdle();

        Serial.println("NO MORE CALLS");
    }
}

// =====================================================
// BUZZER
// =====================================================

void updateBuzzer()
{
    if (!activeCall)
    {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerState = false;
        return;
    }

    if (millis() - buzzerTimer >= 500)
    {
        buzzerTimer = millis();

        buzzerState = !buzzerState;

        digitalWrite(BUZZER_PIN, buzzerState);
    }
}

// =====================================================
// RF RECEIVE
// =====================================================

// NOTE:
// This V1 uses a simple serial-style RF interface.
// Actual 433 MHz module wiring/protocol can be replaced
// with RadioHead / RH_ASK after hardware selection.

void checkRF()
{
    if (!digitalRead(RF_RX_PIN))
    {
        // RF activity detected
        // Placeholder for RF decoder

        Serial.println("RF activity detected");

        delay(10);
    }
}

// =====================================================
// RF TRANSMIT
// =====================================================

void sendCall()
{
    Serial.println("Sending RF CALL");

    Serial.print("Group: ");
    Serial.println(groupID);

    Serial.print("Device: ");
    Serial.println(deviceID);

    // Placeholder for actual RF transmission
}

// =====================================================
// BUTTONS
// =====================================================

void checkCallButton()
{
    bool reading = digitalRead(CALL_BUTTON_PIN);

    if (reading != lastCallButton)
    {
        lastCallDebounce = millis();
    }

    if ((millis() - lastCallDebounce) > debounceDelay)
    {
        if (reading == LOW && lastCallButton == HIGH)
        {
            Serial.println("CALL BUTTON PRESSED");

            sendCall();
        }
    }

    lastCallButton = reading;
}


void checkAckButton()
{
    bool reading = digitalRead(ACK_BUTTON_PIN);

    if (reading != lastAckButton)
    {
        lastAckDebounce = millis();
    }

    if ((millis() - lastAckDebounce) > debounceDelay)
    {
        if (reading == LOW && lastAckButton == HIGH)
        {
            acknowledgeCall();
        }
    }

    lastAckButton = reading;
}

// =====================================================
// WIFI AP SETUP
// =====================================================

String htmlPage()
{
    String page;

    page += "<!DOCTYPE html>";
    page += "<html>";
    page += "<head>";
    page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    page += "<title>ESP RF Nurse Call</title>";
    page += "</head>";

    page += "<body>";

    page += "<h2>ESP RF Nurse Call Setup</h2>";

    page += "<form action='/save' method='POST'>";

    page += "Device ID:<br>";
    page += "<input name='deviceID' value='" + String(deviceID) + "'><br><br>";

    page += "Group ID:<br>";
    page += "<input name='groupID' value='" + String(groupID) + "'><br><br>";

    page += "Room Name:<br>";
    page += "<input name='roomName' value='" + roomName + "'><br><br>";

    page += "Device Type:<br>";

    page += "<select name='type'>";

    page += "<option value='RECEIVER'";
    
    if (deviceType == "RECEIVER")
        page += " selected";

    page += ">RECEIVER</option>";

    page += "<option value='TRANSMITTER'";

    if (deviceType == "TRANSMITTER")
        page += " selected";

    page += ">TRANSMITTER</option>";

    page += "</select><br><br>";

    page += "<input type='submit' value='SAVE'>";

    page += "</form>";

    page += "</body>";
    page += "</html>";

    return page;
}


void handleRoot()
{
    server.send(200, "text/html", htmlPage());
}


void handleSave()
{
    if (server.hasArg("deviceID"))
        deviceID = server.arg("deviceID").toInt();

    if (server.hasArg("groupID"))
        groupID = server.arg("groupID").toInt();

    if (server.hasArg("roomName"))
        roomName = server.arg("roomName");

    if (server.hasArg("type"))
        deviceType = server.arg("type");

    saveSettings();

    server.send(
        200,
        "text/html",
        "<h2>Saved!</h2><p>Restarting...</p>"
    );

    delay(1000);

    ESP.restart();
}


void startAP()
{
    String apName = "NurseCall-" + String(deviceID);

    WiFi.mode(WIFI_AP);

    WiFi.softAP(apName.c_str(), "12345678");

    Serial.println();
    Serial.println("================================");
    Serial.println("CONFIGURATION AP STARTED");
    Serial.print("AP NAME: ");
    Serial.println(apName);

    Serial.println("PASSWORD: 12345678");

    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());

    Serial.println("================================");

    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);

    server.begin();
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);

    delay(500);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP RF NURSE CALL SYSTEM");
    Serial.println("VERSION 1.0");
    Serial.println("================================");

    pinMode(RF_RX_PIN, INPUT);
    pinMode(RF_TX_PIN, OUTPUT);

    pinMode(CALL_BUTTON_PIN, INPUT_PULLUP);
    pinMode(ACK_BUTTON_PIN, INPUT_PULLUP);

    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW);

    loadSettings();

    // MAX7219
    mx.begin();

    mx.control(MD_MAX72XX::INTENSITY, 5);
    mx.control(MD_MAX72XX::SHUTDOWN, false);
    mx.clear();

    showIdle();

    startAP();

    Serial.println("SYSTEM READY");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    server.handleClient();

    checkCallButton();

    checkAckButton();

    checkRF();

    updateBuzzer();

    delay(5);
}