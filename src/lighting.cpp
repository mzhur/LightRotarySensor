#include "lighting.h"
//#include <Arduino.h>

Light::Light() {
  state = 0;
  brightness = 0;
  topicState = "";
  topicSet = "";
  unic_id = 0;
  port = 0;
}

void Light::Setup(unsigned long u_id, String state, String set, unsigned int port_income) {
unic_id = u_id;
topicState = state;
topicSet = set;  
port = port_income;
}

void Light::Switch() {
state = !state;
}

