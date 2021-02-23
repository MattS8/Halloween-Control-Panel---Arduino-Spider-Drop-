#ifndef SPIDER_DROP_FIREBASE_FUNCTIONS_H
#define SPIDER_DROP_FIREBASE_FUNCTIONS_H

#include "FirebaseESP8266.h"
#include "WiFiCreds.h"
#include <ESP8266WiFi.h>
#include <FS.h>

#ifndef LOCAL_FILE
#define LOCAL_FILE
const char* SpiderDropFilePath = "SpiderDrop.dat";
#endif

typedef struct SpiderDropData {
    int pin;                // Pin number associated with triggering a drop
    int hangTime;           // The time the spider will stay in the dropped position before retracting
    int spiderState;        // The current state of the spider drop device (one of: RETRACTED, DROPPED, RETRACTING)
    int command;            // The most recent command given from firebase. (one of: DROP, RETRACT, NONE)
    bool stayDropped;       // Determines if the spider should stay in the dropped position until manually retracted
    int dropMotorPin;       // Pin number associated with controlling the drop motor
    int currentSensePin;    // Pin number associated with sensing the current
    int retractMotorPin;    // Pin number associated with controlling the retracting motor
    int dropStopSwitchpin;  // Pin number associated with sensing the position of the switch
    int currentPulseDelay;  // How long it takes for motor to start/stop pulse decay
    int currentLimit;       // How much current needs to be read before stopping the motor
    int upLimitSwitchPin;   // Pin used as fail-safe to stop the retracting motor
    int dropMotorDelay;     // Delay used to get the trigger off the release switch

} SpiderDropData;

// STATES
#define RETRACTED 1
#define RETRACTING 2
#define DROPPED 3

const char* STATE_RETRACTED = "RETRACTED";
const char* STATE_RETRACTING = "RETRACTING";
const char* STATE_DROPPED = "DROPPED";

// COMMANDS
#define DROP 1
#define RETRACT 2
#define NONE 3

// CUSTOME FIREBASE DEFINES
#define FIREBASE_PIN_A0 88

const char* COMMAND_DROP = "DROP";
const char* COMMAND_RETRACT = "RETRACT";
const char* COMMAND_NONE = "_none_";

static const String FIREBASE_HOST = "halloween-control-center.firebaseio.com";
static const String FIREBASE_AUTH = "dHKCDHppSOQcBKU5a49DgfOOzVoxXdJIB7PaJDa7";

FirebaseData firebaseDataRECV;
FirebaseData firebaseDataSEND;
String DevicePath = "";
String StatePath = "";
bool newDataReceived = false;
bool WiFiSetup = false;
bool FirebaseSetup = false;

/*
    Setting default values will allow the spider drop to work even before it has read
    data from Firebase backend.
*/
SpiderDropData SpiderDrop = {
    2,          //pin
    2000,       //hangTime
    RETRACTED,  //spiderState
    NONE,       //command
    false,      //stayDropped
    5,          //dropMotorPin
    A0,         //currentSensePin
    4,          //retractMotorPin
    12,         //dropStopSwitchPin
    250,        //currentPulseDelay
    200,        //currentLimit
    14,         //upLimitSwitchPin
    1450        //dropMotorDelay
};

#endif