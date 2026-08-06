#include "EspNowClient.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <string.h>

static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    (void)mac_addr;
    Serial.printf("[ESPNOW] Send %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAILED");
}

void EspNowClient::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("[ESPNOW] Init failed");
        return;
    }

    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, ESPNOW_PEER_MAC, 6);
    peerInfo.channel = ESPNOW_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("[ESPNOW] Add peer failed");
    }
}

bool EspNowClient::sendReading(const espnow_sensor_msg_t &msg)
{
    esp_err_t result = esp_now_send(ESPNOW_PEER_MAC, (const uint8_t *)&msg, sizeof(msg));
    return result == ESP_OK;
}
