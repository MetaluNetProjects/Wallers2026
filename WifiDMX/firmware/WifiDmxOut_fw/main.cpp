// Palantir main

#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "udp_server.hpp"
#include "fraise.h"
#include "cpuload.h"
#include "free_heap.h"
#include "system.h"
#include "math.h"
#include "partition_info.h"
#include "wifi_partition.hpp"
#include "pico/bootrom.h"
#include "boot/picobin.h"

#include "DmxOutput.h"
#include "pico_ws_server/web_socket_server.h"
#include "data_partition.hpp"
#include "lamp.h"
#include "button.h"

#include <sstream>

// pins definition
#define PIN_BUTTON      0
#define PIN_DMX_RX      20
#define PIN_DMX_TX      21
#define PIN_DMX_DRV     22

DmxOutput port;
uint8_t channels[513]{0};
int max_channel = 200;
bool dmxInited = false;
bool is_connected = false;
bool lampGroupsEnable = true;
//WebSocketServer wsserver(10);
WebSocketServer *wsserver = nullptr;
DataPartition html;
DataPartition tables;
LampGroups lampGroups;
Button button(PIN_BUTTON, lampGroups);

void blink(int ledPeriod) {
    static bool led = false;
    static absolute_time_t nextLed_time;

    if(time_reached(nextLed_time)) {
        set_led(led = !led);
        nextLed_time = make_timeout_time_ms(ledPeriod);
    }
}

void initDmx() {
    if(!dmxInited) port.begin(PIN_DMX_TX);
    dmxInited = true;
}

void on_connect(WebSocketServer& server, uint32_t conn_id) {
  fraise_printf("l WebSocket opened %d\n", conn_id);
  server.sendMessage(conn_id, "hello web socket!");
  is_connected = true;
}

void on_disconnect(WebSocketServer& server, uint32_t conn_id) {
  fraise_printf("l WebSocket closed %d\n", conn_id);
  is_connected = false;
}

void on_message(WebSocketServer& server, uint32_t conn_id, const void* data, size_t len) {
    fraise_printf("l WebSocket message %d\n%s\n", (int)len, (char*)data);
    //fraise_printf("%s\n", (int)len, (char*)data);
    //char name[128];
    /*int val;
    int ret = sscanf((const char*)data, "%127s %d", name, &val);
    if(ret == 2 && !strcmp(name, "volume")) {
        char buf[128];
        sprintf(buf, "volume=%d", val);
        server.broadcastMessage(buf);
        //count_ms = MAX(val, 2);
    }*/
    /*std::string str((char*)data);
    std::stringstream ss(str);

    std::string word;

    if(! ss >> word) return;
    if(word == "group") {
        if(! ss >> word) return;
        try {
            int group = std::stoi(word);*/
    char *indata = (char*)data;
    //indata[len] = 0; // terminate string
    //trace("RH::onWebSocketData: this=%p len=%d data=[%.*s]", this, len, len, data);
    switch(indata[0]) {
        case 'f': 
            {
                indata += 2; // remove "f "
                char *token;
                char buf[64];
                int buflen = 0;
                while(indata && (token = strsep(&indata, " "))) {
                    buf[buflen++] = atoi(token);
                }
                fraise_init_get_buffer(buf, len);
                fraise_receivebytes(buf, buflen);
            }
            break;
    }
}

void run_server() {
    if(wsserver) delete wsserver;
    wsserver = new WebSocketServer(10);

    wsserver->setConnectCallback(on_connect);
    wsserver->setCloseCallback(on_disconnect);
    wsserver->setMessageCallback(on_message);

    bool server_ok = wsserver->startListening(80);
    if (!server_ok) {
        fraise_printf("l Failed to start WebSocket server\n");
        while (1) tight_loop_contents();
    }
    fraise_printf("l WebSocket server started\n");
}

void setup() {
    //ip_addr_t ip;
    //IP4_ADDR(&ip, 192, 168, 5, WIFI_IPADDR_D);
    //netif_set_ipaddr(netif_default, &ip);
    gpio_init(PIN_DMX_DRV);
    gpio_set_dir(PIN_DMX_DRV, GPIO_OUT);
    gpio_put(PIN_DMX_DRV, 1);
    initDmx();
    html.setup("HTML");
    tables.setup("Tables");
    lampGroups.config(tables.get_address());
    Lamp::set_period_ms((max_channel * 11) / 250);
    button.setup();
    run_server();
}

void loop() {
    if(dmxInited && !port.busy()) {
        if(lampGroupsEnable) lampGroups.compute((char*)channels);
        port.write(channels, max_channel);
        button.compute();
    }
    if(wsserver) wsserver->popMessages();
    //blink(250);
}

void set_channel(int channel, uint8_t value) {
    if(channel > 0 && channel < max_channel) channels[channel] = value;
}

void fraise_receivebytes(const char* data, uint8_t len) {
    char command = fraise_get_uint8();
    data++; len--;
    switch(command) {
    case 10: {
            int chan = fraise_get_int16();
            len -= 2;
            data += 2;
            while(len-- && chan < max_channel) {
                channels[chan++] = *data++;
            }
        }
        break;
    //case 100: Lamp::command(data, len); break;
    //case 101: Lamp::config(data, len); break;
    case 90:
        lampGroupsEnable = (data[0] != 0);
        break;
    case 100: 
        lampGroups.command(data, len);
        break;
    case 180:
        html.fraise_receivebytes(data, len);
        break;
    case 181:
        tables.fraise_receivebytes(data, len);
        break;
    case 200: run_server(); break;
    case 240: fraise_printf("l free ram: %ld\n", getFreeHeap()); break;
    case 241:
        wifi_print_status();
        {
            const ip4_addr_t *a = netif_ip4_addr(netif_default);
            fraise_printf("l IP %s\n", ip4addr_ntoa(a));
        }
        break;
    case 242:
        print_partitions();
        break;
    case 243:
        {
            intptr_t start;
            int length;
            if (get_partition_address("Tables", &start, &length)) {
                fraise_printf("l Tables partition found 0x%08x %d\n", start, length);
            } else fraise_printf("l Tables partition not found!\n");
        }
        break;
    case 244:
        {
            const char *ssid = WifiPartition::get_ssid();
            const char *pw = WifiPartition::get_password();
            const char *apssid = WifiPartition::get_apssid();
            const char *appw = WifiPartition::get_appassword();
            if(ssid && pw && apssid && appw) fraise_printf("l wifi current config %s %s %s %s\n", ssid, pw, apssid, appw);
            else fraise_printf("l wifi config not initialized\n");
        }
        break;
    }
}

extern void print_version(); // from version.cpp

void fraise_receivechars(const char *data, uint8_t len){
    if(data[0] == 'E') { // Echo
        fraise_printf("E%s\n", data + 1);
    } else if(data[0] == 'V') { // Version
        print_version();
    } else if(data[0] == 'W') { // wifi config
        char ssid[64]{0};
        char pw[64]{0};
        int nb = sscanf(data, "WIFI_CONFIG %64s %64s", ssid, pw);
        if(nb == 2 && strlen(ssid) && strlen(pw)) {
            printf("l wifi saving config %s %s: ", ssid, pw);
            if(WifiPartition::set(ssid, pw)) printf("SUCCESS\n");
            else printf("FAILURE\n");
        }
        nb = sscanf(data, "WIFI_APCONFIG %64s %64s", ssid, pw);
        if(nb == 2 && strlen(ssid) && strlen(pw)) {
            printf("l wifi saving apconfig %s %s: ", ssid, pw);
            if(WifiPartition::setap(ssid, pw)) printf("SUCCESS\n");
            else printf("FAILURE\n");
        }
    } else fraise_printf("unknown %d\n", data[0]);
}
char *get_html_address() {return html.get_address();}
int get_html_length() {return html.get_length();}
