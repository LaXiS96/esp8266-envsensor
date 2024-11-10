#include "wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include <arpa/inet.h>
#include <string.h>

#define WIFI_READY_EVENT_BIT BIT0

static const char *TAG = "wifi";
static EventGroupHandle_t wifi_event_group;

static void wifi_init_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGW(TAG, "connecting");
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(wifi_event_group, WIFI_READY_EVENT_BIT);
    }
}

static void wifi_disconnect_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGW(TAG, "reconnecting");
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void parse_bssid(const char *in, uint8_t *mac)
{
    if (strlen(in) != 17)
    {
        ESP_LOGE(TAG, "invalid BSSID: %s", in);
        return;
    }

    char str[18] = "";
    strcpy(str, in);

    int i = 0;
    char *token = strtok(str, ":");
    while (token != NULL)
    {
        mac[i] = strtoul(token, NULL, 16);
        token = strtok(NULL, ":");
    }
}

void wifi_start()
{
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .channel = CONFIG_WIFI_CHANNEL,
        },
    };
    parse_bssid(CONFIG_WIFI_BSSID, wifi_config.sta.bssid);

    tcpip_adapter_ip_info_t ip_info = {};
    if (inet_aton(CONFIG_ESP_IP_ADDRESS, &ip_info.ip) == 0)
    {
        ESP_LOGE(TAG, "invalid IP address: %s", CONFIG_ESP_IP_ADDRESS);
        return; // TODO should return esp_err_t
    }
    if (inet_aton(CONFIG_ESP_IP_NETMASK, &ip_info.netmask) == 0)
    {
        ESP_LOGE(TAG, "invalid IP netmask: %s", CONFIG_ESP_IP_NETMASK);
        return; // TODO should return esp_err_t
    }
    // TODO gateway

    tcpip_adapter_init();
    ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_init_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_disconnect_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_init_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    wifi_event_group = xEventGroupCreate();
    xEventGroupWaitBits(wifi_event_group, WIFI_READY_EVENT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_init_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &wifi_init_event_handler));
    // wifi_disconnect_event_handler is not unregistered because it is used for reconnection

    vEventGroupDelete(wifi_event_group);
}

void wifi_stop()
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_disconnect_event_handler));
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
}
