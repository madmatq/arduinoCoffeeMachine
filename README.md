# Máquina de Café Inteligente con Arduino

## Descripción del Proyecto

Esta es la simulacion de una maquina de cafe inteligente, usando Arduino a la vez que sensores y actuadores. Su construccion es relativamente sencilla si se siguen las indicaciones de este post.

Se trata de una maquina con una interfaz de usuario muy intuitiva (user-friendly).
Desde el inicio el usuario recibira feedback constante del estado del programa que se mostrara en el display LCD incluido.

![Imagen principal del proyecto](media/image2.JPG)

---

## Características Principales

### Hardware Utilizado
- **Microcontrolador**: Arduino Elegoo UNO R3
- **Pantalla**: LCD 16x2
- **Sensores**:
  - DHT11 (temperatura y humedad)
  - Sensor ultrasónico HC-SR04 (detección de presencia)
- **Controles**:
  - Joystick analógico con botón
  - Botón adicional para funciones especiales
- **Indicadores**: 2 LEDs (estado y progreso)

![Esquema de conexiones](media/arduinoCoffeeMachine_bb.jpg)

### Funcionalidades del Sistema

#### Sistema de Estados Inteligente
- **Arranque**: Secuencia de inicialización con parpadeo de LEDs
- **Detección automática de clientes**: Usando sensor ultrasónico
- **Servicio activo**: Menú interactivo de productos
- **Preparación de bebidas**: Con indicador de progreso
- **Modo administrador**: Para configuración avanzada

#### Productos Disponibles
- Café Solo (€1.00)
- Café Cortado (€1.10)
- Café Doble (€1.25)
- Café Premium (€1.50)
- Chocolate (€2.00)

---

## Interfaz de Usuario

### Navegación con Joystick
- **Arriba/Abajo**: Navegar por menús y productos
- **Izquierda**: Retroceder en menús
- **Botón del joystick**: Seleccionar/Confirmar

### Controles con Botón Principal
- **Presión corta (1-2s)**: No hace nada
- **Presión media (2-3s)**: Reiniciar servicio
- **Presión larga (5s+)**: Entrar/Salir modo administrador

---

## Modo Administrador

El modo administrador permite acceso a:

### Ver Sensores
- **Temperatura y Humedad**: Lecturas en tiempo real del DHT11
- **Distancia**: Medición del sensor ultrasónico

### Configuración del Sistema
- **Contador de tiempo**: Tiempo de funcionamiento desde el arranque
- **Modificación de precios**: Ajuste dinámico de precios de productos

---

## Arquitectura del Software

### Sistema Multihilo
El código utiliza la librería `Thread` para gestionar múltiples tareas concurrentes:

- **Hilo de Arranque** (1000ms): Secuencia de inicialización
- **Hilo de Sensores** (500ms): Lectura de temperatura, humedad y distancia
- **Hilo de Interfaz** (50ms): Gestión de botones y joystick
- **Hilo de Preparación** (100ms): Control del proceso de preparación
- **Hilo de Admin** (200ms): Actualización del modo administrador

### Características de Seguridad
- **Watchdog Timer**: Reinicio automático en caso de cuelgue
- **Detección de rebotes**: Control preciso de botones
- **Estados protegidos**: Prevención de transiciones inválidas

---

## Instalación y Configuración

### Librerías Requeridas
```cpp
#include <LiquidCrystal.h>
#include <DHT.h>
#include <avr/wdt.h>
#include <Thread.h>
#include <ThreadController.h>
```

### Configuración de Pines
```cpp
// LCD
#define LCD_RS 12
#define LCD_EN 11
#define LCD_D4 5
#define LCD_D5 4
#define LCD_D6 13
#define LCD_D7 2

// Sensores
#define PIN_DHT 7
#define PIN_TRIG 8
#define PIN_ECHO 9

// Controles
#define JOYSTICK_X A0
#define JOYSTICK_Y A1
#define BOTON_JOYSTICK 10
#define PIN_BOTON 6

// LEDs
#define PIN_LED1 A2
#define PIN_LED2 3
```

---

## Funcionamiento

### Video Demostrativo
[url video del funcionamiento completo]

### Flujo de Operación

1. **Arranque del Sistema**
   - Inicialización de componentes
   - Secuencia de parpadeo de LEDs
   - Calibración de sensores

2. **Detección de Cliente**
   - El sensor ultrasónico detecta presencia (< 100cm)
   - Activación automática del servicio
   - Muestra temperatura y humedad inicial

3. **Selección de Producto**
   - Navegación con joystick
   - Visualización de precio en tiempo real
   - Confirmación con botón del joystick

4. **Preparación de Bebida**
   - Tiempo aleatorio de preparación (4-8 segundos)
   - Indicador LED con fade progresivo
   - Mensaje de "Retire bebida" al finalizar

---

## Personalización

### Modificar Productos
```cpp
Producto productos[5] = {
  {"Cafe Solo", 1.00},
  {"Cafe Cortado", 1.10},
  {"Cafe Doble" , 1.25},
  {"Cafe Premium" , 1.50},
  {"Chocolate" , 2.00}
};
```

### Ajustar Tiempos
- Tiempo de detección de cliente
- Duración de preparación
- Timeouts de pantalla

### Configurar Sensores
- Umbral de distancia para detección
- Intervalos de lectura de sensores
- Calibración de temperatura

---

## Referencias

-[URJC-AulaVirtual-Sistemas Empotrados y de tiempo real-2025-Practica 3-pdf](docs/Practica3.pdf)

-[Awesome README](https://github.com/matiassingers/awesome-readme)

-[Fritzing](https://fritzing.org)

---

*Desarrollado por @mtsj (Matias J. Mercado)*
