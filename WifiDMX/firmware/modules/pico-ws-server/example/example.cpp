#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "pico_ws_server/web_socket_server.h"

WebSocketServer wsserver(10);
bool is_connected = false;
int count_ms = 200;

void setled(bool on) {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

void on_connect(WebSocketServer& server, uint32_t conn_id) {
  printf("WebSocket opened %d\n", conn_id);
  server.sendMessage(conn_id, "hello web socket!");
  is_connected = true;
}

void on_disconnect(WebSocketServer& server, uint32_t conn_id) {
  printf("WebSocket closed %d\n", conn_id);
  is_connected = false;
}

void on_message(WebSocketServer& server, uint32_t conn_id, const void* data, size_t len) {
    //printf("WebSocket message %d\n%s\n", (int)len, (char*)data);
    //printf("%s\n", (int)len, (char*)data);
    char name[128];
    int val;
    int ret = sscanf((const char*)data, "%127s %d", name, &val);
    if(ret == 2) {
        printf("%s : %d (msg:%s)\n", name, val, data);
    } else {
        printf("bad conversion: ret=%d msg:%s\n", ret, data);
    }
    if(!strcmp(name, "volume")) {
        char buf[128];
        sprintf(buf, "volume=%d", val);
        wsserver.broadcastMessage(buf);
        count_ms = MAX(val, 2);
    }
}

int counter = 0;
absolute_time_t count_timeout;

void process_count() {
    char buf[256];
    if(! is_connected) return;

    if(! time_reached(count_timeout)) return;
    count_timeout = make_timeout_time_ms(count_ms);
    sprintf(buf, "counter=%d", counter);
    wsserver.broadcastMessage(buf);
    counter++;
    //setled(counter % 2);
}

int main() {
  stdio_init_all();
  sleep_ms(1000);

  if (cyw43_arch_init() != 0) {
    printf("cyw43_arch_init failed\n");
    while (1) tight_loop_contents();
  }

  cyw43_arch_enable_sta_mode();

  printf("Connecting to Wi-Fi...\n");
  int errcode = 0;
  while ((errcode = cyw43_arch_wifi_connect_timeout_ms("Domozic", "bidouchette", CYW43_AUTH_WPA2_AES_PSK, 30000)) != 0) {
    printf("failed to connect: %d\n", errcode);
    //while (1) tight_loop_contents();
  }
  printf("Connected.\n");
  setled(true);

  wsserver.setConnectCallback(on_connect);
  wsserver.setCloseCallback(on_disconnect);
  wsserver.setMessageCallback(on_message);

  bool server_ok = wsserver.startListening(80);
  if (!server_ok) {
    printf("Failed to start WebSocket server\n");
    while (1) tight_loop_contents();
  }
  printf("WebSocket server started\n");

  while (1) {
    cyw43_arch_poll();
    process_count();
    wsserver.popMessages();
  }
}
