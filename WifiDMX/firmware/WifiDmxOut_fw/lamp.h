// DMX512 "lamp"

#pragma once
#include "fraise.h"
#include <stdlib.h>

#define MAX_LAMPS 200
#define MAX_GROUPS 100
#define MAX_PRESETS 20

#ifndef CLIP
#define CLIP(x, min, max) (MIN(MAX(x, min), max))
#endif

#define printf fraise_printf

#if 0
class Lamp {
  private:
    static inline Lamp* lamps[MAX_LAMPS]{0};
    static inline int* groups[MAX_GROUPS]{0};
  protected:
    int id;
    int chan;
    Lamp(int _id, int _chan): id(_id), chan(_chan) { lamps[id] = this;}
    virtual void _compute(char *buf); // dest is the destination DMX buffer
    virtual void _command(const char *data, uint8_t len) {
        switch(data[0]) {
            case 200: // get chan
                printf("l lamp %d: chan %d\n", id, chan);
                break;
            default: ;
        }
    }
    inline void do_command(const char *data, uint8_t len) {
        _command(data, len);
        Lamp::_command(data, len);
    }
    static inline int period_ms = 10; // compute period
    static inline absolute_time_t timeout;
  public:
    static void compute(char *buf) {
        if(!time_reached(timeout)) return;
        timeout = make_timeout_time_ms(period_ms);
        for(int i = 0; i < MAX_LAMPS; i++) { 
            if(lamps[i]) lamps[i]->_compute(buf);
        }
    }
    static void command(const char *data, uint8_t len) {
        if(!len) return;
        unsigned int i = data[0];
        data++; len--;
        if(len && i < MAX_LAMPS && lamps[i]) {
            /*lamps[i]->_command(data, len);
            lamps[i]->Lamp::_command(data, len);*/
            lamps[i]->do_command(data, len);
        }
    }
    static void config(const char *data, uint8_t len) {
        if(!len) return;
        unsigned int com = data[0];
        data++; len--;
        switch(com) {
            case 0: // period_ms
                period_ms = data[0];
                break;
            case 1: // define group
                if(len > 0) {
                    int g = data[0];
                    data++; len--;
                    if(g >= MAX_GROUPS) break;
                    if(groups[g]) delete groups[g];
                    groups[g] = new int[len + 1];
                    int i = 0;
                    while(len--) {
                        groups[g][i] = data[i];
                        i++;
                    }
                    groups[g][i] = MAX_LAMPS; // MAX_LAMPS means 'end of group'
                }
                break;
            case 2: // group command
                if(len > 1) {
                    int g = data[0];
                    if(!groups[g]) { printf("e groups %d undefined!\n", g); return;}
                    data++; len--;
                    int i = 0;
                    int l = (unsigned int)groups[g][i];
                    while(l < MAX_LAMPS) {
                        if(lamps[l]) lamps[l]->do_command(data, len);
                        l = groups[g][++i];
                    }
                }
                break;
            default: ;
        }
        /*if(len && i < MAX_LAMPS && lamps[i]) {
            lamps[i]->do_command(data, len);
        }*/
    }
    static void set_period_ms(int ms) {period_ms = ms;}
};

class LampFade: public Lamp {
    float val = 0;
    unsigned char target = 0;
    float factor = 1.0; // ms
  public:
    LampFade(int id, int chan): Lamp(id, chan) {}
    void _compute(char *buf){
        val += ((float)target - val) * factor;
        buf[chan] = val;
    }
    void _command(const char *data, uint8_t len) {
        unsigned char command = data[0];
        unsigned int time_constant;
        switch(command) {
            case 0: // target
                target = data[1];
                break;
            case 1 : // time constant ms
                time_constant = data[1] * 256 + data[2];
                if(time_constant < 1) time_constant = 1;
                factor = ((float)period_ms) / time_constant;
                if(factor > 1.0) factor = 1.0;
                printf("factor: %f\n", factor);
                break;
            default: ;
        }
    }
};
#endif

inline int rnd(int scale) {
    return (int)((unsigned)random() % (MAX((unsigned)scale, 1)));
}

inline void filter(int &val, int input, int speed, int speed_scale) {
    val += ((input - val) * speed) / speed_scale;
}

inline void ffilter(float &val, int input, int speed, int speed_scale) {
    val += ((input - val) * speed) / (float)speed_scale;
}

struct Color { uint8_t r, g, b, w; };

struct LampSetting { //24 bytes
    uint8_t intensMax;
    uint8_t intensMin;
    uint8_t colorMaxSpeedUp;
    uint8_t colorMaxSpeedDown;
    uint8_t colorMaxCountLow;
    uint8_t colorMaxCountHigh;
    uint8_t colorRandom;
    uint8_t colorFiltFactor;
    uint8_t intensSpeed;
    Color colorLow;
    Color colorHigh;
    uint8_t crepitOne;
    uint8_t crepitAll;
    uint8_t _padding_[5];
};

struct [[gnu::packed]] LampDefinition { // 2 bytes
    unsigned channel:9;
    unsigned ncolors:7;
};

class Lamp {
  public:
    static inline int period_ms = 10; // compute period

  private:
    unsigned char ncolors = 0;
    int channel = 0;

    int colorSpeedUp = 60, colorSpeedDown = 130;
    int colorCountLow = 1, colorCountHigh = 1;
    int colorCount = 0;
    int colorFilt = 0, color = 0; // 0 - 65535
    int colorTarget = 30000;
    int intens = 0, intensFilt = 0; // 0 - 4095
    int flash_count;
    int flash_duration_ms = 700;
    enum {none, armed, done} flash_state = none;
    enum {low, up, high, down} col_state = up;
    Color lastColor{0};
    bool debugRecalc = false;
    int _lastIntensMax = 0;

    void initParams() {
        colorSpeedUp = 60;
        colorSpeedDown = 130;
        colorCountLow = 1;
        colorCountHigh = 1;
        colorCount = 0;
        colorFilt = 0;
        color = 0;
        colorTarget = 30000;
        intens = 0;
        intensFilt = 0;
    }

    inline void setbuf(char* buf, unsigned char r, unsigned char g, unsigned char b, unsigned char w) {
        buf[channel + 0] = r;
        if(ncolors > 1) {
            buf[channel + 1] = g;
            if(ncolors > 2) {
                buf[channel + 2] = b;
                if(ncolors > 3) buf[channel + 3] = w;
            }
        }
    }

public:
    static void set_period_ms(int ms) {
        period_ms = ms;
    }

    void recalcParams(LampSetting &setting) {
        #define rndmix(val, percent) (rnd((int)(val) * (int)(percent)) + (int)(val) * (100 - (int)(percent)))

        colorSpeedUp = rndmix(setting.colorMaxSpeedUp * (int)setting.colorMaxSpeedUp, setting.colorRandom) / 255; // colorSpeedUp ~= 100*colorMaxSpeedUp
        if(colorSpeedUp < 20) colorSpeedUp = 20;

        colorSpeedDown = rndmix(setting.colorMaxSpeedDown * (int)setting.colorMaxSpeedDown, setting.colorRandom) / 255;
        if(colorSpeedDown < 20) colorSpeedDown = 20;

        colorTarget = rndmix(65536, setting.colorRandom) / 100;
        colorCountLow = rndmix(setting.colorMaxCountLow, setting.colorRandom) / 100;
        colorCountHigh = rndmix(setting.colorMaxCountHigh, setting.colorRandom) / 100;
        if(debugRecalc) {
            fraise_printf("l lamp_recalc %d %d %d %d %d\n", colorSpeedUp, colorSpeedDown, colorTarget, colorCountLow, colorCountHigh);
            fraise_printf("l lamp_recalc_speedup %d %d %d %d %d\n", setting.colorMaxSpeedUp, setting.colorRandom,
                (int)setting.colorMaxSpeedUp * (int)setting.colorRandom, rnd((int)setting.colorMaxSpeedUp * (100 - (int)setting.colorRandom)),
                rnd(((int)setting.colorMaxSpeedUp) * ((int)100 - ((int)setting.colorRandom))));
        }
    }

    void compute(char *buf, LampSetting &setting){
        if(flash_state != none) {
            if(flash_state == armed) {
                if(flash_count == 0) {
                    setbuf(buf, 255, 255, 255, 255);
                    flash_count = flash_duration_ms / period_ms;
                    flash_state = done;
                    return;
                }
                else flash_count--;
            }
            else if(flash_state == done) {
                float v = ((float)flash_count * period_ms) / flash_duration_ms;
                v = v * v * 255;
                v = CLIP(v, 0, 255);
                setbuf(buf, v, v, v, v);
                if(flash_count == 0) flash_state = none;
                else flash_count--;
                return;
            }
            setbuf(buf, 0, 0, 0, 0);
            intens = intensFilt = 0;
            return;
        }
        switch(col_state) {
            case up:
                color += colorSpeedUp;
                if(color > colorTarget) {
                    if(color > 65535) color = 65535;
                    recalcParams(setting);
                    col_state = high;
                    colorCount = 0;
                }
                break;
            case high:
                if(colorCount >= colorCountHigh) {
                    col_state = down;
                } else colorCount++;
                break;
            case down:
                color -= colorSpeedDown;
                if(color < 0) {
                    color = 0;
                    col_state = low;
                    colorCount = 0;
                }
                break;
            case low:
                if(colorCount >= colorCountLow) {
                    col_state = up;
                } else colorCount++;
                break;
            default: ;
        }
        colorFilt += ((color - colorFilt) * (int)setting.colorFiltFactor) / 100;

        int intensMax = 16 * (int)setting.intensMax;
        _lastIntensMax = intensMax;
        int intensMin = 16 * (int)setting.intensMin;
        int intens_rnd = intensMax;
        if(intensMax > intensMin) intens_rnd = (1.5 * rnd(intensMax - intensMin)) + intensMin;
        int squIntensSpeed = setting.intensSpeed * (int)setting.intensSpeed;
        intens += ((intens_rnd - intens) * squIntensSpeed) / 65536;
        if(intens < 0) intens = 0;
        if(intens > 4095) intens = 4095;
        intensFilt += ((intens - intensFilt) * squIntensSpeed) / 65536;
        if(intensFilt < 0) intensFilt = 0;
        if(intensFilt > 4095) intensFilt = 4095;

        #define MIX(c) ((((65535 - colorFilt) * (int)setting.colorLow.c + colorFilt * (int)setting.colorHigh.c) / 256) * intensFilt) / (4096 * 256);
        #define MIXCLIP(c) do{lastColor.c = MIX(c); lastColor.c = CLIP(lastColor.c, 0, 255);} while(0)
        MIXCLIP(r);
        MIXCLIP(g);
        MIXCLIP(b);
        MIXCLIP(w);
        setbuf(buf, lastColor.r, lastColor.g, lastColor.b, lastColor.w);
    }

    void command(const char *data, uint8_t len) {
        unsigned char command = data[0];
        switch(command) {
            case 0: // stats
                fraise_printf("l lamp %d %d %d %d %d %d %d %d i %d %d %d c %d %d %d\n", 
                    color, colorSpeedUp, colorSpeedDown, colorCountLow, colorCountHigh, colorCount, colorFilt, colorTarget,
                    intens, intensFilt, _lastIntensMax,
                    lastColor.r, lastColor.g, lastColor.b);
                break;
            case 1: 
                debugRecalc = data[1] != 0;
                break;
            case 2: 
                initParams();
                break;
            default: ;
        }
    }

    void flash() {
        flash_state = armed;
        flash_count = 0;
        flash_duration_ms = 0;
    }

    void resetCount() {
        colorCount = 0;
        color = 0;
    }

    //LampFire(int id, int chan, unsigned char ncolors): Lamp(id, chan), ncolors(ncolors) { ncolors = CLIP(ncolors, 1, 4);}
    void define(LampDefinition &def) {
        channel = def.channel;
        ncolors = def.ncolors;
    }
};

struct GroupSetting {
    static inline const int lamps_per_group = 100;
    uint8_t lamps[lamps_per_group];
    LampSetting settings[16]{0};
};

class LampGroup {
    LampSetting startSetting{0};
    LampSetting currentSetting{0};
    LampSetting destinationSetting{0};
    int durationMs = 1000;
    absolute_time_t startTime = 0;
    int interpLastRatio;
  public:
    void startTransition(LampSetting &setting, int ms) {
        startTime = get_absolute_time();
        startSetting = currentSetting;
        destinationSetting = setting;
        durationMs = ms;
        /*fraise_printf("\nl group transition in %dms to", ms);
        for(int i = 0; i < sizeof(LampSetting); i++) {
            fraise_printf(" %d", ((uint8_t *)&setting)[i]);
        }
        fraise_printf("\n");*/
    }

    void resetCounts(GroupSetting &setting, Lamp (&lamps)[MAX_LAMPS]) {
        for(int lamp = 0; lamp < GroupSetting::lamps_per_group; lamp++) {
            if(setting.lamps[lamp] == 0) break;
            lamps[setting.lamps[lamp]].resetCount();
        }
    }

    void compute(char *buf, GroupSetting &setting, Lamp (&lamps)[MAX_LAMPS]) {
        int ratio = CLIP(absolute_time_diff_us(startTime, get_absolute_time()) / durationMs, 0, 1000);
        // interpolate setting
        uint8_t *current = (uint8_t*)&currentSetting;
        uint8_t *start = (uint8_t*)&startSetting;
        uint8_t *dest = (uint8_t*)&destinationSetting;
        for(int i = 0; i < sizeof(LampSetting); i++) {
            current[i] = (start[i] * (1000 - ratio) + dest[i] * ratio) / 1000;
        }
        if(abs(interpLastRatio - ratio) > 100) {
            interpLastRatio = ratio;
            for(int lamp = 0; lamp < GroupSetting::lamps_per_group; lamp++) {
                if(setting.lamps[lamp] == 0) break;
                lamps[setting.lamps[lamp]].recalcParams(currentSetting);
            }
        }
        currentSetting.colorFiltFactor = CLIP(currentSetting.colorFiltFactor, 1, 100);
        if(currentSetting.intensSpeed < 2) currentSetting.intensSpeed = 2;
        // crepit
        bool flashAll = false;
        int crepitAll = currentSetting.crepitAll;
        int crepitOne = currentSetting.crepitOne;
        if(crepitAll != 0) {
            if(rnd(65536) < (crepitAll * crepitAll) / 2) flashAll = true;
        }
        // for each lamp, compute with currentSetting
        for(int lamp = 0; lamp < GroupSetting::lamps_per_group; lamp++) {
            if(setting.lamps[lamp] == 0) break;
            if(flashAll) lamps[setting.lamps[lamp]].flash();
            else if(crepitOne != 0) {
                if(rnd(65536) < (crepitOne * crepitOne) / 2) lamps[setting.lamps[lamp]].flash();
            }
            lamps[setting.lamps[lamp]].compute(buf, currentSetting);
        }
    }

    void command(const char *data, uint8_t len) {
        unsigned char command = data[0];
        switch(command) {
            case 0: // intens
                destinationSetting.intensMax = data[1];
                if(len > 2) destinationSetting.intensMin = data[2];
                break;
            case 1: // config
                //if(len < 8) break;
                destinationSetting.colorMaxSpeedUp     = data[1];
                destinationSetting.colorMaxSpeedDown   = data[2];
                destinationSetting.colorMaxCountLow    = data[3];
                destinationSetting.colorMaxCountHigh   = data[4];
                destinationSetting.colorRandom         = data[5];
                destinationSetting.colorFiltFactor     = 100 - data[6];
                destinationSetting.colorFiltFactor = CLIP(destinationSetting.colorFiltFactor, 1, 100);
                destinationSetting.intensSpeed         = data[7];
                if(destinationSetting.intensSpeed < 2) destinationSetting.intensSpeed = 2;
                break;
            case 2: // colors
                if(len < 9) break;
                destinationSetting.colorLow.r  = data[1];
                destinationSetting.colorLow.g  = data[2];
                destinationSetting.colorLow.b  = data[3];
                destinationSetting.colorLow.w  = data[4];
                destinationSetting.colorHigh.r = data[5];
                destinationSetting.colorHigh.g = data[6];
                destinationSetting.colorHigh.b = data[7];
                destinationSetting.colorHigh.w = data[8];
                break;
            case 10: // flash
                destinationSetting.crepitOne = data[1];
                destinationSetting.crepitAll = data[2];
                break;
            default: ;
        }
    }
};

struct PresetSetting {
    uint16_t ms;
    uint8_t setting[MAX_GROUPS];
};

struct LampGroupsSetting {
    LampDefinition lamps[MAX_LAMPS]{0};
    GroupSetting groups[MAX_GROUPS];
    PresetSetting presets[MAX_PRESETS];
};

class LampGroups {
    LampGroupsSetting *settings;
    Lamp lamps[MAX_LAMPS];
    LampGroup groups[MAX_GROUPS];
    absolute_time_t timeout;
  public:
    void update_settings() {
        for(int i = 0; i < MAX_LAMPS; i++) {
            lamps[i].define(settings->lamps[i]);
        }
    }

    void config(char *settingsAddress) {
        settings = (LampGroupsSetting *)settingsAddress;
        update_settings();
    }

    void compute(char *buf) {
        if(!time_reached(timeout)) return;
        timeout = make_timeout_time_ms(Lamp::period_ms);
        for(int group = 0; group < MAX_GROUPS; group++) { 
            if(settings->groups[group].lamps[0] == 0) continue;
            groups[group].compute(buf, settings->groups[group], lamps);
        }
    }

    void startTransition(int group, int setting, int ms) {
        if(group < 0 || group >= MAX_GROUPS) return;
        if(setting < 0 || setting > 15) return;
        groups[group].startTransition(settings->groups[group].settings[setting], ms);
        //fraise_printf("\nl group %d transition to %d in %d ms\n", group, setting, ms);
    }

    void resetCounts(int group) {
        if(group < 0 || group >= MAX_GROUPS) return;
        groups[group].resetCounts(settings->groups[group], lamps);
    }

    void callPreset(int preset) {
        if(preset < 0 || preset >= MAX_PRESETS) return;
        int ms = settings->presets[preset].ms;
        for(int group = 0; group < MAX_GROUPS; group++) {
            int setting = settings->presets[preset].setting[group];
            if(setting < 0 || setting > 15) continue;
            //fraise_printf("l preset %d group %d setting %d\n", preset, group, setting);
            groups[group].startTransition(settings->groups[group].settings[setting], ms);
        }
    }

    void command(const char *data, uint8_t len) {
        unsigned char command = data[0];
        data++; len--;
        switch(command) {
            case 0: // lamp command
                {
                    unsigned char lamp = data[0];
                    data++; len--;
                    if(lamp < MAX_LAMPS)
                        lamps[lamp].command(data, len);
                }
                break;
            case 1: // group
                {
                    unsigned char group = data[0];
                    data++; len--;
                    if(group < MAX_GROUPS) groups[group].command(data, len);
                }
                break;
            case 10: // transition
                {
                    int group = data[0];
                    int setting = data[1];
                    int ms = (int)data[2] * 256 + data[3];
                    update_settings(); // ??
                    startTransition(group, setting, ms);
                }
                break;
            case 11: // reset counts
                {
                    int group = data[0];
                    resetCounts(group);
                }
                break;
            case 20: // preset
                callPreset(data[0]);
                break;
            case 100: // inspect lamp def
                {
                    unsigned char lamp = data[0];
                    if(lamp < MAX_LAMPS)
                        fraise_printf("l lamp %d %d %d\n", lamp, settings->lamps[lamp].channel, settings->lamps[lamp].ncolors);
                }
                break;
            case 101: // inspect group def
                {
                    unsigned char group = data[0];
                    unsigned char setting = data[1];
                    if(group < MAX_GROUPS) {
                        fraise_printf("l group %d", group);
                        for(int i = 0 ; i < 100; i++) {
                            int l = settings->groups[group].lamps[i];
                            if(l == 0) break;
                            fraise_printf(" %d", l);
                        }
                        fraise_printf("\nl group %d setting %d", group, setting);
                        for(int i = 0; i < sizeof(LampSetting); i++) {
                            fraise_printf(" %d", ((uint8_t *)&settings->groups[group].settings[setting])[i]);
                        }
                        fraise_printf("\n");
                    }
                }
                break;
        }
    }
};

static_assert(sizeof(LampDefinition) == 2);
static_assert(sizeof(LampSetting) == 24);
static_assert(sizeof(GroupSetting) == 484);
static_assert(sizeof(PresetSetting) == 102);
static_assert(sizeof(LampGroupsSetting) == 50840);

