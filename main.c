#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// --- 1. CONFIGURACIÓN DE PINES ---
const uint LED_1 = 15;     // Crítico (Rojo)
const uint LED_2 = 14;     // Advertencia (Amarillo)
const uint LED_3 = 13;     // Correcto (Verde)
const uint BUZZER = 20;    // Zumbador (Lado derecho, Pin 26)
const uint TRIG_PIN = 16;  // Sensor Trig
const uint ECHO_PIN = 17;  // Sensor Echo

/* ----------------------------- Modelo FSM ------------------------------ */

/* Estados del sistema */
enum state { SEGURO = 0, ADVERTENCIA, CRITICO, STATE_MAX };

/* Eventos basados en la distancia */
enum event { EV_NONE = 0, EV_LEJOS, EV_CERCA, EV_MUY_CERCA, EVENT_MAX };

// --- ACCIONES DE LOS ESTADOS (Solo se ejecutan al entrar en el estado) ---
void accion_seguro(void) {
    gpio_put(LED_1, 0); gpio_put(LED_2, 0);gpio_put(LED_3, 1); gpio_put(BUZZER, 0);
    printf("\n[FSM] -> SEGURO\n");
}

void accion_advertencia(void) {
    gpio_put(LED_1, 0); gpio_put(LED_2, 1);gpio_put(LED_3, 0); gpio_put(BUZZER, 0);
    printf("\n[FSM] -> ADVERTENCIA\n");
}

void accion_critico(void) {
    gpio_put(LED_1, 1); gpio_put(LED_2, 0);gpio_put(LED_3, 0);
    printf("\n[FSM] -> CRITICO - ¡PELIGRO!\n");
}

// --- HANDLERS DE TRANSICIÓN ---

// Acción cíclica para que el zumbador pite continuamente en CRÍTICO
enum state trans_mantener_critico(void) {
    gpio_put(BUZZER, 1);
    sleep_ms(80); // Duración del pitido
    gpio_put(BUZZER, 0);
    return CRITICO;
}

enum state trans_to_seguro(void)  { accion_seguro(); return SEGURO; }
enum state trans_to_adv(void)     { accion_advertencia(); return ADVERTENCIA; }
enum state trans_to_critico(void) { accion_critico(); return trans_mantener_critico(); }

/* TABLA DE TRANSICIÓN: Estructura del profesor aplicada al SCTR */
enum state (*trans_table[STATE_MAX][EVENT_MAX])(void) = {
    [SEGURO] = {
        [EV_CERCA]     = trans_to_adv,
        [EV_MUY_CERCA] = trans_to_critico
    },
    [ADVERTENCIA] = {
        [EV_LEJOS]     = trans_to_seguro,
        [EV_MUY_CERCA] = trans_to_critico
    },
    [CRITICO] = {
        [EV_LEJOS]     = trans_to_seguro,
        [EV_CERCA]     = trans_to_adv,
        [EV_MUY_CERCA] = trans_mantener_critico // Mantiene el pitido cíclico
    }
};

/* --------------------------- SOPORTE HARDWARE --------------------------- */

float medir_distancia() {
    gpio_put(TRIG_PIN, 0);
    sleep_us(50); // Limpieza de ruido eléctrico
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);

    uint32_t timeout_init = 30000;
    while (gpio_get(ECHO_PIN) == 0) {
        if (timeout_init-- == 0) return -1;
        sleep_us(1);
    }
    absolute_time_t start = get_absolute_time();
    uint32_t timeout_pulse = 30000;
    while (gpio_get(ECHO_PIN) == 1) {
        if (timeout_pulse-- == 0) return -1;
        sleep_us(1);
    }
    absolute_time_t end = get_absolute_time();
    float d = (absolute_time_diff_us(start, end) * 0.0343) / 2;
    return (d < 2.0) ? -1 : d; 
}

enum event event_parser(float d) {
    if (d >= 15.0) return EV_LEJOS;
    if (d >= 10.0 && d < 15.0) return EV_CERCA;
    if (d > 0 && d < 10.0) return EV_MUY_CERCA;
    return EV_NONE;
}

/* -------------------------- PROGRAMA PRINCIPAL -------------------------- */

int main() {
    stdio_init_all();
    
    // Inicialización de GPIOs
    uint pins_out[] = {LED_1, LED_2,LED_3,BUZZER, TRIG_PIN};
    for(int i=0; i<4; i++) {
        gpio_init(pins_out[i]);
        gpio_set_dir(pins_out[i], GPIO_OUT);
    }
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    sleep_ms(3000); 
    printf("SCTR: Sistema Table-Driven Iniciado\n");

    enum state st = SEGURO;
    accion_seguro();

    while (true) {
        float d = medir_distancia();
        
        if (d > 0) {
            printf("\rDistancia: %.2f cm  ", d); // Mostrar distancia en tiempo real
            enum event ev = event_parser(d);
            
            // Lógica de la FSM: se busca la transición en la tabla
            enum state (*tr)(void) = trans_table[st][ev];
            
            if (tr != NULL) {
                st = tr(); // Ejecuta transición y actualiza estado
            }
        } else {
            // Si hay error de sensor, aseguramos que el buzzer se apague
            gpio_put(BUZZER, 0);
        }
        
        sleep_ms(100); 
    }
}
