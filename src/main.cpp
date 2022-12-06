#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet2.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <EthernetUdp2.h>
#include <Button2.h>
#include <lighting.h>
#define NUM_LEDS 8
#define DATA_PIN 6
//#define SERIAL_DEBUG 9600
//#define LOG_TOPIC "englishmile/sensors/log"
#define PIN_SIGNAL_A 2
#define PIN_SIGNAL_B 3
#define PIN_BUTTON 4
#define PIN_IR 5
#define ETH_RESET_PIN 9 //для выключателя 234 нужно заменить на 5
#define COMMON_GROUND
//#define ROTARY_BACKLIGHT
#define BACKLIGHT_PORT 7031
//#define IP_END_BYTE 233 // Выключатель у кровати в спальне моя сторона
//#define IP_END_BYTE 234 // Выключатель у кровати в спальне сторона Инны
//#define IP_END_BYTE 235 // Выключатель у входа в спальне
//#define IP_END_BYTE 236  // Выключатель в гостинной над столом
#define IP_END_BYTE 237 // Выключатель в ванной

//#define IP_END_BYTE 238 // Выключатель в гостинная у входа
//#define IP_END_BYTE 239 // Выключатель над столешницей
//#define IP_END_BYTE 240 // Выключатель детская у входа
#define SETUP_TOPIC0 "englishmile/sensors/237/master"
#define SETUP_TOPIC1 "englishmile/sensors/237/slave"
#define SETUP_TOPIC2 "englishmile/sensors/237/backlight"
#define MQTT_NAME "englishmile-light-sensor-237"
#define MAXIMUM_JSON_LEN 60 
#define DEBOUNCE 50
#define LONGCLICK 350
#define UDP_PACKET_SIZE 7
#define DIRECTION 0
#define BIT_INCREMENT 1
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, IP_END_BYTE}; //last octet like last ip byte
#ifdef DATA_PIN
CRGB leds[NUM_LEDS];
#endif

IPAddress router_IP(192,168,55,1);
IPAddress sensor_IP(192,168,55,IP_END_BYTE);
IPAddress mqtt_IP(192,168,55,1);
StaticJsonDocument<MAXIMUM_JSON_LEN> json_message;
EthernetClient ethClient;
EthernetUDP Udp;
PubSubClient mqtt(ethClient);
Button2 buttonA = Button2(PIN_BUTTON);
#ifdef PIN_IR
Button2 buttonB = Button2(PIN_IR);
#endif
Light light_main = Light();
Light light_slave = Light();
volatile int light_increment = 0; 
volatile int lastEncoded = 0;
volatile int startButton = 0;
#ifdef DATA_PIN
byte lastBackLightBrightness = 3;
byte color1 = 255;
byte color2 = 255;
byte color3 = 255;
#endif
unsigned long pooling = 0;


void serviceEncoderInterrupt() {
  int signalA = !digitalRead(PIN_SIGNAL_A);
  int signalB = !digitalRead(PIN_SIGNAL_B);

  int encoded = (signalB << 1) | signalA;  // converting the 2 pin value to single number
  int sum  = (lastEncoded << 2) | encoded; // adding it to the previous encoded value
if (DIRECTION) {
  if(sum == 0b0001 || sum == 0b0111 || sum == 0b1110 || sum == 0b1000) light_increment ++;
  if(sum == 0b0010 || sum == 0b1011 || sum == 0b1101 || sum == 0b0100) light_increment --;
}
else {
  if(sum == 0b0001 || sum == 0b0111 || sum == 0b1110 || sum == 0b1000) light_increment --;
  if(sum == 0b0010 || sum == 0b1011 || sum == 0b1101 || sum == 0b0100) light_increment ++;
}
  lastEncoded = encoded; // store this value for next time
}

// Обработка нажатия кнопки
void pressed(Button2& btn) {
    if (btn == buttonA) {
    pooling = millis();
    }
}

void long_press() {
  String test_str;
  if (light_slave.topicSet.length() > 0) {
  light_slave.Switch();
  if (light_slave.state){
  json_message["state"] = "ON"; 
  }
  else {
  json_message["state"] = "OFF";  
  }
  serializeJson(json_message, test_str);
  char charBuf[test_str.length() + 1];
  test_str.toCharArray(charBuf,test_str.length() + 1); 
  char topicBuf[light_slave.topicSet.length() + 1];
  light_slave.topicSet.toCharArray(topicBuf,light_slave.topicSet.length() + 1);
  mqtt.publish(topicBuf, charBuf);
  }
  else{
  #ifdef LOG_TOPIC
  mqtt.publish(LOG_TOPIC, "Cветильник 2 не настроен");
  #endif
  }
  json_message.clear();
}

void released(Button2& btn) {
    String test_str;  
    if ((btn == buttonA) && (pooling > 0) && (btn.wasPressedFor() < LONGCLICK)) {
    pooling = 0;
    if (light_main.topicSet.length() > 0){
        light_main.Switch();
        if (light_main.state){
          json_message["state"] = "ON"; 
        }
        else {
          json_message["state"] = "OFF";  
        }
        serializeJson(json_message, test_str);
        char charBuf[test_str.length() + 1];
        test_str.toCharArray(charBuf,test_str.length() + 1); 
        char topicBuf[light_main.topicSet.length() + 1];
        light_main.topicSet.toCharArray(topicBuf,light_main.topicSet.length() + 1);
        mqtt.publish(topicBuf, charBuf);
        json_message.clear();
      }
    }
    else if ((btn == buttonA) && (pooling > 0) && (btn.wasPressedFor() >= LONGCLICK)) {
    pooling = 0;
    long_press();
  }
}

void soft_Reset(){
asm volatile ("jmp 0x0000");
}

void mqtt_setup_connect(){
  mqtt.connect(MQTT_NAME);
  delay(500);
  String test_str;
  if (mqtt.connected()) {
  mqtt.subscribe(SETUP_TOPIC0);
  mqtt.subscribe(SETUP_TOPIC1);
  mqtt.subscribe(SETUP_TOPIC2);
  }
  else {
  delay(1000);
  }
  }


void update_state(Light &light, const char* state, long brightness){
if (String(state) == "ON") {
light.state = 1;
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC,"Включен");
#endif
}
else{
light.state = 0;
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC,"Выключен");
#endif
}
if (brightness > 0) {
light.brightness = brightness;
}
}

void on_message(char* topic, byte* payload, unsigned int length) {
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, topic);
#endif  
char json[length]={};
unsigned int i=0;
for (i=0;i<length;i++) {
   json[i] =(char)payload[i];
  }
DeserializationError err = deserializeJson(json_message, json);
if (err) {
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, "Serelization Error");
#endif
}
else {
//Начало блока разбора по темам

if (String(topic) == SETUP_TOPIC0) { // Обработка топика SETUP0
auto u_id = json_message["light_id"].as<unsigned long>();
auto set = json_message["set_topic"].as<const char*>();
auto state = json_message["state_topic"].as<const char*>();
auto port = json_message["port"].as<long>();
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, "id main ОК");
#endif

light_main.Setup(u_id, String(state), String(set), port);
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, "change state light1:");
char errBuf1[light_main.topicSet.length() + 1];
light_main.topicSet.toCharArray(errBuf1,light_main.topicSet.length() + 1); 
mqtt.publish(LOG_TOPIC, errBuf1);
#endif
mqtt.subscribe(state);
}

if (String(topic) == SETUP_TOPIC1) { // Обработка топика SETUP1
auto u_id = json_message["light_id"].as<unsigned long>();
auto set = json_message["set_topic"].as<const char*>();
auto state = json_message["state_topic"].as<const char*>();
auto port = json_message["port"].as<unsigned int>();
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, "id slave ОК");
#endif
light_slave.Setup(u_id, String(state), String(set), port);
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, "change state light2:");
char errBuf2[light_slave.topicSet.length() + 1];
light_slave.topicSet.toCharArray(errBuf2,light_slave.topicSet.length() + 1); 
mqtt.publish(LOG_TOPIC, errBuf2);
#endif
mqtt.subscribe(state);
}

if (String(topic) == SETUP_TOPIC2) {
color1 = json_message["green"].as<byte>();
color2 = json_message["red"].as<byte>();
color3 = json_message["blue"].as<byte>();
}

if (String(topic) == light_main.topicState) { //обработка сообщения состояния для первого светильника
auto brightness = json_message["brightness"].as<long>();;
auto state = json_message["state"].as<const char*>();
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC,"Получено состояние светильника 1");
#endif
update_state(light_main, state, brightness);
}
if (String(topic) == light_slave.topicState) { //обработка сообщения состояния для второго светильника
auto brightness = json_message["brightness"].as<long>();;
auto state = json_message["state"].as<const char*>();
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC,"Получено состояние светильника 2");
#endif
update_state(light_slave, state, brightness);
}

} //Конец блока разбора по темам
json_message.clear();
}

#ifdef DATA_PIN
void set_color(){
for (byte j=0; j<NUM_LEDS; j++) {
leds[j] = CRGB(round(color1*lastBackLightBrightness/255),round(color2*lastBackLightBrightness/255),round(color3*lastBackLightBrightness/255));
}
FastLED.show();
}
#endif

void motion_on(Button2& btn) {
mqtt.publish("wc/motion", "1"); 
}

void motion_off(Button2& btn) {
mqtt.publish("wc/motion", "0");
}


void setup() {
  #ifdef DATA_PIN
  //initialize the backlight diods
  FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);
  #endif
  #ifdef SERIAL_DEBUG
  Serial.begin (SERIAL_DEBUG);
  #endif
  // setup encoder pins
  pinMode(PIN_SIGNAL_A, INPUT); 
  pinMode(PIN_SIGNAL_B, INPUT);
  pinMode(PIN_BUTTON, INPUT);
  #ifdef PIN_IR
  pinMode(PIN_IR, INPUT);
  #endif

  pinMode(ETH_RESET_PIN, OUTPUT);
  digitalWrite(ETH_RESET_PIN, LOW);
  //setup button
  #ifdef COMMON_GROUND
  digitalWrite(PIN_BUTTON, HIGH);
  #endif
  //buttonA.setPressedHandler(pressed);
  //buttonA.setReleasedHandler(released);
  buttonA.setDebounceTime(DEBOUNCE);
  buttonA.setPressedHandler(pressed);
  buttonA.setReleasedHandler(released);
  
  #ifdef PIN_IR
  buttonB.setDebounceTime(DEBOUNCE);
  buttonB.setPressedHandler(motion_on);
  buttonB.setReleasedHandler(motion_off);
  #endif

  // setup rotary encoder
  #ifdef COMMON_GROUND
  // enable pullup resistors on interrupt pins
  digitalWrite(PIN_SIGNAL_A, HIGH);
  digitalWrite(PIN_SIGNAL_B, HIGH);
  #endif
  attachInterrupt(0, serviceEncoderInterrupt, CHANGE); 
  attachInterrupt(1, serviceEncoderInterrupt, CHANGE);
  // wait little bit for init all and start ethernet
  delay(50);
  digitalWrite(ETH_RESET_PIN, HIGH);
  delay(50);
  Ethernet.begin(mac, sensor_IP);
  delay(500);
  mqtt.setServer(mqtt_IP, 1883);
  mqtt.setCallback(on_message);
  mqtt_setup_connect();
  Udp.begin(BACKLIGHT_PORT);
  #ifdef DATA_PIN
  set_color();
  #endif
  delay(200);
 }


void send_UDP_data(int increment){
light_increment=0;
byte packetBuffer[UDP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
memset(packetBuffer, 0, UDP_PACKET_SIZE);
if (light_main.state){
packetBuffer[0] = IP_END_BYTE;
packetBuffer[1] = light_main.unic_id & 0xff;
packetBuffer[2] = light_main.unic_id  >> 8;
packetBuffer[3] = light_main.unic_id  >> 16;
packetBuffer[4] = light_main.unic_id  >> 24;
packetBuffer[5] = increment & 0xff;
packetBuffer[6] = increment  >> 8;
Udp.beginPacket(router_IP, light_main.port);
Udp.write(packetBuffer, UDP_PACKET_SIZE);
Udp.endPacket();
#ifdef SERIAL_DEBUG
Serial.println("Main light Change");
Serial.print("Increment: ");
Serial.println(increment);
Serial.print("First byte: ");
Serial.println(increment & 0xff);
Serial.print("Second byte: ");
Serial.println(increment  >> 8);
#endif
}
else if (light_slave.state){
packetBuffer[0] = IP_END_BYTE;
packetBuffer[1] = light_slave.unic_id & 0xff;
packetBuffer[2] = light_slave.unic_id  >> 8;
packetBuffer[3] = light_slave.unic_id  >> 16;
packetBuffer[4] = light_slave.unic_id  >> 24;
packetBuffer[5] = increment & 0xff;
packetBuffer[6] = increment  >> 8;
Udp.beginPacket(router_IP, light_slave.port);
Udp.write(packetBuffer, UDP_PACKET_SIZE);
Udp.endPacket();
#ifdef SERIAL_DEBUG
Serial.println("Slave light change");
Serial.print("Increment: ");
Serial.println(increment);
Serial.print("First byte: ");
Serial.println(increment & 0xff);
Serial.print("Second byte: ");
Serial.println(increment  >> 8);
#endif
}
else{
#ifdef SERIAL_DEBUG
Serial.println("Change backlight: ");
#endif
#ifdef DATA_PIN
#ifdef ROTARY_BACKLIGHT  
if (((lastBackLightBrightness + increment) > 0) && ((lastBackLightBrightness + increment) <= 255)){
lastBackLightBrightness = lastBackLightBrightness + increment;  
}
else if ((lastBackLightBrightness + increment) <=0){
lastBackLightBrightness = 0;
}
else if ((lastBackLightBrightness + increment) > 255){
lastBackLightBrightness = 255;
}
set_color();
#endif
#endif
}
}

void loop() {
// Подключаемся к MQTT если не подключены
if (!mqtt.connected()){
  mqtt_setup_connect();
}
#ifdef DATA_PIN
// Получение цвета подсветки
byte packetBuffer[5];
int packetSize = Udp.parsePacket();
if (packetSize == 5){
Udp.read(packetBuffer, 5);
if ((packetBuffer[0] + packetBuffer[1] + packetBuffer[2]) == ((packetBuffer[3]<<8) | (packetBuffer[4]))){
lastBackLightBrightness = 255;
color1 = packetBuffer[0];
color2 = packetBuffer[1];
color3 = packetBuffer[2];
set_color();
}
}
#endif
//rotary encoder signal need to bee push by network (UDP), mqtt is totaly slow....
if (light_increment > BIT_INCREMENT) {
    send_UDP_data(light_increment);
}
if (light_increment < -BIT_INCREMENT) {
    send_UDP_data(light_increment);
}

buttonA.loop();
buttonB.loop();
// Зажжем второй светильник, если кнопка зажата долго
if ((pooling > 0) && ((millis()-pooling) > LONGCLICK)) {
  pooling = 0;
  long_press();
}
mqtt.loop();
}