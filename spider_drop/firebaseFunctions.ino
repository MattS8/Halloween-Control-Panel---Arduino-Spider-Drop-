#include "firebaseFunctions.h"

#define WLD_DEBUG
void writeLocalData()
{
    if (!SPIFFS.format()) {
#ifdef WLD_DEBUG
        Serial.println("File System Formatting Failed.");
#endif
        return;
    }

    File f = SPIFFS.open(SpiderDropFilePath, "w");
    if (!f) {
#ifdef WLD_DEBUG
        Serial.println("Failed to open file.");
#endif
        return;
    }

    f.write(((uint8_t *) &SpiderDrop), sizeof(SpiderDropData));
    f.flush();
    f.close();

#ifdef WLD_DEBUG
    Serial.println("Finished writing Spider Drop Data to local storage.");
#endif
}

#define SETUP_FF_DEBUG
void setupFirebaseFunctions() {
    char* temp = (char* )malloc(50 * sizeof(char));

	sprintf(temp, "/devices/%lu", ESP.getChipId());
    DevicePath = String(temp);

    delete[] temp;

#ifdef SETUP_FF_DEBUG
    Serial.print("DevicePath: ");
    Serial.println(DevicePath);
#endif
}

void connectToWiFi() {
  WiFiSetup = true;
  WiFi.begin(WIFI_AP_NAME, WIFI_AP_PASS);
}

#define CON_WIFI_DEBUG
void connectToFirebase() {
  FirebaseSetup = true;
#ifdef CON_WIFI_DEBUG
	Serial.println();
	Serial.print("Connected with IP: ");
	Serial.println(WiFi.localIP());
	Serial.println();
#endif // CON_WIFI_DEBUG

    // Connect to Firebase
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.reconnectWiFi(true);

    if (!Firebase.beginStream(firebaseDataRECV, DevicePath)) {
#ifdef CON_WIFI_DEBUG
        Serial.println("------------------------------------");
        Serial.println("Can't begin stream connection...");
        Serial.println("REASON: " + firebaseDataRECV.errorReason());
        Serial.println("------------------------------------");
        Serial.println();
#endif
    }

    Firebase.setStreamCallback(firebaseDataRECV, handleDataRecieved, handleTimeout);
}

void handleTimeout(bool timeout) {
#ifdef TIMEOUT_DEBUG
  if (timeout)
  {
    Serial.println();
    Serial.println("Stream timeout, resume streaming...");
    Serial.println();
  }
#endif
}

int getPinValueFirebase(int pin) {
    return pin == A0 ? FIREBASE_PIN_A0
        : pin;
}

int getPinValue(String value) {
    return value == String(FIREBASE_PIN_A0) ? A0 
        : value.toInt();
}

#define HAR_DEBUG
void handleDataRecieved(StreamData data) {
    if (data.dataType() == "json") {
#ifdef HAR_DEBUG
        Serial.println("Stream data available...");
        Serial.println("STREAM PATH: " + data.streamPath());
        Serial.println("EVENT PATH: " + data.dataPath());
        Serial.println("DATA TYPE: " + data.dataType());
        Serial.println("EVENT TYPE: " + data.eventType());
        Serial.print("VALUE: ");
        printResult(data);
        Serial.println();
#endif

        FirebaseJson *json = data.jsonObjectPtr();
        size_t len = json->iteratorBegin();
        String key, value = "";
        int type = 0;
        int temp = -1;

        for (size_t i = 0; i < len; i++) {
            json->iteratorGet(i, type, key, value);
            if (key == "pin") {
                SpiderDrop.pin = getPinValue(value);
            }
            else if (key == "dropMotorPin") {
                SpiderDrop.dropMotorPin = getPinValue(value);
            }
            else if (key == "currentSensePin") {
                SpiderDrop.currentSensePin = getPinValue(value);
            }
            else if (key == "retractMotorPin") {
                SpiderDrop.retractMotorPin = getPinValue(value);
            }
            else if (key == "dropStopSwitchPin") {
                SpiderDrop.dropStopSwitchpin = getPinValue(value);
            }
            else if (key == "currentPulseDelay") {
                temp = value.toInt();
                if (temp >= 0)
                    SpiderDrop.currentPulseDelay = temp;
            }
            else if (key == "currentLimit") {
                SpiderDrop.currentLimit = value.toInt();
            }
            else if (key == "hangTime") {
                temp = value.toInt();
                if (temp >= 0)
                    SpiderDrop.hangTime = temp;
            }
            else if (key == "stayDropped") {
                Serial.print("stayDropped: ");
                Serial.println(value);
                SpiderDrop.stayDropped = value.toInt();
            }
            else if (key == "command") {
                if (value == "DROP") {
                    SpiderDrop.command = DROP;
                } else if (value == "RETRACT") {
                    SpiderDrop.command = RETRACT;
                } else if (value == "_none_") {
                    SpiderDrop.command = NONE;
                } else {
                    SpiderDrop.command = NONE;
#ifdef HAR_DEBUG
                    Serial.print("Unknown command: ");
                    Serial.println(value);
#endif                  
                }
            }
            else {
#ifdef HAR_DEBUG
        Serial.print("Skipping value... (");
        Serial.print("TYPE: ");
        Serial.print(type == FirebaseJson::JSON_OBJECT ? "object" : "array");
        Serial.print(" | KEY: ");
        Serial.print(key);
        Serial.println(")");
#endif                
            }
            newDataReceived = true;
        }
        writeLocalData();
    } else if (data.dataType() == "null") {
#ifdef HAR_DEBUG
      Serial.println("No endpoint found on backend. Creating new device...");
#endif
        writeToFirebase();
        delay(3000);
    } else {
#ifdef HAR_DEBUG
    Serial.print("Stream returned non-JSON response: ");
    Serial.println(data.dataType());
#endif
    }
}

void writeToFirebase() {
      FirebaseJson json;
      json.add("name", String(ESP.getChipId()));
      json.add("groupName", "Spider Droppers");
      json.add("pin", getPinValueFirebase(SpiderDrop.pin));
      json.add("dropMotorPin", getPinValueFirebase(SpiderDrop.dropMotorPin));
      json.add("currentSesnePin", getPinValueFirebase(SpiderDrop.currentSensePin));
      json.add("retractMotorPin", getPinValueFirebase(SpiderDrop.retractMotorPin));
      json.add("dropStopSwitchPin", getPinValueFirebase(SpiderDrop.dropStopSwitchpin));
      json.add("currentPulseDelay", SpiderDrop.currentPulseDelay);
      json.add("hangTime", SpiderDrop.hangTime);
      json.add("currentLimit", SpiderDrop.currentLimit);
      json.add("spiderState", 
        SpiderDrop.spiderState == DROPPED ? STATE_DROPPED 
        : SpiderDrop.spiderState == RETRACTING ? STATE_RETRACTING
        : STATE_RETRACTED);
      json.add("command", COMMAND_NONE);
      json.add("stayDropped", SpiderDrop.stayDropped);
      Firebase.set(firebaseDataSEND, DevicePath, json);
}

/** -------- DEBUG FUNCTION -------- **/

#if defined(HAR_DEBUG)
void printResult(StreamData &data)
{

  if (data.dataType() == "int")
    Serial.println(data.intData());
  else if (data.dataType() == "float")
    Serial.println(data.floatData(), 5);
  else if (data.dataType() == "double")
    printf("%.9lf\n", data.doubleData());
  else if (data.dataType() == "boolean")
    Serial.println(data.boolData() == 1 ? "true" : "false");
  else if (data.dataType() == "string" || data.dataType() == "null")
    Serial.println(data.stringData());
  else if (data.dataType() == "json")
  {
    Serial.println();
    FirebaseJson *json = data.jsonObjectPtr();
    //Print all object data
    Serial.println("Pretty printed JSON data:");
    String jsonStr;
    json->toString(jsonStr, true);
    Serial.println(jsonStr);
    Serial.println();
    Serial.println("Iterate JSON data:");
    Serial.println();
    size_t len = json->iteratorBegin();
    String key, value = "";
    int type = 0;
    for (size_t i = 0; i < len; i++)
    {
      json->iteratorGet(i, type, key, value);
      Serial.print(i);
      Serial.print(", ");
      Serial.print("Type: ");
      Serial.print(type == FirebaseJson::JSON_OBJECT ? "object" : "array");
      if (type == FirebaseJson::JSON_OBJECT)
      {
        Serial.print(", Key: ");
        Serial.print(key);
      }
      Serial.print(", Value: ");
      Serial.println(value);
    }
    json->iteratorEnd();
  }
  else if (data.dataType() == "array")
  {
    Serial.println();
    //get array data from FirebaseData using FirebaseJsonArray object
    FirebaseJsonArray *arr = data.jsonArrayPtr();
    //Print all array values
    Serial.println("Pretty printed Array:");
    String arrStr;
    arr->toString(arrStr, true);
    Serial.println(arrStr);
    Serial.println();
    Serial.println("Iterate array values:");
    Serial.println();

    for (size_t i = 0; i < arr->size(); i++)
    {
      Serial.print(i);
      Serial.print(", Value: ");

      FirebaseJsonData *jsonData = data.jsonDataPtr();
      //Get the result data from FirebaseJsonArray object
      arr->get(*jsonData, i);
      if (jsonData->typeNum == FirebaseJson::JSON_BOOL)
        Serial.println(jsonData->boolValue ? "true" : "false");
      else if (jsonData->typeNum == FirebaseJson::JSON_INT)
        Serial.println(jsonData->intValue);
      else if (jsonData->typeNum == FirebaseJson::JSON_FLOAT)
        Serial.println(jsonData->floatValue);
      else if (jsonData->typeNum == FirebaseJson::JSON_DOUBLE)
        printf("%.9lf\n", jsonData->doubleValue);
      else if (jsonData->typeNum == FirebaseJson::JSON_STRING ||
               jsonData->typeNum == FirebaseJson::JSON_NULL ||
               jsonData->typeNum == FirebaseJson::JSON_OBJECT ||
               jsonData->typeNum == FirebaseJson::JSON_ARRAY)
        Serial.println(jsonData->stringValue);
    }
  }
  else if (data.dataType() == "blob")
  {

    Serial.println();

    for (int i = 0; i < data.blobData().size(); i++)
    {
      if (i > 0 && i % 16 == 0)
        Serial.println();

      if (i < 16)
        Serial.print("0");

      Serial.print(data.blobData()[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
  else if (data.dataType() == "file")
  {

    Serial.println();

    File file = data.fileStream();
    int i = 0;

    while (file.available())
    {
      if (i > 0 && i % 16 == 0)
        Serial.println();

      int v = file.read();

      if (v < 16)
        Serial.print("0");

      Serial.print(v, HEX);
      Serial.print(" ");
      i++;
    }
    Serial.println();
    file.close();
  }
}
#endif