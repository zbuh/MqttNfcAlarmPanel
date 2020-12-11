//WIFI
const char ssid[] = "SSID";
const char pass[] = "PASSWORD";

//MQTT
const char* mqtt_host = "mqtt.host";
const int mqtt_port = 1883;
const char* mqtt_user = "mqtt_user";
const char* mqtt_pass = "mqtt_password";
const char* mqtt_clientId = "M5stack-alarm1";
const char* mqtt_state_topic = "alarm/state";
const char* mqtt_card_topic = "alarm/panel/garage/card";
const char* willTopic = "alarm/panel/garage/status";
const char* willMessage = "offline";
byte willQoS = 0;
boolean willRetain = true;