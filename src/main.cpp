#include <Arduino.h>
#include <IBMIOTF8266.h>
#include <Wire.h>
#include <SSD1306.h> 
#include <DHTesp.h>

SSD1306               display(0x3c, 4, 5, GEOMETRY_128_32);

String user_html = "";

char*               ssid_pfix = (char*)"IOTther";

const int           pulseA = 12;
const int           pulseB = 13;
const int           pushSW = 2;
volatile int        lastEncoded = 0;
volatile long       encoderValue = 70;

#define             DHTPIN 14
DHTesp              dht;
#define             interval 2000
unsigned long       lastDHTReadMillis = 0;

unsigned long       lastPublishMillis = - pubInterval;
float               humidity;
float               temperature;
float               tgtT;
void gettemperature();


IRAM_ATTR void handleRotary() {
    // Never put any long instruction
    int MSB = digitalRead(pulseA); //MSB = most significant bit
    int LSB = digitalRead(pulseB); //LSB = least significant bit

    int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
    int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value
    if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue ++;
    if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue --;
    lastEncoded = encoded; //store this value for next time
    if (encoderValue > 255) {
        encoderValue = 255;
    } else if (encoderValue < 0 ) {
        encoderValue = 0;
    }
    lastPublishMillis = millis() - pubInterval + 200;

}

void publishData() {

    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");


    char dht_buffer[10];
    char dht_buffer2[10];
    char dht_buffer3[10];

    gettemperature();
    display.setColor(BLACK);
    display.fillRect(80, 11, 100, 33);
    display.setColor(WHITE);
    sprintf(dht_buffer, "%2.1f", temperature);
    data["temperature"] = dht_buffer;
    display.drawString(80, 11, dht_buffer);

    sprintf(dht_buffer3, "%2.1f", humidity);
    data["humidity"] = dht_buffer3;

    tgtT = map(encoderValue, 0, 255, 10, 50);
    sprintf(dht_buffer2, "%2.1f", tgtT);
    data["target"] = dht_buffer2;
    display.drawString(80, 22, dht_buffer2);
    display.display();

    data["rotary"] = encoderValue;

    serializeJson(root, msgBuffer);
    client.publish(publishTopic, msgBuffer); 

 
  
}



void readDHT22() {
    unsigned long currentMillis = millis();

    if(currentMillis - lastDHTReadMillis >= interval) {
        lastDHTReadMillis = currentMillis;

        humidity = dht.getHumidity();              // Read humidity (percent)
        temperature = dht.getTemperature();        // Read temperature as Fahrenheit
    }
}


void handleUserCommand(JsonDocument* root) {
    JsonObject d = (*root)["d"];
    if(d.containsKey("target")) {
      tgtT = d["target"];
      encoderValue = map(tgtT, 10, 50, 0, 255);
      lastPublishMillis = - pubInterval;
    }

}

void message(char* topic, byte* payload, unsigned int payloadLength) {
    byte2buff(msgBuffer, payload, payloadLength);
    StaticJsonDocument<512> root;
    DeserializationError error = deserializeJson(root, String(msgBuffer));
  
    if (error) {
        Serial.println("handleCommand: payload parse FAILED");
        return;
    }

    handleIOTCommand(topic, &root);
    if (!strncmp(updateTopic, topic, cmdBaseLen)) {
// USER CODE EXAMPLE : meta data update

    } else if (!strncmp(commandTopic, topic, cmdBaseLen)) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(pulseA, INPUT_PULLUP);
    pinMode(pulseB, INPUT_PULLUP);
    attachInterrupt(pulseA, handleRotary, CHANGE);
    attachInterrupt(pulseB, handleRotary, CHANGE);
    dht.setup(DHTPIN, DHTesp::DHT22);
    display.init();
    display.flipScreenVertically();
    display.drawString(35, 0, "IOT thermostat");
    display.drawString(20, 11, "Current : ");
    display.drawString(20, 22, "Target : ");
    display.display();
    initDevice();
    // If not configured it'll be configured and rebooted in the initDevice(),
    // If configured, initDevice will set the proper setting to cfg variable

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // main setup
    Serial.printf("\nIP address : "); Serial.println(WiFi.localIP());
// USER CODE EXAMPLE : meta data to local variable
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;
    lastPublishMillis = - pubInterval;
// USER CODE EXAMPLE
    
    set_iot_server();
    client.setCallback(message);
    iot_connect();
    
    
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
// USER CODE EXAMPLE : main loop
//     you can put any main code here, for example, 
//     the continous data acquistion and local intelligence can be placed here
// USER CODE EXAMPLE
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
    readDHT22();
    Serial.printf("%.1f\t %.1f\n", temperature, humidity);

    
}

void gettemperature() {
  unsigned long currentMillis = millis();
  if(currentMillis - lastDHTReadMillis >= interval) {
  lastDHTReadMillis = currentMillis;
  humidity = dht.getHumidity(); // Read humidity (percent)
  temperature = dht.getTemperature(); // Read temperature as Fahrenheit
  }
}
