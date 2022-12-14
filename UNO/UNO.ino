#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "DHT.h"
#include <IRremote.h>  // thư viện hỗ trợ IR remote
// default config
// Cam bien nhiet do & do am
const int DHTPIN = A0;
const int DHTTYPE = DHT11;
DHT dht(DHTPIN, DHTTYPE);
//
bool fan_enable = false;
int fan_speed = 1;
bool temp_enable = false;
float temp_measure = 33.0;
float temp_measure_old = 00.0;
float temp_threshold = 33.0;

bool espConnected = false;

// speed by temp
// float temp_threshold_one = 30.0;
// float temp_threshold_two = 31.0;
// float temp_threshold_three = 32.0;

// Cam bien hong ngoai
const int receiverPin = A2;  // chân digital 8 dùng để đọc tín hiệu
IRrecv irrecv(receiverPin);  // tạo đối tượng IRrecv mới
decode_results results;      // lưu giữ kết quả giải mã tín hiệu

String dataSend = "";
String dataRead = "";
bool isSending = true;
bool isReading = true;

// key for json data
#define KEY_DATA_FAN_ENABLE "fanEnable"
#define KEY_DATA_TEMP_ENABLE "tempEnable"
#define KEY_DATA_TEMP_MEASURE "tempMeasure"
#define KEY_DATA_FAN_SPEED "fanSpeed"
#define KEY_DATA_TEMP_THRESHOLD "tempTh"
// function declare
void readDataFromEsp();
void handleFanState();             // convert to json data to send
void handleJsonData(String data);  // convert json data to data can handle

// define uart
#define RX 8
#define TX 9
SoftwareSerial unoEspSerial = SoftwareSerial(RX, TX);

// declare pinmode
#define FAN_CONTROL_PIN 3

// control fan
#define OFF_FAN digitalWrite(FAN_CONTROL_PIN, LOW)
#define ON_FAN digitalWrite(FAN_CONTROL_PIN, HIGH)

void setup() {
  Serial.begin(9600);
  unoEspSerial.begin(9600);
  irrecv.enableIRIn();
  pinMode(FAN_CONTROL_PIN, OUTPUT);
  OFF_FAN;
  dht.begin();
}

void loop() {
  readDataFromEsp();
  readTemp();
  controlByRemote();
  controlByTemp();
  controlFanState();
}


void readDataFromEsp() {
  // while (unoEspSerial.available()) {

  //   char readChar = (char)unoEspSerial.read();
  //   if (readChar != '\n') {
  //     dataRead += readChar;
  //   } else {
  //     handleJsonData(dataRead);
  //     dataRead = "";
  //     // espConnected = true;
  //   }
  // }
  if (unoEspSerial.available()) {
    String dataRead = unoEspSerial.readString();
    handleJsonData(dataRead);
    espConnected = true;
  }
}


void sendUart(String data) {
  Serial.println("Send " + data);
  unoEspSerial.print(data + '\n');
  unoEspSerial.flush();  // wait end
}


String jsonWriteOne(String key, String value) {

  String result = String("{\"") + key + String("\":\"") + value + String("\"}");
  return result;
}


void handleJsonData(String data) {
  Serial.println("read " + data);
  DynamicJsonBuffer jsonBuffer(200);
  JsonObject& root = jsonBuffer.parseObject(data);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  if (root.containsKey(KEY_DATA_FAN_ENABLE)) {
    String data = root[KEY_DATA_FAN_ENABLE];
    fan_enable = stringToBool(data);
    Serial.println("fan enable " + data);
  }
  if (root.containsKey(KEY_DATA_FAN_SPEED)) {
    String data = root[KEY_DATA_FAN_SPEED];
    fan_speed = data.toInt();
    Serial.println("fan speed " + data);
  }
  if (root.containsKey(KEY_DATA_TEMP_ENABLE)) {
    String data = root[KEY_DATA_TEMP_ENABLE];
    temp_enable = stringToBool(data);
    Serial.println("temp enable " + data);
  }

  if (root.containsKey(KEY_DATA_TEMP_THRESHOLD)) {
    String data = root[KEY_DATA_TEMP_THRESHOLD];
    temp_threshold = data.toFloat();
    Serial.println("temp threshold " + data);
  }
};

void controlFanState() {
  if (fan_enable) {
    ON_FAN;
    // analogWrite(FAN_CONTROL_PIN, 1000);
    // controlFanSpeed();
  } else OFF_FAN;
}

void controlFanSpeed() {
  switch (fan_speed) {
    case 1:
      Serial.println(1);
      analogWrite(FAN_CONTROL_PIN, 300);
      break;
    case 2:
      Serial.println(2);
      analogWrite(FAN_CONTROL_PIN, 700);
      break;
    case 3:
      Serial.println(3);
      analogWrite(FAN_CONTROL_PIN, 1023);
      break;
  }
}

void readTemp() {
  delay(1000);
  temp_measure = dht.readTemperature();
  Serial.println(temp_measure);
  if (abs(temp_measure - temp_measure_old) >= 0.1 && espConnected) {
    Serial.println(temp_measure - temp_measure_old);
    temp_measure_old = temp_measure;
    String dataSend = jsonWriteOne(KEY_DATA_TEMP_MEASURE, String(temp_measure));
    sendUart(dataSend);
  }
}

void controlByTemp() {
  if (!temp_enable) return;
  if (temp_measure > temp_threshold) {
    if (fan_enable) return;
    Serial.println("on");
    fan_enable = true;
    String dataSend = jsonWriteOne(KEY_DATA_FAN_ENABLE, boolToString(fan_enable));  // send new value
    sendUart(dataSend);
  } else {
    if (!fan_enable) return;
    fan_enable = false;
    Serial.println("off");
    String dataSend = jsonWriteOne(KEY_DATA_FAN_ENABLE, boolToString(fan_enable));  // send new value
    sendUart(dataSend);
  }
}

void controlByRemote() {
  if (irrecv.decode(&results))  // nếu nhận được tín hiệu
  {


    unsigned int value = results.value;
    if (temp_enable && value != 4335) {  // nếu điều khiển bằng remote tắt chế độ bật tắt bằng nhiệt độ
      Serial.print("off");
      temp_enable = false;
      sendUart(jsonWriteOne(KEY_DATA_TEMP_ENABLE, "false"));
    }
    Serial.println(value);
    switch (value) {
      case 26775:
      case 65535:
        fan_enable = !fan_enable;
        sendUart(jsonWriteOne(KEY_DATA_FAN_ENABLE, boolToString(fan_enable)));
        delay(500);
        break;
      case 4335:
        temp_enable = !temp_enable;
        Serial.println(temp_enable);
        sendUart(jsonWriteOne(KEY_DATA_TEMP_ENABLE, boolToString(temp_enable)));
        delay(500);
        break;
        // case 17085:
        //   fan_speed = 1;
        //   break;
        // case 19125:
        //   fan_speed = 2;
        //   break;
        // case 21165:
        //   fan_speed = 3;
        //   break;
    }
  }
  irrecv.resume();  // Receive the next value
}

bool stringToBool(String value) {
  if (value == "true") return true;
  return false;
}

String boolToString(bool value) {
  if (value) return "true";
  return "false";
}
