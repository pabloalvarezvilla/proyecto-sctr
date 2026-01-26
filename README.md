# SCTR - Sensor de Parking con FSM Table-Driven

Este proyecto implementa un sistema de alerta de proximidad utilizando una Raspberry Pi Pico, un sensor HC-SR04 y una máquina de estados finitos basada en tablas

##  Instrucciones de Compilación
Para cumplir con la evaluación, siga estos pasos para compilar desde el repositorio:

1. **Clonar el repositorio:**
   ```bash
   git clone https://github.com/pabloalvarezvilla/proyecto-sctr.git
   cd proyecto_SCTR
2. **Configurar el SDK:**
   ```bash
   export PICO_SDK_PATH=/home/iago/pico-sdk

3. **Compilar:**
   ```bash
   mkdir build && cd build
   cmake ..
   make
