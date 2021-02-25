#include "spider_drop.h"

#define RLD_DEBUG
void readLocalData()
{
	if (!SPIFFS.begin()) {
#ifdef RLD_DEBUG
		Serial.println("SPIFFS Initialization FAILED.");
#endif
		return;
	}

	File f = SPIFFS.open(SpiderDropFilePath, "r");

	if (!f) {
#ifdef RLD_DEBUG
		Serial.println("Failed to open file.");
#endif
		return;
	}

	uint8_t* pointer = (uint8_t*) &SpiderDrop;

	while (f.available()) {
		*pointer = f.read();
		++pointer;
	}

	if (SpiderDrop.hangTime < 0)
		SpiderDrop.hangTime = 1;

	f.close();
#ifdef RLD_DEBUG
	Serial.println("Read in SpiderData from file:");
    Serial.println("-----------------------------");
	Serial.print("Pin: "); Serial.println(SpiderDrop.pin);
    Serial.print("Hang Time: "); Serial.println(SpiderDrop.hangTime);
    Serial.print("Spider State: "); Serial.println(SpiderDrop.spiderState);
    Serial.print("Command: "); Serial.println(SpiderDrop.command);
    Serial.print("stayDropped: "); Serial.println(SpiderDrop.stayDropped);
    Serial.print("dropMotorPin: "); Serial.println(SpiderDrop.dropMotorPin);
    Serial.print("currentSensePin: "); Serial.println(SpiderDrop.currentSensePin);
    Serial.print("retractMotorPin: "); Serial.println(SpiderDrop.retractMotorPin);
    Serial.print("dropStopSwitchpin: "); Serial.println(SpiderDrop.dropStopSwitchpin);
    Serial.print("currentPulseDelay: "); Serial.println(SpiderDrop.currentPulseDelay);
    Serial.println("-----------------------------");
#endif
}

void setup() 
{
	Serial.begin(115200);

    readLocalData();

    // Setup Pins
    pinMode(SpiderDrop.dropMotorPin, OUTPUT);
    digitalWrite(SpiderDrop.dropMotorPin, HIGH);    // Stop drop motor

    pinMode(SpiderDrop.retractMotorPin, OUTPUT);
    digitalWrite(SpiderDrop.retractMotorPin, HIGH); // Start motor to pull up spider

    pinMode(SpiderDrop.pin, INPUT_PULLUP);
    pinMode(SpiderDrop.dropStopSwitchpin, INPUT_PULLUP);
    pinMode(SpiderDrop.currentSensePin, INPUT); 
    pinMode(SpiderDrop.upLimitSwitchPin, INPUT_PULLUP);

    delay(SpiderDrop.currentPulseDelay);    // Get past startup current pulse

	setupFirebaseFunctions();
}

int retractMotorState;
int dropDetectionState;
int dropSwitchState;
unsigned long hangDelayStart = 0;
int currentVal = 0;
long retractTimeout = 0;
void loop()
{
    if (!WiFiSetup)
        connectToWiFi();
    
    if (!FirebaseSetup && WiFi.status() == WL_CONNECTED)
		connectToFirebase();
    
    // Set instance variables to initial values if they were never set up before
	// or if new data was read from firebase
	if (newDataReceived) {
		handleNewData();
		newDataReceived = false;
	}

    /* Spider dropping logic goes here */

    /*
    * The device can be in one of three states: RETRACTED, DROPPED, or RETRACTING
    *      When in the RETRACTED state:
    *       - check SpiderDrop.pin to see if the spider should be dropped
    *       - drop spider when SpiderDrop.pin is LOW
    * 
    *      When in the DROPPED state:
    *       - check how long the spider has been hanging
    *       - pull spider back up with enough time has passed and the device isn't in 'stayDropped' mode
    * 
    *      When in the RETRACTING state:
    *       - ensure the drop motor isn't running
    *       - check the voltage drop across the motor
    *       - shut off the retract motor when needed
    * 
    * The state flow is as follows:   RETRACTED -> DROPPED -> RETRACTING -> RETRACTRED
    *   - The device flows from RETRACTED -> DROPPED when: 
    *       - SpiderDrop.pin == LOW  
    *       OR  
    *       - firebase command signals a drop
    * 
    *   - The device flows from DROPPED -> RETRACTING when: 
    *       - hangTime delay has expired AND device NOT in 'stayDropped' mode
    *       OR
    *       - firebase commang signals a retraction
    * 
    *   - The device flows from RETRACTING -> RETRACTED when:
    *       - voltage drop > SpiderDrop.currentLimit
    */

    if (SpiderDrop.spiderState == RETRACTING && millis() > retractTimeout) {
        digitalWrite(SpiderDrop.retractMotorPin, HIGH); 
        writeErrorToFirebase("RETRACT_MOTOR_TIMEOUT: Failed to trigger end of retraction. Timeout occur.");
        retractTimeout = 0;
    }

    switch (SpiderDrop.spiderState)
    {
    case RETRACTED:
        dropDetectionState = digitalRead(SpiderDrop.pin);
        if (dropDetectionState == LOW) {
            dropSpider();
        }
        break;

    case DROPPED:
        if (millis() - hangDelayStart > SpiderDrop.hangTime && !SpiderDrop.stayDropped) {
            retractSpider();
        }
        break;

    case RETRACTING:
        digitalWrite(SpiderDrop.dropMotorPin, HIGH);                        // Make sure drop motor is stopped
        currentVal = analogRead(SpiderDrop.currentSensePin);                // Get voltage drop across the motor load resistor
        if (digitalRead(SpiderDrop.upLimitSwitchPin) == LOW || currentVal > SpiderDrop.currentLimit) {
            stopRetractingSpider();
        }
        break;
    default:
        break;
    }

    delay(5); 
}

#define DROP_MOTOR_TIMEOUT 10000
void dropSpider()
{
    digitalWrite(SpiderDrop.retractMotorPin, HIGH);                     // Make sure motor is off
    digitalWrite(SpiderDrop.dropMotorPin, LOW);                         // Drop the spider  
    long dropTimeoutTime = millis() + DROP_MOTOR_TIMEOUT;


    do {
        delay(CONST_DELAY);
        dropSwitchState = digitalRead(SpiderDrop.dropStopSwitchpin);    // check the switch

        if (millis() > dropTimeoutTime) {
            writeErrorToFirebase("DROP_MOTOR_TIEMOUT: Drop Motor failed to release.");
            break;
        }
    } while (dropSwitchState == HIGH);

    delay(SpiderDrop.dropMotorDelay);                                   // Time for motor to get off position switch
    digitalWrite(SpiderDrop.dropMotorPin, HIGH);                        // Stop drop motor

    hangDelayStart = millis();                                          // Mark the beginning of the hang time

    SpiderDrop.spiderState = DROPPED;               
    writeStateToFirebase();                                             // Update Firebase about new state of the device
}

#define RETRACT_MOTOR_TIMEOUT 20000
void retractSpider()
{
    retractTimeout = millis() + RETRACT_MOTOR_TIMEOUT;
    // Hang time delay is over            
    digitalWrite(SpiderDrop.retractMotorPin, LOW);                  // Pull the spider back up

    SpiderDrop.spiderState = RETRACTING;               
    writeStateToFirebase();                                         // Update Firebase about new state of the device

    delay(SpiderDrop.currentPulseDelay);                            // Allow time for initial current pulse
}

void stopRetractingSpider()
{
    digitalWrite(SpiderDrop.retractMotorPin, HIGH);                 // Shut off motor

    SpiderDrop.spiderState = RETRACTED;                             
    writeStateToFirebase();                                         // Update Firebase about new state of the device

    delay(SpiderDrop.currentPulseDelay);                            // Time for voltage spike to decay
}

// Handles new data from firebase
// This includes potential commands to drop/retract the spider
#define HND_DEBUG
void handleNewData()
{
    int handledCommand = SpiderDrop.command;                        // Temp variable
    SpiderDrop.command = NONE;                                      // Immediately consume the command to prevent double-actions

    if (handledCommand == DROP) {
        // Handle Drop
        if (SpiderDrop.spiderState == RETRACTED) {
            dropSpider();
        } else {
            // Spider is not ready to be dropped
            #ifdef HND_DEBUG
            Serial.println("Received command to drop spider, however, however it's not ready to do so yet!");
            #endif
        }
        writeCommandToFirebase();
    } else if (handledCommand == RETRACT) {
        // Handle Retract
        if (SpiderDrop.spiderState == DROPPED) {
            retractSpider();
        } else {
            // Spider is not ready to be retracted
            #ifdef HND_DEBUG
            Serial.println("Received command to retract spider, however it's not ready to do so yet!");
            #endif
        }
        writeCommandToFirebase();
    }
}
