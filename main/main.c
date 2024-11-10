#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "driver/adc.h"
#include "driver/i2c.h"

#include <sys/socket.h>
#include <sys/time.h>

#include "bme280.h"
#include "wifi.h"

#define _STRING(x) #x
#define STRING(x) _STRING(x)

static const char *TAG = "main";

static const float adc_ratio = 56.0 / (220.0 + 56.0) * 1000.0;

static void i2c_start()
{
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_4,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = GPIO_NUM_5,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .clk_stretch_tick = 300, // ?
    };
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, config.mode));
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &config));
}

static void tcp_send(const char *address, uint16_t port, const void *data, size_t len)
{
    struct sockaddr_in host = {};
    host.sin_family = AF_INET;
    inet_aton(address, &host.sin_addr);
    host.sin_port = htons(port);

    int s = socket(host.sin_family, SOCK_STREAM, 0);
    if (s < 0)
    {
        ESP_LOGE(TAG, "socket() failed: %d", errno);
        return;
    }

    if (connect(s, (struct sockaddr *)&host, sizeof(host)) < 0)
    {
        ESP_LOGE(TAG, "connect() failed: %d", errno);
        closesocket(s);
        return;
    }

    if (send(s, data, len, 0) < 0)
    {
        ESP_LOGE(TAG, "send() failed: %d", errno);
        closesocket(s);
        return;
    }

    // struct timeval recv_timeout = {
    //     .tv_sec = 5,
    //     .tv_usec = 0,
    // };
    // if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0)
    // {
    //     ESP_LOGE(TAG, "setsockopt() failed: %d", errno);
    //     closesocket(s);
    //     return;
    // }

    int r = 0;
    do
    {
        char resbuf[128] = {};
        r = recv(s, resbuf, sizeof(resbuf) - 1, 0);
        if (r > 0)
            ESP_LOGI(TAG, "received: %s", resbuf);
        else if (r < 0)
        {
            ESP_LOGE(TAG, "recv() failed: %d", errno);
            closesocket(s);
            return;
        }
    } while (r > 0);

    closesocket(s);
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init()); // TODO erase on errors?
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_start();
    i2c_start();
    bme280_init();

    // Note: ESP_LOG and printf variants do not format float values
    ESP_LOGI(TAG, "adc_ratio: %u", (uint32_t)(adc_ratio * 1000));

    while (true)
    {
        // Note: ADC readings seem to be off by +200mV
        adc_config_t adc_config = {.mode = ADC_READ_TOUT_MODE, .clk_div = 8};
        ESP_ERROR_CHECK(adc_init(&adc_config));
        uint16_t adc_value = 0;
        ESP_ERROR_CHECK(adc_read(&adc_value));
        // ESP_ERROR_CHECK(adc_read_fast(&adc_value, 1));
        uint32_t adc_volts = adc_value / adc_ratio * 1000;
        ESP_ERROR_CHECK(adc_deinit());

        ESP_LOGI(TAG, "adc: %u volts: %u", adc_value, adc_volts);

        int32_t temp;
        uint32_t press, hum;
        bme280_measure(&temp, &press, &hum);
        ESP_LOGI(TAG, "measured temp: %i press: %u hum: %u", temp, press, hum);

        char bodybuf[128] = "";
        int bodylen = snprintf(bodybuf, sizeof(bodybuf),
                               "test adc_value=%u,volts=%u,temp=%i.%u,press=%u.%u,hum=%u.%u",
                               adc_value, adc_volts,
                               (int32_t)(temp / 100.0), (uint32_t)(temp % 100),
                               (uint32_t)(press / 256.0), (uint32_t)(press % 256),
                               (uint32_t)(hum / 1024.0), (uint32_t)(hum % 1024));
        ESP_LOGI(TAG, "body: %s", bodybuf);

        // clang-format off
        char msgbuf[512] = "POST /api/v2/write?org=" CONFIG_INFLUXDB_ORG "&bucket=" CONFIG_INFLUXDB_BUCKET " HTTP/1.1\r\n"
                           "Host: " CONFIG_INFLUXDB_ADDRESS ":" STRING(CONFIG_INFLUXDB_PORT) "\r\n"
                           "Authorization: Token " CONFIG_INFLUXDB_TOKEN "\r\n"
                           "Connection: close\r\n"
                           "Content-Type: text/plain\r\n";
        // clang-format on
        int msglen = strlen(msgbuf);
        msglen += snprintf(msgbuf + msglen, sizeof(msgbuf) - msglen, "Content-Length: %u\r\n", bodylen);
        msglen += snprintf(msgbuf + msglen, sizeof(msgbuf) - msglen, "\r\n%s", bodybuf);

        tcp_send(CONFIG_INFLUXDB_ADDRESS, 8086, msgbuf, msglen);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

    wifi_stop();

    // TODO wake up periodically to obtain sensor data (with RF radio disabled)
    //      then every N wakeups enable radio and send that data to mqtt
    // TODO how to keep the time for intermediate sensor readings?
    //      it looks like maybe the system clock is updated on wakeup based on the slept time, check that first
    //      then maybe consider rtc_time_get and rtc_clk_to_us

    // esp_deep_sleep_set_rf_option(2); // Do not calibrate RF after wakeup
    // esp_deep_sleep(10 * 1000000);
}
