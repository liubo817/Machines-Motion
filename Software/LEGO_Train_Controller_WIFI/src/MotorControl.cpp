
#include <Arduino.h>
#include "MotorControl.h"

// Simulated motor state
static bool trainRunning = false;

void initializeMotor() {
    trainRunning = false;
    Serial.println("Motor initialized");
}

void startTrain() {
    trainRunning = true;
    Serial.println("Train STARTED");
}

void stopTrain() {
    trainRunning = false;
    Serial.println("Train STOPPED");
}

bool isTrainRunning() {
    return trainRunning;
}
