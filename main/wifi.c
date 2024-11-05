#include "wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define WIFI_READY_EVENT_BIT BIT0

static const char *TAG = "wifi";
static EventGroupHandle_t wifi_event_group;

static void wifi_init_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        uint8_t mac[6];
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac));
        char hostname[33] = {0};
        snprintf(hostname, 32, "esp-rps-%.2x%.2x%.2x", mac[3], mac[4], mac[5]);
        ESP_LOGI(TAG, "hostname: %s", hostname);
        ESP_ERROR_CHECK_WITHOUT_ABORT(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname));

        ESP_LOGW(TAG, "WiFi connecting...");
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(wifi_event_group, WIFI_READY_EVENT_BIT);
    }
}

static void wifi_disconnect_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGW(TAG, "WiFi reconnecting...");
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void wifi_init()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_APP_WIFI_SSID,
            .password = CONFIG_APP_WIFI_PSK,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

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
    // wifi_disconnect_event_handler is not unregistered

    vEventGroupDelete(wifi_event_group);
}
