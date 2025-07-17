// C standard library
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

// ESP-IDF included SDK
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_system.h"

#include "ds3231.h"
#include "i2cdev.h"

#define RTC_I2C I2C_NUM_0 // assigning ESP's I2C controller 0
#define RTC_SDA (10)
#define RTC_SCL (11)
#define RTC_SQW (21)

#define WAKE "WAKE UP"

// Not being run at boot >> means wake up from deep sleep
struct tm fake_time = {
    .tm_year = 2025 - 1900,
    .tm_mon  = 6,   // July
    .tm_mday = 14,
    .tm_hour = 15,
    .tm_min  = 0,
    .tm_sec  = 0
};

void app_main(void) // >>>>> Starts here after wake from deep sleep
{
    i2cdev_init();

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(WAKE, "Wakeup cause: %d (%s)", cause,
    (cause == ESP_SLEEP_WAKEUP_UNDEFINED) ? "Cold boot" :
    (cause == ESP_SLEEP_WAKEUP_EXT0) ? "EXT0 RTC Alarm" :
    "Other");

    i2c_dev_t dev = { 0 };

    ESP_ERROR_CHECK(ds3231_init_desc(&dev, RTC_I2C, RTC_SDA, RTC_SCL));
    printf("DS3231 descriptor initialized\n"); fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    if (ds3231_clear_alarm_flags(&dev, DS3231_ALARM_1) == ESP_OK)
    {
        printf("Alarm cleared!\n"); fflush(stdout);
    }

    if (cause == ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        ds3231_set_time(&dev, &fake_time);
        printf("Cold boot: Fake time set!\n"); fflush(stdout);
    }

    struct tm alarm_time;
    if (ds3231_get_time(&dev, &alarm_time) == ESP_OK)
    {
        printf("Pasted time!\n"); fflush(stdout);
    }
    
    alarm_time.tm_sec += 60;
    mktime(&alarm_time);

    ds3231_set_alarm(&dev, DS3231_ALARM_1, &alarm_time, DS3231_ALARM1_MATCH_SECMIN, NULL, 0);
    printf("Alarm set for: %02d:%02d:%02d\n", alarm_time.tm_hour, alarm_time.tm_min, alarm_time.tm_sec);

    ds3231_enable_alarm_ints(&dev, DS3231_ALARM_1);

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << RTC_SQW,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&io_conf);

    esp_sleep_enable_ext0_wakeup(RTC_SQW, 0);

    i2cdev_done();  // Frees any semaphores still hanging around
    ESP_LOGI("ABOUT TO SLEEP", "5s to deep sleep");
    vTaskDelay(pdMS_TO_TICKS(1000 * 5));

    esp_deep_sleep_start();
}