#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "ssd1306.h"

// === Definições para SSD1306 ===
ssd1306_t disp;

QueueHandle_t xQueueBtn;
QueueHandle_t xQueueTime;
SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueDistance;

const int PIN_ECHO = 10;
const int PIN_TRIGGER = 11;
const int BTN_PIN_R = 4;
const int BTN_PIN_G = 5;
const int BTN_PIN_B = 6;
const int LED_PIN_R = 7;
const int LED_PIN_G = 8;
const int LED_PIN_B = 9;

// == funcoes de inicializacao ===
void btn_callback(uint gpio, uint32_t events) {
    if (events & GPIO_IRQ_EDGE_FALL) xQueueSendFromISR(xQueueBtn, &gpio, 0);
}

void oled_display_init(void) {
    i2c_init(i2c1, 400000);
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);

    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);
    ssd1306_show(&disp);
}

void btns_init(void) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);

    gpio_init(BTN_PIN_G);
    gpio_set_dir(BTN_PIN_G, GPIO_IN);
    gpio_pull_up(BTN_PIN_G);

    gpio_init(BTN_PIN_B);
    gpio_set_dir(BTN_PIN_B, GPIO_IN);
    gpio_pull_up(BTN_PIN_B);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true, &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_G, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_PIN_B, GPIO_IRQ_EDGE_FALL, true);
}

void led_rgb_init(void) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 1);

    gpio_init(LED_PIN_G);
    gpio_set_dir(LED_PIN_G, GPIO_OUT);
    gpio_put(LED_PIN_G, 1);

    gpio_init(LED_PIN_B);
    gpio_set_dir(LED_PIN_B, GPIO_OUT);
    gpio_put(LED_PIN_B, 1);
}

void task_1(void *p) {
    uint btn_data;

    while (1) {
        if (xQueueReceive(xQueueBtn, &btn_data, pdMS_TO_TICKS(2000))) {
            printf("btn: %d \n", btn_data);

            switch (btn_data) {
                case BTN_PIN_B:
                    gpio_put(LED_PIN_B, 0);
                    ssd1306_draw_string(&disp, 8, 0, 2, "BLUE");
                    ssd1306_show(&disp);
                    break;
                case BTN_PIN_G:
                    gpio_put(LED_PIN_G, 0);
                    ssd1306_draw_string(&disp, 8, 24, 2, "GREEN");
                    ssd1306_show(&disp);
                    break;
                case BTN_PIN_R:
                    gpio_put(LED_PIN_R, 0);

                    ssd1306_draw_string(&disp, 8, 48, 2, "RED");
                    ssd1306_show(&disp);
                    break;
                default:
                    // Handle other buttons if needed
                    break;
            }
        } else {
            ssd1306_clear(&disp);
            ssd1306_show(&disp);
            gpio_put(LED_PIN_R, 1);
            gpio_put(LED_PIN_G, 1);
            gpio_put(LED_PIN_B, 1);
        }
    }
}

void pin_callback(uint gpio, uint32_t events){
    if(gpio == PIN_ECHO){
        uint64_t time_us = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &time_us, 0);
    }
}

void double_pra_str(double num, char *str){
    int inteiro = (int)num;
    int decimal = (int)((num - inteiro) * 100 + 0.5); //arredonda

    if(decimal >= 100){
        decimal = 0;
        inteiro++; 
    }

    str[0] = 'D';
    str[1] = 'i';
    str[2] = 's';
    str[3] = 't';
    str[4] = ':';
    str[5] = ' ';
    int i = 6;
   
    //inteiro
    if(inteiro >= 100){ //se tiver 3 algarismos
        str[i++] = '0' + (inteiro / 100); //cent
        str[i++] = '0' + ((inteiro / 10) % 10); //dezena
        str[i++] = '0' + (inteiro % 10); //unid
    } else if(inteiro >= 10){ //se tiver 2 algarismos
        str[i++] = '0' + (inteiro / 10);
        str[i++] = '0' + (inteiro % 10);
    } else { //se tiver só 1 algarismo 
        str[i++] = '0' + inteiro;
    }
    
    str[i++] = '.';
    
    //decimal
    str[i++] = '0' + (decimal / 10); //dezena 
    str[i++] = '0' + (decimal % 10); //unid

    str[i++] = ' '; 
    str[i++] = 'c'; 
    str[i++] = 'm'; 
    str[i] = '\0';
}


void trigger_task(void *p){
    while(true){
        gpio_put(PIN_TRIGGER, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_put(PIN_TRIGGER, 0);
        xSemaphoreGive(xSemaphoreTrigger);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void echo_task(void *p){
    uint64_t t_subida, t_descida;
    while(true){
        if(xQueueReceive(xQueueTime, &t_subida, pdMS_TO_TICKS(100))){
            if(xQueueReceive(xQueueTime, &t_descida, pdMS_TO_TICKS(50))){
                double dur_pulso = (double)(t_descida - t_subida);
                double dist = (dur_pulso * 0.0343) / 2.0;
                xQueueSend(xQueueDistance, &dist, 0);
            }
        }
    }
}

void oled_task(void *p){
    double dist_cm;
    char dist_em_str[20];
    while(1){
        if(xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(500)) == pdTRUE){
            if(xQueueReceive(xQueueDistance, &dist_cm, pdMS_TO_TICKS(100)) == pdTRUE){
                double_pra_str(dist_cm, dist_em_str);
                if(dist_cm <= 100.0){ 
                    gpio_put(LED_PIN_R, 1); gpio_put(LED_PIN_G, 0);
                } else {
                    gpio_put(LED_PIN_R, 0); gpio_put(LED_PIN_G, 0);
                }
                ssd1306_clear(&disp);
                ssd1306_draw_string(&disp, 0, 16, 2, dist_em_str);
                ssd1306_show(&disp);
            } else { 
                gpio_put(LED_PIN_R, 0); gpio_put(LED_PIN_G, 1);
                ssd1306_clear(&disp);
                ssd1306_draw_string(&disp, 0, 24, 2, "Falha!");
                ssd1306_show(&disp);
            }
        }
    }
}

int main() {
    stdio_init_all();

    btns_init();
    led_rgb_init();
    oled_display_init();

    gpio_init(PIN_TRIGGER);
    gpio_set_dir(PIN_TRIGGER, GPIO_OUT);
    gpio_init(PIN_ECHO);
    gpio_set_dir(PIN_ECHO, GPIO_IN);
    gpio_set_irq_enabled_with_callback(PIN_ECHO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    xQueueBtn = xQueueCreate(32, sizeof(uint));
    xQueueTime = xQueueCreate(10, sizeof(uint64_t));
    xQueueDistance = xQueueCreate(5, sizeof(double));
    xSemaphoreTrigger = xSemaphoreCreateBinary();

    xTaskCreate(task_1, "Task 1", 8192, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED Task", 2048, NULL, 1, NULL);
    xTaskCreate(trigger_task, "Trigger Task", 256, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo Task", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true);
}
