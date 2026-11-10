
#include <Arduino.h>
#include <WiFi.h>
#include "wifi_secrets.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
BLEClient* train1Client = nullptr;
BLEClient* train2Client = nullptr;
BLEClient* train3Client = nullptr;

BLERemoteCharacteristic* train1Control = nullptr;
BLERemoteCharacteristic* train2Control = nullptr;
BLERemoteCharacteristic* train3Control = nullptr;
void scanLEGOHub()
{
    Serial.println("Scanning for LEGO Hub...");

    BLEDevice::init("");
    BLEScan* scanner = BLEDevice::getScan();
    scanner->setActiveScan(true);

    BLEScanResults* results = scanner->start(5, false);

    for (int i = 0; i < results->getCount(); i++)
    {
        BLEAdvertisedDevice device = results->getDevice(i);

        Serial.print("Device: ");
        Serial.println(device.toString().c_str());
    }

    scanner->clearResults();
}
void legoNotificationCallback(
    BLERemoteCharacteristic* characteristic,
    uint8_t* data,
    size_t length,
    bool isNotify)
{
    Serial.print("LEGO Hub response: ");

    for (size_t i = 0; i < length; i++)
    {
        Serial.printf("%02X ", data[i]);
    }

    Serial.println();
}

bool connectToLEGOHub(
    const char* address,
    BLEClient*& client,
    BLERemoteCharacteristic*& control)
{
    Serial.print("Connecting to LEGO Hub: ");
    Serial.println(address);

    control = nullptr;

    client = BLEDevice::createClient();
    BLEAddress hubAddress(address);

    if (!client->connect(hubAddress)) {
        Serial.println("BLE connection failed!");
        return false;
    }

    Serial.println("BLE connected!");

    BLEUUID serviceUUID(
        "00001623-1212-efde-1623-785feabcd123"
    );

    BLERemoteService* service =
        client->getService(serviceUUID);

    if (service == nullptr) {
        Serial.println("LEGO service not found!");
        client->disconnect();
        return false;
    }

    BLEUUID characteristicUUID(
        "00001624-1212-efde-1623-785feabcd123"
    );

    BLERemoteCharacteristic* characteristic =
        service->getCharacteristic(characteristicUUID);

    if (characteristic == nullptr ||
        !characteristic->canWrite()) {
        Serial.println("LEGO control not available!");
        client->disconnect();
        return false;
    }

    control = characteristic;

    if (control->canNotify()) {
        control->registerForNotify(
            legoNotificationCallback
        );
    }

    Serial.println("LEGO Hub ready!");
    return true;
}
unsigned long motorStopTime = 0;
bool motorRunning = false;
int train1Speed = 0;
const unsigned long MOTOR_TIMEOUT = 10000;

unsigned long train2StopTime = 0;
bool train2Running = false;
int train2Speed = 0;
bool train1Braking = false;
bool train2Braking = false;

unsigned long train1BrakeTime = 0;
unsigned long train2BrakeTime = 0;
bool train1Accelerating = false;
bool train2Accelerating = false;

int train1TargetSpeed = 0;
int train2TargetSpeed = 0;

unsigned long train1AccelTime = 0;
unsigned long train2AccelTime = 0;

const unsigned long ACCEL_INTERVAL = 200;
const int ACCEL_STEP = 5;

const unsigned long BRAKE_INTERVAL = 200;
const int BRAKE_STEP = 5;

unsigned long train3StopTime = 0;
bool train3Running = false;
int train3Speed = 0;

bool train3Braking = false;
unsigned long train3BrakeTime = 0;

bool train3Accelerating = false;
int train3TargetSpeed = 0;
unsigned long train3AccelTime = 0;


void setMotorSpeed(
    BLERemoteCharacteristic* control,
    int8_t speed)
{
    if (control == nullptr)
    {
        Serial.println("Train control not available!");
        return;
    }

    // Identify which train we are controlling
    BLEClient* client = nullptr;

    if (control == train1Control)
    {
        client = train1Client;
    }
    else if (control == train2Control)
    {
        client = train2Client;
    }
    else if (control == train3Control)
    {
        client = train3Client;
    }
    else
    {
        Serial.println("Unknown train control!");
        return;
    }

    // Check BLE connection
    if (client == nullptr || !client->isConnected())
    {
        Serial.println("BLE disconnected! Motor command not sent!");
        return;
    }

    // LEGO Powered Up motor command
    uint8_t command[] = {
        0x08,
        0x00,
        0x81,
        0x00,
        0x10,
        0x51,
        0x00,
        (uint8_t)speed
    };

    // Send command to LEGO Hub
    control->writeValue(
        command,
        sizeof(command),
        false
    );

    // Remember last commanded motor power
    if (control == train1Control)
    {
        train1Speed = speed;
    }
    else if (control == train2Control)
    {
        train2Speed = speed;
    }
    else if (control == train3Control)
    {
        train3Speed = speed;
    }
}



void smoothStop(
    BLERemoteCharacteristic* control)
{
    if (control == nullptr)
    {
        Serial.println("Train not connected!");
        return;
    }

    if (control == train1Control)
    {
        train1Accelerating = false;
        train1Braking = (train1Speed != 0);
        train1BrakeTime = millis();
    }
    else if (control == train2Control)
    {
        train2Accelerating = false;
        train2Braking = (train2Speed != 0);
        train2BrakeTime = millis();
    }
    else if (control == train3Control)
    {
        train3Accelerating = false;
        train3Braking = (train3Speed != 0);
        train3BrakeTime = millis();
    }
}



void smoothStart(
    BLERemoteCharacteristic* control,
    int targetSpeed)
{
    if (control == nullptr)
    {
        Serial.println("Train not connected!");
        return;
    }

    if (control == train1Control)
    {
        train1Braking = false;
        train1TargetSpeed = targetSpeed;
        train1Accelerating = (train1Speed != targetSpeed);
        train1AccelTime = millis();
        
    }
    else if (control == train2Control)
    {
        train2Braking = false;
        train2TargetSpeed = targetSpeed;
        train2Accelerating = (train2Speed != targetSpeed);
        train1AccelTime = millis();
        
    }
    else if (control == train3Control)
    {      
        train3Braking = false;
        train3TargetSpeed = targetSpeed;
        train3Accelerating = (train3Speed != targetSpeed);
        train1AccelTime = millis();
        
    }
}

void updateAcceleration()
{
    unsigned long now = millis();

    if (train1Accelerating &&
        now - train1AccelTime >= ACCEL_INTERVAL)
    {
        train1AccelTime = now;

        int speed = train1Speed;

        if (speed < train1TargetSpeed)
            speed = min(speed + ACCEL_STEP, train1TargetSpeed);
        else
            speed = max(speed - ACCEL_STEP, train1TargetSpeed);

        setMotorSpeed(train1Control, speed);

        if (speed == train1TargetSpeed)
        {
            train1Accelerating = false;
            Serial.println("Train 1 reached target power!");
        }
    }

    if (train2Accelerating &&
        now - train2AccelTime >= ACCEL_INTERVAL)
    {
        train2AccelTime = now;

        int speed = train2Speed;

        if (speed < train2TargetSpeed)
            speed = min(speed + ACCEL_STEP, train2TargetSpeed);
        else
            speed = max(speed - ACCEL_STEP, train2TargetSpeed);

        setMotorSpeed(train2Control, speed);

        if (speed == train2TargetSpeed)
        {
            train2Accelerating = false;
            Serial.println("Train 2 reached target power!");
        }
    }
    
if (train3Accelerating &&
    now - train3AccelTime >= ACCEL_INTERVAL)
{
    train3AccelTime = now;

    int speed = train3Speed;

    if (speed < train3TargetSpeed)
        speed = min(speed + ACCEL_STEP, train3TargetSpeed);
    else
        speed = max(speed - ACCEL_STEP, train3TargetSpeed);

    setMotorSpeed(train3Control, speed);

    if (speed == train3TargetSpeed)
    {
        train3Accelerating = false;
        Serial.println("Train 3 reached target power!");
    }
}


}


void updateBraking()
{
    unsigned long now = millis();

    if (train1Braking &&
        now - train1BrakeTime >= BRAKE_INTERVAL)
    {
        train1BrakeTime = now;

        int speed = train1Speed;

        if (speed > 0)
            speed = max(0, speed - BRAKE_STEP);
        else
            speed = min(0, speed + BRAKE_STEP);

        setMotorSpeed(train1Control, speed);

        if (speed == 0)
        {
            train1Braking = false;
            motorRunning = false;
            Serial.println("Train 1 smoothly stopped!");
        }
    }

    if (train2Braking &&
        now - train2BrakeTime >= BRAKE_INTERVAL)
    {
        train2BrakeTime = now;

        int speed = train2Speed;

        if (speed > 0)
            speed = max(0, speed - BRAKE_STEP);
        else
            speed = min(0, speed + BRAKE_STEP);

        setMotorSpeed(train2Control, speed);

        if (speed == 0)
        {
            train2Braking = false;
            train2Running = false;
            Serial.println("Train 2 smoothly stopped!");
        }
    }
    if (train3Braking &&
    now - train3BrakeTime >= BRAKE_INTERVAL)
{
    train3BrakeTime = now;

    int speed = train3Speed;

    if (speed > 0)
        speed = max(0, speed - BRAKE_STEP);
    else
        speed = min(0, speed + BRAKE_STEP);

    setMotorSpeed(train3Control, speed);

    if (speed == 0)
    {
        train3Braking = false;
        train3Running = false;
        Serial.println("Train 3 smoothly stopped!");
    }
}
}


void setLights(
    BLERemoteCharacteristic* control,
    uint8_t brightness)
{
    if (control == nullptr) {
        Serial.println("Train not connected!");
        return;
    }

    uint8_t command[] = {
        0x08,
        0x00,
        0x81,
        0x01,
        0x10,
        0x51,
        0x00,
        brightness
    };

    control->writeValue(
        command,
        sizeof(command),
        false
    );
}


void setup()
{

    Serial.begin(115200);
    delay(2000);
    scanLEGOHub();

Serial.println("Connecting Train 1...");

connectToLEGOHub(
    "9c:9a:c0:4a:8b:b9",
    train1Client,
    train1Control
);

Serial.println("Connecting Train 2...");

connectToLEGOHub(
    "9c:9a:c0:33:9f:eb",
    train2Client,
    train2Control
);
Serial.println("Connecting Train 3...");

connectToLEGOHub(
    "9c:9a:c0:4b:fd:20",
    train3Client,
    train3Control
);
    Serial.println("Starting ESP32...");
    int networks = WiFi.scanNetworks();

Serial.println(networks);
for (int i = 0; i < networks; i++) {
  Serial.println(WiFi.SSID(i));
}
    WiFi.begin(ssid, password);

    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("WiFi connection timeout!");
    }
}
void loop()
{
    if (Serial.available() > 0)
    {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toUpperCase();

    

if (command == "START")
{
    Serial.println("Starting Motor A...");

    smoothStart(train1Control, 30);

    motorRunning = true;
    motorStopTime = millis() + MOTOR_TIMEOUT;

    Serial.println("Train 1 accelerating smoothly!");
}
else if (command == "TRAIN3 START")
{
    if (train3Control != nullptr)
    {
        smoothStart(train3Control, 20);

        train3Running = true;
        train3StopTime = millis() + MOTOR_TIMEOUT;

        Serial.println("Train 3 starting!");
    }
    else
    {
        Serial.println("Train 3 not connected!");
    }
}
else if (command == "TRAIN3 STOP")
{
    smoothStop(train3Control);

    Serial.println("Train 3 braking smoothly...");
}

else if (command == "REVERSE")
{
    Serial.println("Reversing Motor A...");
    train1Accelerating = false;
    train1Braking = false;
    setMotorSpeed(train1Control, 0);
    motorRunning = false;

    delay(500);

    setMotorSpeed(train1Control, -20);

    motorRunning = true;
    motorStopTime = millis() + MOTOR_TIMEOUT;

    Serial.println("Motor A running REVERSE!");
}

else if (command.startsWith("REVERSE "))
{
    String value = command.substring(8);
    value.trim();

    int speed = value.toInt();

    if (speed >= 0 && speed <= 60 &&
        value == String(speed))
    {
        // Cancel ongoing braking
        train1Braking = false;
        train1Accelerating = false;
        // Stop before changing direction
        setMotorSpeed(train1Control, 0);
        motorRunning = false;

        delay(500);

        // Negative power = reverse
        setMotorSpeed(train1Control, -speed);

        motorRunning = (speed != 0);
        motorStopTime = millis() + MOTOR_TIMEOUT;

        Serial.printf(
            "Train 1 reversing at %d%% power\n",
            speed
        );
    }
    else
    {
        Serial.println("Invalid reverse speed! Use 0-60.");
    }
}

else if (command == "LIGHTS ON")
{
    setLights(train1Control, 100);
    Serial.println("Lights ON!");
}
else if (command == "LIGHTS OFF")
{
    setLights(train1Control, 0);
    Serial.println("Lights OFF!");
}

else if (command.startsWith("LIGHTS "))
{
    String value = command.substring(7);
    int brightness = value.toInt();

    if (brightness >= 0 && brightness <= 100 &&
        value.length() > 0 &&
        value.toInt() == brightness)
    {
        setLights(train1Control, brightness);
        Serial.printf("Lights brightness: %d%%\n", brightness);
    }
    else
    {
        Serial.println("Invalid brightness! Use 0-100.");
    }
}
else if (command.startsWith("SPEED "))
{
    String value = command.substring(6);
    value.trim();

    int speed = value.toInt();

    if (speed >= 0 && speed <= 60 &&
        value == String(speed))
    {
        train1Accelerating = false;
        train1Braking = false;

        setMotorSpeed(train1Control, speed);

        motorRunning = (speed > 0);
        motorStopTime = millis() + MOTOR_TIMEOUT;

        Serial.printf("Train 1 motor power: %d%%\n", speed);
    }
    else
    {
        Serial.println("Invalid speed! Use 0-60.");
    }
}
// speed for 3 train

else if (command.startsWith("TRAIN3 SPEED "))
{
    String value = command.substring(13);
    value.trim();

    int speed = value.toInt();

    if (speed >= 0 && speed <= 60 &&
        value == String(speed))
    {
        train3Accelerating = false;
        train3Braking = false;

        setMotorSpeed(train3Control, speed);

        train3Running = (speed > 0);
        train3StopTime = millis() + MOTOR_TIMEOUT;

        Serial.printf("Train 3 motor power: %d%%\n", speed);
    }
    else
    {
        Serial.println("Invalid speed! Use 0-60.");
    }
}

else if (command == "STOP")
{
     smoothStop(train1Control);

    Serial.println("Train 1 braking smoothly...");
}


else if (command == "TRAIN2 START")
{
    smoothStart(train2Control, 20);

    train2Running = true;
    train2StopTime = millis() + MOTOR_TIMEOUT;

    Serial.println("Train 2 accelerating smoothly!");
}


else if (command.startsWith("TRAIN2 SPEED "))
{
    String value = command.substring(13);
    value.trim();

    int speed = value.toInt();

    if (speed >= 0 && speed <= 60 &&
        value == String(speed))
    {
        train2Accelerating = false;
        train2Braking = false;

        setMotorSpeed(train2Control, speed);

        train2Running = (speed > 0);
        train2StopTime = millis() + MOTOR_TIMEOUT;

        Serial.printf("Train 2 motor power: %d%%\n", speed);
    }
    else
    {
        Serial.println("Invalid speed! Use 0-60.");
    }
}


else if (command == "TRAIN2 STOP")
{
     smoothStop(train2Control);

    Serial.println("Train 2 braking smoothly...");
}
else if (command == "TRAIN2 REVERSE")
{
    train2Accelerating = false;
    train2Braking = false;
    setMotorSpeed(train2Control, 0);
    train2Running = false;

    delay(500);

    setMotorSpeed(train2Control, -20);

    train2Running = true;
    train2StopTime = millis() + MOTOR_TIMEOUT;

    Serial.println("Train 2 REVERSE!");
}

else if (command == "ALL REVERSE")
{
    train1Accelerating = false;
    train2Accelerating = false;
    train3Accelerating = false;
    // Cancel ongoing braking
    train1Braking = false;
    train2Braking = false;
    train3Braking = false;

    // Stop both motors before reversing
    setMotorSpeed(train1Control, 0);
    setMotorSpeed(train2Control, 0);
    setMotorSpeed(train3Control, 0);


    delay(500);

    // Reverse both trains
    setMotorSpeed(train1Control, -20);
    setMotorSpeed(train2Control, -20);
    setMotorSpeed(train3Control, -20);

    motorRunning = true;
    train2Running = true;
    train3Running = true;

    motorStopTime = millis() + MOTOR_TIMEOUT;
    train2StopTime = millis() + MOTOR_TIMEOUT;
    train3StopTime = millis() + MOTOR_TIMEOUT;


    Serial.println("ALL REVERSE - All trains reversing!");
}
else if (command.startsWith("ALL REVERSE "))
{
    String value = command.substring(12);
    value.trim();

    int speed = value.toInt();

    if (speed >= 0 && speed <= 60 &&
        value == String(speed))
    {
        train1Braking = false;
        train2Braking = false;
        train3Braking = false;

        train1Accelerating = false;
        train2Accelerating = false;
        train3Accelerating = false;

        setMotorSpeed(train1Control, 0);
        setMotorSpeed(train2Control, 0);
        setMotorSpeed(train3Control, 0);

        motorRunning = false;
        train2Running = false;
        train3Running = false;

        delay(500);

        setMotorSpeed(train1Control, -speed);
        setMotorSpeed(train2Control, -speed);
        setMotorSpeed(train3Control, -speed);

        motorRunning = (speed != 0);
        train2Running = (speed != 0);
        train3Running = (speed != 0);

        motorStopTime = millis() + MOTOR_TIMEOUT;
        train2StopTime = millis() + MOTOR_TIMEOUT;
        train3StopTime = millis() + MOTOR_TIMEOUT;

        Serial.printf(
            "ALL REVERSE - All trains reversing at %d%% power\n",
            speed
        );
    }
    else
    {
        Serial.println("Invalid speed! Use 0-60.");
    }
}


else if (command == "ALL START")
{
    train1Braking = false;
    train2Braking = false;
    train3Braking = false;

    smoothStart(train1Control, 30);
    smoothStart(train2Control, 20);
    smoothStart(train3Control, 20);

    motorRunning = true;
    train2Running = true;
    train3Running = true;

    motorStopTime = millis() + MOTOR_TIMEOUT;
    train2StopTime = millis() + MOTOR_TIMEOUT;
    train3StopTime = millis() + MOTOR_TIMEOUT;

    Serial.println("ALL START - All trains accelerating!");
}


else if (command == "ALL STOP")
{
    setMotorSpeed(train1Control, 0);
    setMotorSpeed(train2Control, 0);
    setMotorSpeed(train3Control, 0);

    motorRunning = false;
    train2Running = false;
    train1Accelerating = false;

    train2Accelerating = false;
    train1Braking = false;
    train2Braking = false;

    train3Accelerating = false;
    train3Braking = false;
    train3Running = false;

    Serial.println("ALL STOP requested!");
}


else if (command.startsWith("TRAIN2 REVERSE "))
{
    String value = command.substring(15);
    value.trim();

    int speed = value.toInt();

    if (speed >= 0 && speed <= 60 &&
        value == String(speed))
    {
        // Cancel acceleration and braking
        train2Accelerating = false;
        train2Braking = false;

        // Stop motor before reversing
        setMotorSpeed(train2Control, 0);
        train2Running = false;

        delay(500);

        // Reverse motor
        setMotorSpeed(train2Control, -speed);

        train2Running = (speed != 0);
        train2StopTime = millis() + MOTOR_TIMEOUT;

        Serial.printf(
            "Train 2 reversing at %d%% power\n",
            speed
        );
    }
    else
    {
        Serial.println("Invalid reverse speed! Use 0-60.");
    }
}


else if (command.startsWith("TRAIN3 REVERSE "))
{
    String value = command.substring(15);
    value.trim();

    int speed = value.toInt();

    if (speed >= 0 && speed <= 60 &&
        value == String(speed))
    {
        // Cancel acceleration and braking
        train3Accelerating = false;
        train3Braking = false;

        // Stop motor before reversing
        setMotorSpeed(train3Control, 0);
        train3Running = false;

        delay(500);

        // Reverse motor
        setMotorSpeed(train3Control, -speed);

        train3Running = (speed != 0);
        train3StopTime = millis() + MOTOR_TIMEOUT;

        Serial.printf(
            "Train 3 reversing at %d%% power\n",
            speed
        );
    }
    else
    {
        Serial.println("Invalid reverse speed! Use 0-60.");
    }
}


else if (command == "ALL SMOOTH STOP")
{
    smoothStop(train1Control);
    smoothStop(train2Control);
    smoothStop(train3Control);

    Serial.println("All trains braking smoothly!");
}



    }
    
if (motorRunning &&
    (long)(millis() - motorStopTime) >= 0)
{
    train1Accelerating = false;
    train1Braking = false;

    setMotorSpeed(train1Control, 0);
    motorRunning = false;

    Serial.println("Train 1 - Automatic STOP!");
}

if (train2Running &&
    (long)(millis() - train2StopTime) >= 0)
{
    train2Accelerating = false;
    train2Braking = false;

    setMotorSpeed(train2Control, 0);
    train2Running = false;

    Serial.println("Train 2 - Automatic STOP!");
}

if (train3Running &&
    (long)(millis() - train3StopTime) >= 0)
{
    train3Accelerating = false;
    train3Braking = false;

    setMotorSpeed(train3Control, 0);
    train3Running = false;

    Serial.println("Train 3 - Automatic STOP!");
}

updateBraking();
updateAcceleration();
}