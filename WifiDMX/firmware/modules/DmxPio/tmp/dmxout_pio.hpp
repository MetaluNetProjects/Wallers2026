#pragma once
#include "DmxOutput.h"

class Dmxout_pio {
private:
    DmxOutput port;
    int max_channel;
    unsigned char channels[513]{0};
public:
    Dmxout_pio(int p, int max_channel);
    void setup();
    void set(int channel, unsigned char value);
    void service();
}
