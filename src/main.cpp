// M5Stack RFID MQTT alarm panel
// Author: Nuno Martins
// Version: 0.1
// Date: 09-12-2020

#include <SPIFFS.h>
#include <M5Stack.h>
#include <M5ez.h>
#include <ezTime.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <MFRC522_I2C.h>

#include "config.h"

// set MQTT timeout parameters to overcome constant reconnection
#define MQTT_SOCKET_TIMEOUT 30
#define MQTT_MAX_PACKET_SIZE 512
#define MQTT_KEEPALIVE 60

// Clock in Status bar
#define M5EZ_CLOCK 1

// RGB M5 Base
#define M5STACK_FIRE_NEO_NUM_LEDS 10
#define M5STACK_FIRE_NEO_DATA_PIN 15

const char* mqtt_command_topic = "alarm/command";
const char* mqtt_state_armed_away = "armed_away";
const char* mqtt_state_armed_home = "armed_home";
const char* mqtt_state_armed_night = "armed_night";
const char* mqtt_state_disarmed = "disarmed";
const char* mqtt_state_pending = "pending";
const char* mqtt_state_arming = "arming";
const char* mqtt_state_triggered = "triggered";

const char* display_state_full ="Fully Armed";
const char* display_state_home ="Armed Home";
const char* display_state_night ="Armed Night";
const char* display_state_disarmed ="Disarmed";
const char* display_state_pending ="Pending";
const char* display_state_arming ="Arming";
const char* display_state_triggered ="Intrusion!!!";
const char* display_present_card = "Please present Card!";


WiFiClient wifi_client;
PubSubClient client(wifi_client);

// Current alarm status
String alarmStatus = "";

// SCREEN STATUS
int screenTimeout = 10000; // screen timeout in mSec
int screenStatus = 1;

// BEEP STATUS
int beepDelay=1;
long beepTimer=0;
bool beepStatus=false;

MFRC522 mfrc522(0x28); 

// RGB
Adafruit_NeoPixel strip = Adafruit_NeoPixel(M5STACK_FIRE_NEO_NUM_LEDS, M5STACK_FIRE_NEO_DATA_PIN, NEO_GRB + NEO_KHZ800);
uint32_t last_color = strip.Color(0,0,0);


// RGB Methods
void stop_rgb(){
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
}

void start_rgb(uint32_t color){
  last_color = color;
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// Beeping Methods
void beep(int tone){
  M5.Speaker.beep();
}

void start_beep(int delay) {
  M5.Speaker.setVolume(100);
  M5.Speaker.update();
  beepDelay = delay;
  beepStatus = true;
}

void stop_beep(){
  if (beepStatus){
    M5.Speaker.setVolume(0);
    M5.Speaker.update();
    beepStatus = false;
    beepTimer=0;
  }
}

void beep_update(){
  if (beepStatus)
  {
    if (beepDelay == 0 || (beepTimer % beepDelay == 0) ){
        M5.Speaker.beep();
    }
    beepTimer++;
  }
}

// Screen Methods
void screen_on(int duration=10000){
  screenTimeout = duration;
  M5.Lcd.wakeup();
  M5.Lcd.setBrightness(50);
  screenStatus = 1;  
  start_rgb(last_color);
}

void screen_off()
{
  M5.Lcd.setBrightness(0);
  M5.Lcd.sleep();
  screenStatus = 0;
  screenTimeout = 0;
}

void screen_update()
{
  // screen is off, do nothing
  if (screenStatus == 0){
    return;
  }

  if (screenTimeout >= 200){
    screenTimeout = screenTimeout-200;
  }else{
    screenTimeout = 0;
  }

  // If screenTimeout is zero then reset timer
  if (screenTimeout == 0){
      for (int i = 50; i >= 0; i--) {
        M5.Lcd.setBrightness(i);
        delay(20);
      }
      screen_off();
      stop_rgb();
    }
}

// Alarm Status images
void show_state(const char* file){
  M5.Lcd.drawJpgFile(SPIFFS, file, 100, 60);
}

// MQTT topic update
void callback(char* topic, byte* payload, unsigned int length) {
  String topicText= "";
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);  
    topicText +=((char)payload[i]);
  }
  Serial.println();

  if (topicText == mqtt_state_armed_home || topicText == mqtt_state_armed_night || topicText == mqtt_state_armed_away){
    beep(1000);
    show_state("/armed.jpg");
    ez.header.title(topicText);
    alarmStatus = topicText;
    start_rgb(strip.Color(0,0,255));
    stop_beep();
    screen_on();
  }
  
  if (topicText == mqtt_state_disarmed){    
    show_state("/disarmed.jpg");
    ez.header.title(display_state_disarmed);
    alarmStatus = mqtt_state_disarmed;
    start_rgb(strip.Color(0,255,0));
    stop_beep();
    screen_on();
  }
  
  if (topicText == mqtt_state_triggered){
    show_state("/triggered.jpg");
    ez.header.title(display_state_triggered);
    alarmStatus = mqtt_state_triggered;
    start_rgb(strip.Color(255,0,0));
    start_beep(4);
    screen_on();
  }
    if (topicText == mqtt_state_pending){
    show_state("/pending.jpg");
    ez.header.title(display_state_pending);
    alarmStatus = mqtt_state_pending;
    start_rgb(strip.Color(255,69,0));
    start_beep(10);
    screen_on();
  }
    if (topicText == mqtt_state_arming){
    show_state("/pending.jpg");
    ez.header.title(display_state_arming);
    alarmStatus = mqtt_state_arming;
    start_rgb(strip.Color(255,69,0));
    start_beep(15);
    screen_on();
  }
  
  ez.header.show();
}

// Wifi + Mqtt reconnect loop
void reconnect() {
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, pass);
    }
    Serial.print("Attempting MQTT connection...");
   if (client.connect(mqtt_clientId,mqtt_user,mqtt_pass)) {
      Serial.println("connected");
      client.subscribe(mqtt_state_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Setup
void setup(){
    M5.begin(true, false, true, true);

    // RGB strips
    strip.begin(); 
    strip.setBrightness(50);
    strip.show(); 

    // SPIFFS start
    SPIFFS.begin(true);

    // NTP
    ezt::setInterval(3600);
    
    // Setup Wifi
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Screen 
    ez.begin();
    M5.Lcd.fillScreen(TFT_BLACK);
    ez.header.shown();
    
    // Initialize RFID reader
    mfrc522.PCD_Init();            
    
    client.setServer(mqtt_host, mqtt_port);
    client.setCallback(callback);
    reconnect();
}

// Alarm status update routine
void alarm_status_update(){

  if (alarmStatus == mqtt_state_pending || alarmStatus == mqtt_state_arming || alarmStatus == mqtt_state_triggered){
    screen_on();
  }
}

// read card and send via MQTT
void read_card_mqtt(){
  byte readCard[4];

  // Read serial and sent via MQTT
  Serial.print(F("Card UID:"));
  char card_id[mfrc522.uid.size*3+1];
  memset(card_id, 0, sizeof(card_id));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    readCard[i] = mfrc522.uid.uidByte[i];
    sprintf(&card_id[i * 3], "%02X-", mfrc522.uid.uidByte[i]);
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  } 
  card_id[mfrc522.uid.size*3-1] = '\0';
  
  // Turn on screen after valid read
  screen_on();

  Serial.println();
  client.publish(mqtt_card_topic, "");
  client.publish(mqtt_card_topic, card_id); 
}

// Main Loop
void loop(){
  if (!client.connected()) {
    reconnect();
  }
  // mqtt update
  client.loop();

  // Look for button presses
  if (M5.BtnA.wasPressed()){
    Serial.println("--> BtnA");
    screen_on();
  }
  if (M5.BtnB.wasPressed()){
    Serial.println("--> BtnB");
    screen_on();
  }
  if (M5.BtnC.wasPressed()){
    Serial.println("--> BtnC");
    screen_on();
  }     
  ez.buttons.poll();
  
  // Screen status update
  screen_update();

  // Beep update
  beep_update();
  
  // alarm status update
  alarm_status_update();  
  // Look for cards
  if ( mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial() ) {
    read_card_mqtt();
    delay(2000);
  }

  // M5 platform update
  M5.update();
}

