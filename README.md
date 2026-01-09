/*
 * SPDX-License-Identifier: MIT
 *
 * Ejercicio: Alarma domestica minima (DISARMED/ARMED/ALARM)
 *
 * Simula una alarma con tres estados: desarmada, armada y disparada (sirena).
 *
 * Comportamiento:
 * DISARMED->ARMED con 'a'; ARMED->ALARM con 's'; ALARM->DISARMED con 'd'.
 * El resto de combinaciones quedan como NULL y se validan antes de llamar.
 *
 */

#include <stdio.h>

/* Estados */

enum state {
	DISARMED = 0,
	ARMED,
	ALARM,
	STATE_MAX
};

/* Acciones */

enum event {
	NONE = 0,
	ARM_CMD,
	DISARM_CMD,
	SENSOR_TRIP,
	QUIT,
	EVENT_MAX
};

/* Acciones */

void act_arm(void)
{
	printf("Accion: alarma armada\n");
}

void act_trip(void)
{
	printf("Accion: intrusion detectada, sirena activada\n");
}

void act_disarm(void)
{
	printf("Accion: alarma desarmada y sirena apagada\n");
}

/* Manejadores de transiciones (Transition Handlers)*/

enum state trans_disarmed_a(void)
{
	act_arm();
	return ARMED;
}

enum state trans_armed_s(void)
{
	act_trip();
	return ALARM;
}

enum state trans_alarm_d(void)
{
	act_disarm();
	return DISARMED;
}

/* Tabla de transition handlers */

enum state (*trans_table[STATE_MAX][EVENT_MAX])(void) = {
	[DISARMED] = {
		[ARM_CMD] = trans_disarmed_a,
	},
	[ARMED] = {
		[SENSOR_TRIP] = trans_armed_s,
	},
	[ALARM] = {
		[DISARM_CMD] = trans_alarm_d,
	},
};

/* Parseador de Eventos */

enum event event_parser(int ch)
{
	switch (ch) {
	case 'a':
		return ARM_CMD;
	case 'd':
		return DISARM_CMD;
	case 's':
		return SENSOR_TRIP;
	default:
		return NONE;
	}
}

int main(void)
{

	printf("Alarma: a=armar, d=desarmar, s=sensor\n");
	enum state st = DISARMED;
	act_disarm();

	for (;;) {
		int ch = getchar();
		if (ch == EOF) { break; }
		if (ch == '\n' || ch == '\r') { continue; }

		enum event ev = event_parser(ch);
		enum state (*tr)(void) = trans_table[st][ev];
		if (tr == NULL) { 
			printf("Transicion no definida (st=%d, ev=%d)\n", st, ev);
			continue;
		}
		st = tr(); 
	}

	return 0;
}
