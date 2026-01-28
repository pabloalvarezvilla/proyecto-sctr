#include <stdio.h>                 // Biblioteca estándar de C para E/S (printf, etc.)
#include "pico/stdlib.h"           // Funciones básicas de la Raspberry Pi Pico (GPIO, sleep, etc.)
#include "hardware/gpio.h"         // Funciones específicas de configuración y uso de GPIO
#include "hardware/timer.h"        // Funciones de temporización y tiempo absoluto

// --- 1. CONFIGURACIÓN DE PINES ---
// Constantes que indican qué GPIO se usa para cada LED, el buzzer y el sensor HC-SR04
const uint LED_1 = 15;     // Crítico (Rojo)  -> LED que indica zona de peligro
const uint LED_2 = 14;     // Advertencia (Amarillo) -> LED que indica zona de advertencia/intermedia
const uint LED_3 = 12;     // Seguro (Verde) -> LED que indica que la distancia es segura
const uint BUZZER = 20;    // Zumbador -> señal acústica
const uint TRIG_PIN = 16;  // Sensor Trig -> salida de disparo del HC-SR04
const uint ECHO_PIN = 17;  // Sensor Echo -> entrada donde se recibe el eco del HC-SR04

/* ----------------------------- Modelo FSM ------------------------------ */
// Definición de la máquina de estados finita (FSM): estados y eventos

// Estados posibles del sistema en función de la distancia medida
enum state { SEGURO = 0, ADVERTENCIA, CRITICO, STATE_MAX };

// Eventos que representan rangos de distancia (o ausencia de evento)
enum event { EV_NONE = 0, EV_LEJOS, EV_CERCA, EV_MUY_CERCA, EVENT_MAX };

// --- ACCIONES DE LOS ESTADOS (Control de LEDs) ---
// Cada función define cómo deben quedar los LEDs cuando se entra en ese estado

void accion_seguro(void) {
    // Estado SEGURO: LEDs rojo y amarillo apagados, verde encendido
    gpio_put(LED_1, 0); gpio_put(LED_2, 0); gpio_put(LED_3, 1);
    printf("\n[FSM] -> SEGURO\n");
}

void accion_advertencia(void) {
    // Estado ADVERTENCIA: LED amarillo encendido, rojo y verde apagados
    gpio_put(LED_1, 0); gpio_put(LED_2, 1); gpio_put(LED_3, 0);
    printf("\n[FSM] -> ADVERTENCIA\n");
}

void accion_critico(void) {
    // Estado CRITICO: LED rojo encendido, amarillo y verde apagados
    gpio_put(LED_1, 1); gpio_put(LED_2, 0); gpio_put(LED_3, 0);
    printf("\n[FSM] -> CRITICO - ¡PELIGRO!\n");
}

// Funciones de transición simple (ya no manejan el buzzer, solo el estado)
// Cada transición ejecuta la acción del estado de destino y devuelve el nuevo estado

enum state trans_to_seguro(void)  { accion_seguro(); return SEGURO; }

enum state trans_to_adv(void)     { accion_advertencia(); return ADVERTENCIA; }

enum state trans_to_critico(void) { accion_critico(); return CRITICO; }

/* TABLA DE TRANSICIÓN */
// Matriz de punteros a función: dado estado actual y evento, se obtiene la transición
// Si una casilla está vacía (implícitamente NULL), significa que ese evento no provoca cambio
enum state (*trans_table[STATE_MAX][EVENT_MAX])(void) = {
    [SEGURO] =      {[EV_CERCA] = trans_to_adv,    [EV_MUY_CERCA] = trans_to_critico},
    [ADVERTENCIA] = {[EV_LEJOS] = trans_to_seguro, [EV_MUY_CERCA] = trans_to_critico},
    [CRITICO] =     {[EV_LEJOS] = trans_to_seguro, [EV_CERCA] = trans_to_adv}
};

/* --------------------------- SOPORTE HARDWARE --------------------------- */
// Función de medida de distancia con el HC-SR04
// Devuelve la distancia en centímetros o -1 en caso de error/timeout
float medir_distancia() {
    // Aseguramos que TRIG está inicialmente en nivel bajo
    gpio_put(TRIG_PIN, 0);
    sleep_us(50);          // Pausa corta para evitar ruido residual
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);          // Pulso de 10 microsegundos en TRIG para disparar el sensor
    gpio_put(TRIG_PIN, 0);

    // Espera a que ECHO pase a nivel alto, con timeout para evitar bloqueo
    uint32_t timeout_init = 30000;
    while (gpio_get(ECHO_PIN) == 0) {
        if (timeout_init-- == 0) return -1;   // Si se agota el tiempo, error
        sleep_us(1);
    }

    // Registro del tiempo de inicio del pulso de eco
    absolute_time_t start = get_absolute_time();

    // Espera a que ECHO vuelva a nivel bajo, de nuevo con timeout
    uint32_t timeout_pulse = 30000;
    while (gpio_get(ECHO_PIN) == 1) {
        if (timeout_pulse-- == 0) return -1;  // Timeout si el pulso es demasiado largo
        sleep_us(1);
    }
    // Registro del tiempo de fin del pulso de eco
    absolute_time_t end = get_absolute_time();

    // Cálculo de la distancia:
    // tiempo(us) * velocidad del sonido (0.0343 cm/us) / 2 (ida y vuelta)
    return (absolute_time_diff_us(start, end) * 0.0343) / 2;
}

// Convierte una distancia en un evento lógico para la FSM
enum event event_parser(float d) {
    if (d >= 15.0) return EV_LEJOS;               // Distancia segura (lejos)
    if (d >= 10.0 && d < 15.0) return EV_CERCA;   // Zona de advertencia (intermedia)
    if (d > 0 && d < 10.0) return EV_MUY_CERCA;   // Zona crítica (muy cerca)
    return EV_NONE;                               // Sin evento o lectura no válida
}

/* -------------------------- PROGRAMA PRINCIPAL -------------------------- */
int main() {
    // Inicializa la E/S estándar (USB/UART) para usar printf en la Pico
    stdio_init_all();
    
    // Vector con los pines que se configuran como salida (LEDs, buzzer y TRIG)
    uint pins_out[] = {LED_1, LED_2, LED_3, BUZZER, TRIG_PIN};
    for(int i=0; i<5; i++) {
        gpio_init(pins_out[i]);               // Inicializa el GPIO correspondiente
        gpio_set_dir(pins_out[i], GPIO_OUT);  // Lo configura como salida
    }
    // El pin de ECHO se configura como entrada para leer el pulso del sensor
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    // Pequeña espera antes de empezar (por ejemplo, para abrir el monitor serie)
    sleep_ms(3000);
    printf("SCTR: Sistema con Pitido Variable Iniciado\n");

    // Estado inicial de la FSM: SEGURO
    enum state st = SEGURO;
    accion_seguro();   // Aplica las salidas correspondientes al estado SEGURO

    while (true) {
        // Medir la distancia actual
        float d = medir_distancia();
        
        if (d > 0) {
            // Mostrar distancia por consola en tiempo real (sobrescribe la misma línea)
            printf("\rDistancia: %.2f cm    ", d);
            // Obtener el evento asociado a la distancia
            enum event ev = event_parser(d);
            
            // 1. Ejecutar FSM para los LEDs
            // Localiza la transición definida para (estado actual, evento)
            enum state (*tr)(void) = trans_table[st][ev];
            if (tr != NULL) st = tr();    // Si hay transición, se ejecuta y actualiza el estado

            // 2. LÓGICA DEL PITIDO SEGÚN ZONA (Amarilla vs Roja)
        if (d < 15.0 && d >= 10.0) {
            // --- ZONA AMARILLA (Advertencia) ---
            // Aseguramos que el LED rojo esté apagado (no estamos en zona crítica)
            gpio_put(LED_1, 0);
            
            // El LED naranja (amarillo) parpadea al ritmo del buzzer
            gpio_put(BUZZER, 1);
            gpio_put(LED_2, 1);   // Enciende LED de advertencia junto con el buzzer
            sleep_ms(60);
            
            gpio_put(BUZZER, 0);
            gpio_put(LED_2, 0);   // Apaga LED de advertencia cuando se apaga el buzzer
            sleep_ms(250);        // Pausa fija entre pitidos (frecuencia constante)
        }
        else if (d < 10.0) {
            // --- ZONA ROJA (Crítica) ---
            // Apaga el LED de advertencia por si veníamos de la zona amarilla
            gpio_put(LED_2, 0);
            
            // Enciende el LED rojo de forma fija para indicar peligro
            gpio_put(LED_1, 1);
            
            // Pitido variable urgente: un pitido seguido de un tiempo de espera
            gpio_put(BUZZER, 1);
            sleep_ms(60);
            gpio_put(BUZZER, 0);
            
            // Tiempo de espera depende de la distancia:
            // cuanto más pequeña la distancia, más corto el tiempo y más "rápidos" los pitidos
            int espera = (int)(d * 15);  // Factor para ajustar la sensación de urgencia
            if (espera < 25) espera = 25; // No dejar que el tiempo sea demasiado pequeño
            sleep_ms(espera);
        }
        } else {
            // Si la distancia no es válida (d <= 0), se apaga el buzzer
            gpio_put(BUZZER, 0);
        }
        
        // Pequeño retardo general del bucle para limitar la frecuencia de muestreo
        sleep_ms(50);
    }
}
