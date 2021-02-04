//Program to run on an ESP8266(wemosD1) inside the Retro TV Build
//Takes the inputs of two rotary encoders for the front knobs
//Outputs commands via an IR led to the TV
//Adds wifi functionality for providing commands in NodeRed/Google

//Requires decoding of original TV remote commands

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

//Wifi and MQTT vars
const char* ssid = ***;
const char* password =  ***;
const char* mqttServer = "192.168.1.***";
const int mqttPort = 1883;
int holding_var = 0;

// Rotary Encoder Inputs
//Rot1 - Volume:
const int CLK1 = 5; //pin D1
const int DT1 = 4; //pin D2
//const int SW1 = 1; //pin for the click in btn

//Rot2 - Power:
const int CLK2 = 14; //pin D5
const int DT2 = 12; //pin D6
//const int SW2 = 13; // pin D7 pin for the click in btn

//IR LED Pin (via NPN)
IRsend irsend(16); 

//clicker states
int nowState1;
int prevState1;
int nowState2;
int prevState2;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
Serial.begin(9600);
irsend.begin();
//Networking Stuff Start ===========================>
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
   WiFi.hostname("ESP_RetroTV");
client.setServer(mqttServer, mqttPort);
client.setCallback(callback);
 while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
 // Create a random client ID
    String clientId = "RetroTV_";
    clientId += String(random(0xffff), HEX);
if (client.connect(clientId.c_str())) {  
Serial.println("connected");  
 } else {
 Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
      }
  }
//OTA stuff --
ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
  Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //end of OTA stuff ---
 client.publish("esp/test", "Hello from RetroTV"); //handshake
 //subscribe to RetroTV topic
client.subscribe("esp/RetroTV");
//Netoworking Stuff End ===============================<

//Set up Pins
pinMode(CLK1,INPUT);
pinMode(DT1,INPUT);
//pinMode(SW1,INPUT); //pin for the click in btn

pinMode(CLK2,INPUT);
pinMode(DT2,INPUT);
//pinMode(SW2,INPUT); // click in btn

//intialise click state
prevState1 = digitalRead(CLK1);
prevState2 = digitalRead(CLK2);
}

void loop() {

//Network Stuff =============>
ArduinoOTA.handle();
  if (!client.connected()) {
    reconnect();
  }
//End of Network Stuff ======<

   //read RE states
nowState1 = digitalRead(CLK1);
nowState2 = digitalRead(CLK2);

//if rotation has moved (a rotory 'click' = high then low sig, so we just read the high)
//Rot1:
if (nowState1 != prevState1 && nowState1 ==1){
  if (digitalRead(DT1) != nowState1){
    Serial.println("Rot1 - CCW rotation");
    //send volume down ####
    irsend.sendNEC(0x20DF40BF,32);
  } else {
    Serial.println("Rot1 - CW rotation");
    irsend.sendNEC(0x20DFC03F,32);
         } 
  }
//Rot2:
//The power button is a toggle on TV (ON/OFF). So will fire same code regardless of direction
if (nowState2 != prevState2 && nowState2 ==1){
 // if (digitalRead(DT1) != nowState1){
   Serial.println("Rot2 - rotation");
    irsend.sendNEC(0x20DF10EF,32);
 // } else {
 //   Serial.print("Rot1 - CW rotation");
    //irsend.sendNEC(0x20DF10EF,32);
 //        }
  }
prevState1 = nowState1;
prevState2 = nowState2;

client.loop();
delay(1);
} //end of loop

//Functions ==================================

//reconnect function for MQTT Dropout
void reconnect() {
  Serial.println("Reconnect activated");
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");


// Create a random client ID
    String clientId = "RetroTV_";
    clientId += String(random(0xffff), HEX);
if (client.connect(clientId.c_str())) {  
      Serial.println("connected");
       delay(1000);
        client.subscribe("esp/RetroTV");
        client.publish("esp/test", "Hello from RetroTV(recon)");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
//String to Hex function for translating IR codes
uint64_t StrToHex(const char* str)
{
  return (uint64_t) strtoull(str, 0, 16);
}

//Callback function for recieving messages and sending them thru IR ------>
void callback(char* topic, byte* payload, unsigned int length) {
  String messageTemp;
  //get the message, write to messageTemp char array
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i]; 
  }
  //send message as IR signal - with Hex conversion
   irsend.sendNEC(StrToHex(messageTemp.c_str()),32);
}

//end of callback ----------------------------------------->
