// button management

#pragma once
#include "fraise.h"
#include "lamp.h"

class Button {
private:
    int pin;
    int count;
    const int countThres = 10;
    LampGroups &lampGroups;
public:
    Button(int p, LampGroups &l) : pin(p), lampGroups(l) {}

    void setup() {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        setButton(false);
    }

    void compute() {
        if(gpio_get(pin) == 0) {
            if(count < countThres) {
                count++;
                if(count == countThres) setButton(true);
            }
        } else {
            if(count > 0) {
                count--;
                if(count == 0) setButton(false);
            }
        }
    }

    void setButton(bool button) {
        /*if(button) {
            lampGroups.startTransition(0, 0, 5000);
        } else {
            lampGroups.startTransition(0, 4, 5000);
        }*/
        lampGroups.callPreset(button * 1);
    }
};



