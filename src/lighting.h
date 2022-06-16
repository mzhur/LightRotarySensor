#ifndef String
#include <Arduino.h>
#endif
#ifndef Lighting
#define Lighting

class Light {
  public:
    unsigned long unic_id;
    String topicState;
    String topicSet;
    boolean state;
    long brightness;
    unsigned int port;
    Light();
    void Switch();
    void Setup(unsigned long u_id, String state, String set, unsigned int port_income);
};
#endif