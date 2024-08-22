#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/************************* WiFi Access Point *********************************/

#define WLAN_SSID       ""
#define WLAN_PASS       ""

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    ""
#define AIO_KEY         ""

/************ Global State (you don't need to change this!) ******************/

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

Adafruit_MQTT_Publish countercoin = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/counter_coin");
Adafruit_MQTT_Subscribe getinfo = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/get_info");

/*************************** Sketch Code ************************************/

void MQTT_connect();

const int LDR_PIN = 0; // GPIO0

enum State {
  DRAWER_CLOSED,
  DRAWER_OPEN,
  OPEN_FOR_TOO_LONG
};

State currentState = DRAWER_CLOSED;
unsigned long drawerOpenTime = 0;
unsigned long lastReportTime = 0;
const unsigned long openTimeout = 5000;
const unsigned long reportInterval = 5000;

int coinCount = 0;

void setup() {
  Serial.begin(9600);
  pinMode(LDR_PIN, INPUT_PULLUP);
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  mqtt.subscribe(&getinfo);
}

void loop() {
  MQTT_connect();

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &getinfo) {
      Serial.print(F("Got: "));
      Serial.println((char *)getinfo.lastread);
    }
  }

  unsigned long currentMillis = millis();
  int sensorValue = digitalRead(LDR_PIN);

  Serial.print("Sensor Value: ");
  Serial.println(sensorValue);

  switch (currentState) {
    case DRAWER_CLOSED:
      if (sensorValue == LOW) {
        currentState = DRAWER_OPEN;
        drawerOpenTime = currentMillis;
      }
      break;
      
    case DRAWER_OPEN:
      if (sensorValue == HIGH) {
        coinCount++;
        sendCoinCount();
        currentState = DRAWER_CLOSED;
      } else if (currentMillis - drawerOpenTime > openTimeout) {
        currentState = OPEN_FOR_TOO_LONG;
        lastReportTime = currentMillis;
      }
      break;
      
    case OPEN_FOR_TOO_LONG:
      if (sensorValue == HIGH) {
        coinCount++;
        sendCoinCount();
        currentState = DRAWER_CLOSED;
      } else if (currentMillis - lastReportTime > reportInterval) {
        sendOpenTooLongAlert();
        lastReportTime = currentMillis;
      }
      break;
  }
}

void sendCoinCount() {
  Serial.print(F("\nSending coin val "));
  Serial.print(coinCount);
  Serial.print("...");
  if (!countercoin.publish(coinCount)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }
}

void sendOpenTooLongAlert() {
  String payload = "Drawer open for too long!";
  if (!countercoin.publish(coinCount)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }
}

void MQTT_connect() {
  int8_t ret;
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
    retries--;
    if (retries == 0) {
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}
