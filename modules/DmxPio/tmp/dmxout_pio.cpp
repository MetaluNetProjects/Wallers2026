#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "dmxout_pio.hpp"

Dmxout_pio::Dmxout_pio(int p, int max): pin(p), max_channel(max) {
}

void Dmxout_pio::setup() {
    port.begin(pin);
}

void Dmxout_pio::set(int channel, unsigned char value) {
    if(channel > 0 && channel < 513) channels[channel] = value;
}

void Dmxout_pio::service() {
    if(port.busy()) return;
    port.write(channels, max_channel);
}

