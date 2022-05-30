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
#define SERIAL_DEBUG 9600
#define PIN_SIGNAL_A 2
#define PIN_SIGNAL_B 3
#define PIN_BUTTON 4
#define ETH_RESET_PIN 9
#define COMMON_GROUND
#define BRIGHTNESS_PORT 7031
#define SETUP_TOPIC0 "englishmile/sensors/233/master"
#define SETUP_TOPIC1 "englishmile/sensors/233/slave"
//#define LOG_TOPIC "englishmile/sensors/log"
#define MAXIMUM_JSON_LEN 60 
#define DEBOUNCE_MS 50
//#define LONGCLICK_MS 500
//#define DOUBLECLICK_MS 400
// Сделать уникальным для каждого сенсора
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xA3};
CRGB leds[NUM_LEDS];
IPAddress router_IP(192,168,55,1);
IPAddress sensor_IP(192,168,55,233);
IPAddress mqtt_IP(192,168,55,1);
StaticJsonDocument<MAXIMUM_JSON_LEN> json_message;
EthernetClient ethClient;
EthernetUDP Udp;
PubSubClient mqtt(ethClient);
Button2 buttonA = Button2(PIN_BUTTON);
Light light_main = Light();
Light light_slave = Light();
volatile long light_increment = 0; 
volatile int lastEncoded = 0;
volatile int startButton = 0;

void serviceEncoderInterrupt() {
  int signalA = !digitalRead(PIN_SIGNAL_A);
  int signalB = !digitalRead(PIN_SIGNAL_B);

  int encoded = (signalB << 1) | signalA;  // converting the 2 pin value to single number
  int sum  = (lastEncoded << 2) | encoded; // adding it to the previous encoded value

  if(sum == 0b0001 || sum == 0b0111 || sum == 0b1110 || sum == 0b1000) light_increment ++;
  if(sum == 0b0010 || sum == 0b1011 || sum == 0b1101 || sum == 0b0100) light_increment --;

  lastEncoded = encoded; // store this value for next time
}

void pressedShort(Button2& btn) {
String test_str;
#ifdef SERIAL_DEBUG
Serial.println("Light main: ");
Serial.print("Topic Set: ");
Serial.println(light_main.topicSet);
Serial.print("Topic State: ");
Serial.println(light_main.topicState);
Serial.print("State: ");
Serial.println(light_main.state);
Serial.print("Brightness: ");
Serial.println(light_main.brightness);
Serial.println("Light slave: ");
Serial.print("Topic Set: ");
Serial.println(light_slave.topicSet);
Serial.print("Topic State: ");
Serial.println(light_slave.topicState);
Serial.print("State: ");
Serial.println(light_slave.state);
Serial.print("Brightness: ");
Serial.println(light_slave.brightness);
#endif

if (light_main.topicSet.length() > 0){
  #ifdef LOG_TOPIC
  mqtt.publish(LOG_TOPIC, "Переключаем светильник 1");
  #endif
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
  }
  else{
  #ifdef LOG_TOPIC
  mqtt.publish(LOG_TOPIC, "Cветильник 1 не настроен");
  #endif
  }
  json_message.clear();
}

void pressedLong(Button2& btn) {
  String test_str;
  #ifdef SERIAL_DEBUG
Serial.println("Light main: ");
Serial.print("Topic Set: ");
Serial.println(light_main.topicSet);
Serial.print("Topic State: ");
Serial.println(light_main.topicState);
Serial.print("State: ");
Serial.println(light_main.state);
Serial.print("Brightness: ");
Serial.println(light_main.brightness);
Serial.println("Light slave: ");
Serial.print("Topic Set: ");
Serial.println(light_slave.topicSet);
Serial.print("Topic State: ");
Serial.println(light_slave.topicState);
Serial.print("State: ");
Serial.println(light_slave.state);
Serial.print("Brightness: ");
Serial.println(light_slave.brightness);
#endif
  if (light_slave.topicSet.length() > 0) {
  #ifdef LOG_TOPIC
  mqtt.publish(LOG_TOPIC, "Переключаем светильник 2");
  #endif
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

void soft_Reset(){
asm volatile ("jmp 0x0000");
}

void mqtt_setup_connect(){
  mqtt.connect("testSensor2");
  delay(500);
  if (mqtt.connected()) {
  mqtt.subscribe(SETUP_TOPIC0);
  mqtt.subscribe(SETUP_TOPIC1);
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

if (String(topic) == SETUP_TOPIC0) { // Обработка топика SETUP
auto u_id = json_message["light_id"].as<long>();
auto set = json_message["set_topic"].as<const char*>();
auto state = json_message["state_topic"].as<const char*>();
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, "id main ОК");
#endif

light_main.Setup(u_id, String(state), String(set));
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, "change state light1:");
char errBuf1[light_main.topicSet.length() + 1];
light_main.topicSet.toCharArray(errBuf1,light_main.topicSet.length() + 1); 
mqtt.publish(LOG_TOPIC, errBuf1);
#endif
mqtt.subscribe(state);
}

if (String(topic) == SETUP_TOPIC1) { // Обработка топика SETUP
auto u_id = json_message["light_id"].as<long>();
auto set = json_message["set_topic"].as<const char*>();
auto state = json_message["state_topic"].as<const char*>();
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, "id slave ОК");
#endif
light_slave.Setup(u_id, String(state), String(set));
#ifdef LOG_TOPIC
mqtt.publish(LOG_TOPIC, "change state light2:");
char errBuf2[light_slave.topicSet.length() + 1];
light_slave.topicSet.toCharArray(errBuf2,light_slave.topicSet.length() + 1); 
mqtt.publish(LOG_TOPIC, errBuf2);
#endif
mqtt.subscribe(state);
}

if (String(topic) == light_main.topicState) { //обработка сообщения состояния для второго светильника
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

void setup() {
  //initialize the backlight diods
  FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);
  #ifdef SERIAL_DEBUG
  Serial.begin (SERIAL_DEBUG);
  #endif
  // setup encoder pins
  pinMode(PIN_SIGNAL_A, INPUT); 
  pinMode(PIN_SIGNAL_B, INPUT);
  pinMode(PIN_BUTTON, INPUT);
  pinMode(ETH_RESET_PIN, OUTPUT);
  digitalWrite(ETH_RESET_PIN, LOW);
  //setup button
  #ifdef COMMON_GROUND
  digitalWrite(PIN_BUTTON, HIGH);
  #endif
  //buttonA.setPressedHandler(pressed);
  //buttonA.setReleasedHandler(released);
  buttonA.setDebounceTime(50);
  buttonA.setLongClickTime(400);
  buttonA.setClickHandler(pressedShort);
  buttonA.setLongClickHandler(pressedLong);
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
  delay(1000);
  mqtt.setServer(mqtt_IP, 1883);
  mqtt.setCallback(on_message);
  mqtt_setup_connect();
  Udp.begin(BRIGHTNESS_PORT);
}

void loop() {
if (!mqtt.connected()){
  mqtt_setup_connect();
}
buttonA.loop();
mqtt.loop();
}