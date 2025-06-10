#include <LiquidCrystal.h>
#include <DHT.h>
#include <avr/wdt.h>
#include <Thread.h>
#include <ThreadController.h>

// Pines

// LCD_VO conectado directamente a 5V con resistencia 1 kOhm
#define LCD_RS 12
#define LCD_EN 11
#define LCD_D4 5
#define LCD_D5 4
#define LCD_D6 13
#define LCD_D7 2

#define PIN_DHT 7
#define TIPO_DHT DHT11

#define PIN_TRIG 8
#define PIN_ECHO 9

#define JOYSTICK_X A0
#define JOYSTICK_Y A1
#define BOTON_JOYSTICK 10

#define PIN_BOTON 6
#define PIN_LED1 A2 // pin analogico para on/off
#define PIN_LED2 3 // pin PWM para 'fade in'

// Pantalla y sensor temp/humedad
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
DHT dht(PIN_DHT, TIPO_DHT);

// Hilos
ThreadController controladorHilos = ThreadController();
Thread* hiloSensores = new Thread();
Thread* hiloInterfaz = new Thread();
Thread* hiloPreparacion = new Thread();
Thread* hiloAdmin = new Thread();
Thread* hiloArranque = new Thread();

// Estados del sistema
enum EstadoSistema {
  ARRANQUE,
  ESPERANDO_SERVICIO,
  SERVICIO_ACTIVO,
  PREPARANDO_BEBIDA,
  MENU_ADMIN,
  ADMIN_TEMPERATURA,
  ADMIN_DISTANCIA,
  ADMIN_CONTADOR,
  ADMIN_PRECIOS
};

// Estructura de producto
struct Producto {
  String nombre;
  float precio;
};

// Variables globales
EstadoSistema estadoActual = ARRANQUE;
EstadoSistema estadoAnterior = ARRANQUE;

Producto productos[5] = {
  {"Cafe Solo", 1.00},
  {"Cafe Cortado", 1.10},
  {"Cafe Doble", 1.25},
  {"Cafe Premium", 1.50},
  {"Chocolate", 2.00}
};

// Variables de control
int productoSeleccionado = 0;
int opcionAdminSeleccionada = 0;
int productoSeleccionadoPrecio = 0;
bool enEdicionPrecio = false;
bool clienteDetectado = false;
bool mostrandoTemperatura = false;
bool preparandoBebida = false;
bool retirando = false;
bool servicioActivoPermanente = false;

// Variables de tiempo
unsigned long tiempoInicio;
unsigned long inicioMostrarTemperatura = 0;
unsigned long inicioPreparacion = 0;
unsigned long duracionPreparacion = 0;
unsigned long inicioRetirar = 0;
unsigned long tiempoUltimaActualizacion = 0;

// Variables del arranque
int parpadeoInicial = 1;
bool estadoLedArranque = false;

// Variables de entrada
bool estadoBotonActual = LOW;
bool estadoBotonAnterior = LOW;
unsigned long tiempoPresionBoton = 0;
bool botonPresionadoActualmente = false;
bool presionProcesada = false;

// Variables del joystick
int ultimoJoystickX = 512;
int ultimoJoystickY = 512;
bool joystickMovido = false;
unsigned long ultimoTiempoJoystick = 0;
bool ultimoBotonJoystick = false;

// Variables de sensores
float temperaturaActual = 0;
float humedadActual = 0;
float distanciaActual = 0;

void setup() {
  Serial.begin(9600);
  
  // Inicializar pines
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(BOTON_JOYSTICK, INPUT_PULLUP);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  
  // Inicializar LCD
  lcd.begin(16, 2);
  lcd.print("CARGANDO...");
  
  // Inicializar DHT
  dht.begin();
  
  // Inicializar semilla aleatoria
  randomSeed(analogRead(A3));
  
  // Registrar tiempo de inicio
  tiempoInicio = millis();
  
  // Habilitar temporizador watchdog (8 segundos)
  wdt_enable(WDTO_8S);
  
  // Configurar hilos
  configurarHilos();
}

void configurarHilos() {
  // Hilo de arranque, se ejecuta cada 1000ms durante el arranque
  hiloArranque->onRun(ejecutarArranque);
  hiloArranque->setInterval(1000);
  
  // Hilo de sensores, lee sensores cada 500ms
  hiloSensores->onRun(leerSensores);
  hiloSensores->setInterval(500);
  
  // Hilo de interfaz, maneja botones y joystick cada 50ms
  hiloInterfaz->onRun(manejarInterfaz);
  hiloInterfaz->setInterval(50);
  
  // Hilo de preparacion, actualiza progreso cada 100ms
  hiloPreparacion->onRun(actualizarPreparacion);
  hiloPreparacion->setInterval(100);
  
  // Hilo admin, actualiza pantalla admin cada 200ms
  hiloAdmin->onRun(actualizarAdmin);
  hiloAdmin->setInterval(200);
  
  // Agregar hilos al controlador
  controladorHilos.add(hiloArranque);
  controladorHilos.add(hiloSensores);
  controladorHilos.add(hiloInterfaz);
  controladorHilos.add(hiloPreparacion);
  controladorHilos.add(hiloAdmin);
}

void loop() {
  wdt_reset(); // Reiniciar temporizador watchdog
  
  // Ejecutar hilos segun el estado actual
  if (estadoActual == ARRANQUE) {
    hiloArranque->run();
  } else {
    hiloSensores->run();
    hiloInterfaz->run();
    
    if (estadoActual == PREPARANDO_BEBIDA) {
      hiloPreparacion->run();
    }
    
    if (estadoActual >= MENU_ADMIN) {
      hiloAdmin->run();
    }
  }
  
  // Manejar cambios de estado
  manejarCambiosEstado();
}

void ejecutarArranque() {
  if (parpadeoInicial < 3) {
    estadoLedArranque = !estadoLedArranque;
    digitalWrite(PIN_LED1, estadoLedArranque);
    delay(1000);
    if (estadoLedArranque) {
      parpadeoInicial++;
    }
  } else {
    digitalWrite(PIN_LED1, LOW);
    estadoActual = ESPERANDO_SERVICIO;
    lcd.clear();
  }
}

void leerSensores() {
  // Leer temperatura y humedad
  temperaturaActual = dht.readTemperature();
  humedadActual = dht.readHumidity();
  
  // Leer distancia
  distanciaActual = obtenerDistancia();
  
  // Solo detectar cliente si no estamos en servicio permanente
  if (!servicioActivoPermanente) {
    bool clienteAnterior = clienteDetectado;
    clienteDetectado = (distanciaActual < 40);
    
    // Si se detecta un nuevo cliente
    if (!clienteAnterior && clienteDetectado && estadoActual == ESPERANDO_SERVICIO) {
      estadoActual = SERVICIO_ACTIVO;
      servicioActivoPermanente = true;
      mostrandoTemperatura = true;
      inicioMostrarTemperatura = millis();
      mostrarTemperaturaHumedad();
      // Serial.println("Cliente detectado, servicio activo permanente");
    }
  }
}

void manejarInterfaz() {
  manejarBoton();
  manejarJoystick();
}

void manejarBoton() {
  estadoBotonActual = digitalRead(PIN_BOTON);
  
  // Detectar cuando se presiona el boton (transicion LOW -> HIGH)
  if (estadoBotonAnterior == LOW && estadoBotonActual == HIGH) {
    tiempoPresionBoton = millis();
    botonPresionadoActualmente = true;
    presionProcesada = false;
    // Serial.println("Boton presionado");
  }
  
  // Mientras el boton esta presionado, mostrar duracion
  if (botonPresionadoActualmente && estadoBotonActual == HIGH) {
    unsigned long duracionActual = millis() - tiempoPresionBoton;
    
    // Debug: mostrar duracion cada 500ms
    if (duracionActual % 500 < 50) {
      // Serial.print("Presionando: ");
      // Serial.print(duracionActual);
      // Serial.println("ms");
    }
  }
  
  // Detectar cuando se suelta el boton (transicion HIGH -> LOW)
  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW && botonPresionadoActualmente) {
    unsigned long duracionPresion = millis() - tiempoPresionBoton;
    
    // Serial.print("Boton liberado. Duracion: ");
    // Serial.print(duracionPresion);
    // Serial.println("ms");
    
    if (!presionProcesada) {
      // Resetear a servicio (2-3 segundos)
      if (estadoActual == SERVICIO_ACTIVO && duracionPresion >= 2000 && duracionPresion < 3000) {
        // Serial.println("Reiniciando servicio");
        reiniciarAServicio();
        presionProcesada = true;
      }
      // Entrar/salir modo admin (5+ segundos)
      else if (duracionPresion >= 5000) {
        if (estadoActual >= MENU_ADMIN) {
          // Serial.println("Saliendo modo admin");
          salirModoAdmin();
        } else {
          // Serial.println("Entrando modo admin");
          entrarModoAdmin();
        }
        presionProcesada = true;
      }
      else if (duracionPresion >= 1000 && duracionPresion < 2000) {
        // Serial.println("Presion corta (1-2s), sin accion");
        presionProcesada = true;
      }
    }
    
    botonPresionadoActualmente = false;
  }
  
  estadoBotonAnterior = estadoBotonActual;
}

void manejarJoystick() {
  int joystickX = analogRead(JOYSTICK_X);
  int joystickY = analogRead(JOYSTICK_Y);
  bool botonJoystick = digitalRead(BOTON_JOYSTICK) == LOW;
  
  // Detectar movimiento del joystick
  if (abs(joystickY - 512) > 300 && !joystickMovido && millis() - ultimoTiempoJoystick > 300) {
    if (joystickY < 300) {
      manejarJoystickArriba();
    } else if (joystickY > 700) {
      manejarJoystickAbajo();
    }
    joystickMovido = true;
    ultimoTiempoJoystick = millis();
  }
  
  if (abs(joystickX - 512) > 300 && !joystickMovido && millis() - ultimoTiempoJoystick > 300) {
    if (joystickX < 300) {
      manejarJoystickIzquierda();
    }
    joystickMovido = true;
    ultimoTiempoJoystick = millis();
  }
  
  if (abs(joystickX - 512) < 200 && abs(joystickY - 512) < 200) {
    joystickMovido = false;
  }
  
  // Manejar presion del boton del joystick
  if (!ultimoBotonJoystick && botonJoystick) {
    manejarPresionBotonJoystick();
  }
  
  ultimoBotonJoystick = botonJoystick;
}

void actualizarPreparacion() {
  if (!preparandoBebida) return;
  
  unsigned long tiempoActual = millis();
  
  if (tiempoActual - inicioPreparacion >= duracionPreparacion) {
    // Preparacion completada
    digitalWrite(PIN_LED2, LOW);
    lcd.clear();
    lcd.print("RETIRE BEBIDA");
    preparandoBebida = false;
    retirando = true;
    inicioRetirar = tiempoActual;
    estadoActual = SERVICIO_ACTIVO;
  } else {
    // Actualizar intensidad LED2 basado en progreso
    float progreso = (float)(tiempoActual - inicioPreparacion) / duracionPreparacion;
    int valorPWM = (int)(progreso * 255);
    analogWrite(PIN_LED2, valorPWM);
  }
}

void actualizarAdmin() {
  if (estadoActual < MENU_ADMIN) return;
  
  digitalWrite(PIN_LED1, HIGH);
  digitalWrite(PIN_LED2, HIGH);
  
  switch (estadoActual) {
    case MENU_ADMIN:
      mostrarMenuAdmin();
      break;
    case ADMIN_TEMPERATURA:
      mostrarAdminTemperatura();
      break;
    case ADMIN_DISTANCIA:
      mostrarAdminDistancia();
      break;
    case ADMIN_CONTADOR:
      mostrarAdminContador();
      break;
    case ADMIN_PRECIOS:
      mostrarAdminPrecios();
      break;
  }
}

void manejarCambiosEstado() {
  // Transiciones automaticas
  if (estadoActual == SERVICIO_ACTIVO) {
    unsigned long tiempoActual = millis();
    
    // Transicion de mostrar temperatura a menu
    if (mostrandoTemperatura && tiempoActual - inicioMostrarTemperatura >= 5000) {
      mostrandoTemperatura = false;
      mostrarMenuProductos();
    }
    
    // Transicion despues de retirar bebida
    if (retirando && tiempoActual - inicioRetirar >= 3000) {
      retirando = false;
      reiniciarAServicio();
    }
  }
  
  // Actualizar pantalla si no hay cliente
  if (estadoActual == ESPERANDO_SERVICIO && !clienteDetectado) {
    if (millis() - tiempoUltimaActualizacion >= 1000) {
      lcd.clear();
      lcd.print("ESPERANDO");
      lcd.setCursor(0, 1);
      lcd.print("CLIENTE");
      tiempoUltimaActualizacion = millis();
    }
  }
}

void manejarJoystickArriba() {
  switch (estadoActual) {
    case SERVICIO_ACTIVO:
      if (!mostrandoTemperatura && !retirando) {
        productoSeleccionado = (productoSeleccionado - 1 + 5) % 5;
        mostrarMenuProductos();
      }
      break;
      
    case MENU_ADMIN:
      opcionAdminSeleccionada = (opcionAdminSeleccionada - 1 + 4) % 4;
      break;
      
    case ADMIN_PRECIOS:
      if (!enEdicionPrecio) {
        productoSeleccionadoPrecio = (productoSeleccionadoPrecio - 1 + 5) % 5;
      } else {
        productos[productoSeleccionadoPrecio].precio += 0.05;
        if (productos[productoSeleccionadoPrecio].precio > 9.99) {
          productos[productoSeleccionadoPrecio].precio = 9.99;
        }
      }
      break;
  }
}

void manejarJoystickAbajo() {
  switch (estadoActual) {
    case SERVICIO_ACTIVO:
      if (!mostrandoTemperatura && !retirando) {
        productoSeleccionado = (productoSeleccionado + 1) % 5;
        mostrarMenuProductos();
      }
      break;
      
    case MENU_ADMIN:
      opcionAdminSeleccionada = (opcionAdminSeleccionada + 1) % 4;
      break;
      
    case ADMIN_PRECIOS:
      if (!enEdicionPrecio) {
        productoSeleccionadoPrecio = (productoSeleccionadoPrecio + 1) % 5;
      } else {
        productos[productoSeleccionadoPrecio].precio -= 0.05;
        if (productos[productoSeleccionadoPrecio].precio < 0.05) {
          productos[productoSeleccionadoPrecio].precio = 0.05;
        }
      }
      break;
  }
}

void manejarJoystickIzquierda() {
  switch (estadoActual) {
    case ADMIN_TEMPERATURA:
    case ADMIN_DISTANCIA:
    case ADMIN_CONTADOR:
    case ADMIN_PRECIOS:
      if (enEdicionPrecio) {
        enEdicionPrecio = false;
      } else {
        estadoActual = MENU_ADMIN;
      }
      break;
  }
}

void manejarPresionBotonJoystick() {
  switch (estadoActual) {
    case SERVICIO_ACTIVO:
      if (!mostrandoTemperatura && !retirando) {
        comenzarPreparacionBebida();
      }
      break;
      
    case MENU_ADMIN:
      switch (opcionAdminSeleccionada) {
        case 0:
          estadoActual = ADMIN_TEMPERATURA;
          break;
        case 1:
          estadoActual = ADMIN_DISTANCIA;
          break;
        case 2:
          estadoActual = ADMIN_CONTADOR;
          break;
        case 3:
          estadoActual = ADMIN_PRECIOS;
          productoSeleccionadoPrecio = 0;
          enEdicionPrecio = false;
          break;
      }
      break;
      
    case ADMIN_PRECIOS:
      enEdicionPrecio = !enEdicionPrecio;
      break;
  }
}

void mostrarTemperaturaHumedad() {
  lcd.clear();
  lcd.print("Temp: ");
  lcd.print(temperaturaActual, 1);
  lcd.print(" C");
  lcd.setCursor(0, 1);
  lcd.print("Hum: ");
  lcd.print(humedadActual, 1);
  lcd.print(" %");
}

void mostrarMenuProductos() {
  lcd.clear();
  lcd.print(">");
  lcd.print(productos[productoSeleccionado].nombre);
  lcd.setCursor(0, 1);
  lcd.print(productos[productoSeleccionado].precio, 2);
  lcd.print(" EUR");
}

void mostrarMenuAdmin() {
  lcd.clear();
  lcd.print(">");
  
  switch (opcionAdminSeleccionada) {
    case 0:
      lcd.print("Ver temperatura");
      break;
    case 1:
      lcd.print("Ver distancia");
      break;
    case 2:
      lcd.print("Ver contador");
      break;
    case 3:
      lcd.print("Modificar precios");
      break;
  }
}

void mostrarAdminTemperatura() {
  lcd.clear();
  lcd.print("Temp: ");
  lcd.print(temperaturaActual, 1);
  lcd.print(" C");
  lcd.setCursor(0, 1);
  lcd.print("Hum: ");
  lcd.print(humedadActual, 1);
  lcd.print(" %");
}

void mostrarAdminDistancia() {
  lcd.clear();
  lcd.print("Distancia:");
  lcd.setCursor(0, 1);
  lcd.print(distanciaActual, 1);
  lcd.print(" cm");
}

void mostrarAdminContador() {
  unsigned long segundosTranscurridos = (millis() - tiempoInicio) / 1000;
  
  lcd.clear();
  lcd.print("Contador:");
  lcd.setCursor(0, 1);
  lcd.print(segundosTranscurridos);
  lcd.print(" seg");
}

void mostrarAdminPrecios() {
  if (!enEdicionPrecio) {
    lcd.clear();
    lcd.print(">");
    lcd.print(productos[productoSeleccionadoPrecio].nombre);
    lcd.setCursor(0, 1);
    lcd.print(productos[productoSeleccionadoPrecio].precio, 2);
    lcd.print(" EUR");
  } else {
    lcd.clear();
    lcd.print("Editando:");
    lcd.setCursor(0, 1);
    lcd.print(productos[productoSeleccionadoPrecio].precio, 2);
    lcd.print(" EUR");
  }
}

void comenzarPreparacionBebida() {
  duracionPreparacion = random(4000, 8001); // 4-8s aleatorio
  inicioPreparacion = millis();
  preparandoBebida = true;
  estadoActual = PREPARANDO_BEBIDA;
  
  lcd.clear();
  lcd.print("Preparando");
  lcd.setCursor(0, 1);
  lcd.print("Cafe...");
}

void reiniciarAServicio() {
  productoSeleccionado = 0;
  mostrandoTemperatura = true;
  inicioMostrarTemperatura = millis();
  estadoActual = SERVICIO_ACTIVO;
  mostrarTemperaturaHumedad();
}

void entrarModoAdmin() {
  estadoActual = MENU_ADMIN;
  opcionAdminSeleccionada = 0;
}

void salirModoAdmin() {
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);
  
  // Resetear el servicio permanente al salir del admin
  servicioActivoPermanente = false;
  clienteDetectado = false;
  estadoActual = ESPERANDO_SERVICIO;
  
  // Serial.println("Saliendo admin, servicio permanente desactivado");
}

float obtenerDistancia() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duracion = pulseIn(PIN_ECHO, HIGH);
  float distancia = (duracion * 0.034) / 2;
  
  return distancia;
}