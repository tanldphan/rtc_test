// C standard library
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// ESP-IDF included SDK
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "portmacro.h"
#include "driver/i2c.h"
#include "esp_efuse.h"

#include "esp_sleep.h"
#include "esp_system.h"

#include "ds3231.h"
#include "i2cdev.h"

#define RTC_I2C I2C_NUM_0 // assigning ESP's I2C controller 0
#define RTC_SDA (36)
#define RTC_SCL (37)
#define RTC_SQW (10)

void rtc_init(void);
bool rtc_get_time(struct tm *time);
bool rtc_set_time(const struct tm *time);
bool rtc_set_alarm(const struct tm *alarm_time);

struct tm next_alarm = {
    .tm_sec = 30,
    .tm_min = 0,
    .tm_hour = 12,
    .tm_mday = 1,
    .tm_mon = 6,
    .tm_year = 125,
    .tm_wday = 1,
    .tm_yday = 194,
    .tm_isdst = -1
};

void app_main(void)
{
    i2cdev_init();
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI("WAKEUP", "Wakeup cause: %d", cause);
    if (cause == ESP_SLEEP_WAKEUP_EXT0)
    {
    ESP_LOGI("WAKEUP", "Woke up from EXT0 (RTC Alarm)");
    }  
    else
    {
    ESP_LOGI("WAKEUP", "Cold boot (not deep sleep wake)");
    }

    i2c_dev_t dev = { 0 };

    ESP_ERROR_CHECK(ds3231_init_desc(&dev, RTC_I2C, RTC_SDA, RTC_SCL));
    vTaskDelay(pdMS_TO_TICKS(5000));
    ds3231_clear_alarm_flags(&dev, DS3231_ALARM_1);
    struct tm fake_time = {
        .tm_year = 2025 - 1900,
        .tm_mon  = 6,   // July
        .tm_mday = 14,
        .tm_hour = 15,
        .tm_min  = 0,
        .tm_sec  = 0
    };
// Only set fake time if cold boot
    if (cause == ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        ds3231_set_time(&dev, &fake_time);
    }

    struct tm alarm_time = fake_time;
    alarm_time.tm_sec += 30;
    mktime(&alarm_time); // Normalize
    alarm_time.tm_wday = -1; // disable weekday matching

    ds3231_clear_alarm_flags(&dev, DS3231_ALARM_1);
    ds3231_set_alarm(&dev, DS3231_ALARM_1, &alarm_time, DS3231_ALARM1_MATCH_SECMINHOUR, NULL, 0);
    ds3231_enable_alarm_ints(&dev, DS3231_ALARM_1);


    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << RTC_SQW,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

        gpio_set_direction(GPIO_NUM_36, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_36, GPIO_PULLUP_ONLY);
        gpio_set_direction(GPIO_NUM_37, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_37, GPIO_PULLUP_ONLY);
      // EXT0 wakeup (low level trigger)
    esp_sleep_enable_ext0_wakeup(RTC_SQW, 0);  // Wake when INT goes LOW

    ESP_LOGI("DEEPSLEEP", "Going to sleep, will wake up on RTC alarm.");
    i2cdev_done();  // Frees any semaphores still hanging around
    vTaskDelay(pdMS_TO_TICKS(20000));
    esp_deep_sleep_start();
}