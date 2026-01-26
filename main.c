#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// --- 1. CONFIGURACIÓN DE PINES ---
const uint LED_1 = 15;     // Crítico (Rojo)
const uint LED_2 = 14;     // Advertencia (Amarillo)
const uint LED_3 = 12;     // Seguro (Verde)
const uint BUZZER = 20;    // Zumbador
const uint TRIG_PIN = 16;  // Sensor Trig
const uint ECHO_PIN = 17;  // Sensor Echo

/* ----------------------------- Modelo FSM ------------------------------ */
enum state { SEGURO = 0, ADVERTENCIA, CRITICO, STATE_MAX };
enum event { EV_NONE = 0, EV_LEJOS, EV_CERCA, EV_MUY_CERCA, EVENT_MAX };

// --- ACCIONES DE LOS ESTADOS (Control de LEDs) ---
void accion_seguro(void) {
    gpio_put(LED_1, 0); gpio_put(LED_2, 0); gpio_put(LED_3, 1);
    printf("\n[FSM] -> SEGURO\n");
}

void accion_advertencia(void) {
    gpio_put(LED_1, 0); gpio_put(LED_2, 1); gpio_put(LED_3, 0);
    printf("\n[FSM] -> ADVERTENCIA\n");
}

void accion_critico(void) {
    gpio_put(LED_1, 1); gpio_put(LED_2, 0); gpio_put(LED_3, 0);
    printf("\n[FSM] -> CRITICO - ¡PELIGRO!\n");
}

// Funciones de transición simple (ya no manejan el buzzer, solo el estado)
enum state trans_to_seguro(void)  { accion_seguro(); return SEGURO; }
enum state trans_to_adv(void)     { accion_advertencia(); return ADVERTENCIA; }
enum state trans_to_critico(void) { accion_critico(); return CRITICO; }

/* TABLA DE TRANSICIÓN */
enum state (*trans_table[STATE_MAX][EVENT_MAX])(void) = {
    [SEGURO] =      {[EV_CERCA] = trans_to_adv,    [EV_MUY_CERCA] = trans_to_critico},
    [ADVERTENCIA] = {[EV_LEJOS] = trans_to_seguro, [EV_MUY_CERCA] = trans_to_critico},
    [CRITICO] =     {[EV_LEJOS] = trans_to_seguro, [EV_CERCA] = trans_to_adv}
};

/* --------------------------- SOPORTE HARDWARE --------------------------- */
float medir_distancia() {
    gpio_put(TRIG_PIN, 0);
    sleep_us(50);
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
    return (absolute_time_diff_us(start, end) * 0.0343) / 2;
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
    
    uint pins_out[] = {LED_1, LED_2, LED_3, BUZZER, TRIG_PIN};
    for(int i=0; i<5; i++) {
        gpio_init(pins_out[i]);
        gpio_set_dir(pins_out[i], GPIO_OUT);
    }
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    sleep_ms(3000); 
    printf("SCTR: Sistema con Pitido Variable Iniciado\n");

    enum state st = SEGURO;
    accion_seguro();

    while (true) {
        float d = medir_distancia();
        
        if (d > 0) {
            printf("\rDistancia: %.2f cm    ", d);
            enum event ev = event_parser(d);
            
            // 1. Ejecutar FSM para los LEDs
            enum state (*tr)(void) = trans_table[st][ev];
            if (tr != NULL) st = tr();

            // 2. LÓGICA DEL PITIDO SEGÚN ZONA (Amarilla vs Roja)
        if (d < 15.0 && d >= 10.0) { 
            // --- ZONA AMARILLA (Advertencia) ---
            // Pitido a frecuencia fija (ejemplo: cada 250ms)
            gpio_put(BUZZER, 1);
            sleep_ms(60); 
            gpio_put(BUZZER, 0);
            sleep_ms(250); 
        } 
        else if (d < 10.0) {
            // --- ZONA ROJA (Crítica) ---
            // Pitido variable: más rápido cuanto más cerca
            gpio_put(BUZZER, 1);
            sleep_ms(60); 
            gpio_put(BUZZER, 0);
            
            // Cálculo dinámico para la urgencia
            int espera = (int)(d * 15); // Factor reducido para que sea más frenético
            if (espera < 25) espera = 25; // Límite de velocidad máxima
            sleep_ms(espera);
        }
        } else {
            gpio_put(BUZZER, 0);
        }
        
        sleep_ms(50); 
    }
}