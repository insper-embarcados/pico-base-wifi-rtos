/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "cyw43.h"

struct led_task_arg {
    int gpio;
    int delay;
};

void potenciometro_task(){
    printf("ADC Example, measuring GPIO27\n");

    adc_init();

    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(27);
    // Select ADC input 1 (GPIO27)
    adc_select_input(1);

    while (1) {
        // 12-bit conversion, assume max value == ADC_VREF == 3.3 V
        const float conversion_factor = 3.3f / (1 << 12);
        uint16_t result = adc_read();
        printf("Raw value: 0x%03x, voltage: %f V\n", result, result * conversion_factor);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void blink_wifi_task() {
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
    }
    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(250));
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

int main() {
    stdio_init_all();
    printf("Start LED blink\n");

    xTaskCreate(potenciometro_task, "Pot_Task", 256, NULL, 1, NULL);
    
    xTaskCreate(blink_wifi_task, "Blink_Task", 1024, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
