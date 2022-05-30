#include "lighting.h"
//#include <Arduino.h>

Light::Light() {
  state = 0;
  brightness = 0;
  topicState = "";
  topicSet = "";
  unic_id =0;
}

void Light::Setup(unsigned long u_id, String state, String set) {
unic_id = u_id;
topicState = state;
topicSet = set;  
}

void Light::Switch() {
state = !state;
}

