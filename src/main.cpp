/*
 * FreeRTOS Esercizio 30: Esempio illustrativo di ISR con sezioni critiche, task e spinlock in sistemi multicore
 *
 * Il programma di esempio analizza l'effetto di uno spinlock condiviso tra ISR e task 
 * che girano su due core distinti della ESP32. 
 * 
 * Un timer HW, associato al Core1, emette un interrupt ogni 2 secondi
 * Nella ISR corrispondente, eseguita dal Core1, viene programmata una sezione critica protetta da spinlock.
 *
 * Sempre sul Core1 gira anche il TaskL, che non impiega lo spinlock.
 * 
 * Sul Core0 gira il TaskH, con priorità maggiore di TaskL; esso contiene una sezione critica protetta 
 * con lo stesso spinlock impiegato dalla ISR.
 *
 * L'analisi della criticità è piuttosto semplice: sia la ISR che TaskH condividono lo stesso spinlock. 
 * Il TaskH (sul Core0) ha preso lo spinlock ed entra nella propria sezione critica; a questo punto 
 * scade il timer hardware e parte l'esecuzione della ISR (sul Core 1) che tenta di acquisire lo spinlock 
 * ma resta bloccata poiché lo spinlock è preso dal TaskH. 
 * Il Core 1 cicla ripetutamente attendendo il rilascio dello spinlock da parte di TaskH.
 * 
 * Ma dopo poco tempo interviene un watchdog, elemento di salvaguardia associato alla durata delle ISR, 
 * che non venendo aggiornato entro il suo tempo massimo, provoca il core-dump del sistema e il riavvio 
 * della ESP32.
 * 
 * Nota: nel file soc.h sono definiti i riferimenti ai due core della ESP32:
 *   #define PRO_CPU_NUM (0)
 *   #define APP_CPU_NUM (1)
 *
 * Qui vengono adoperati entrambi i core della CPU.
 *
 */

#include <Arduino.h>

// Impostazioni  ***************************************************************

// prescaler del timer hardware (F_tick = 1MHz)
static const uint16_t timer_divider = 80;
// costante di tempo del timer hardware (provare: 2sec, 1sec, 0.1sec)
static const uint64_t timer_max_count = 2000000;
// Tempo di monopolio del core (ms) da parte del Task
static const TickType_t task_wait = 100;
// Tempo di monopolio del core (ms) da parte della ISR
static const TickType_t isr_wait = 20;


// Variabili Globali  **********************************************************

// semaforo di sincronizzazione tra...
static SemaphoreHandle_t bin_sem;
// handle del timer hardware
static hw_timer_t *timer = NULL;
// spinlock di protezione delle sezioni critiche 
// condiviso tra ISR e TaskH
static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;


//*****************************************************************************
// Interrupt Service Routines (ISRs)

// ISR associata al timer hardware, eseguita periodicamente
// Nota: viene eseguita sul Core 1
void IRAM_ATTR onTimer() {

  TickType_t timestamp;
  
  // monopolizza la CPU nella ISR 
  // idea terribile: difficile inventarsi qualcosa peggiore!
  Serial.print("ISR...");

  // sezione critica nella ISR
  portENTER_CRITICAL_ISR(&spinlock);
  timestamp = pdTICKS_TO_MS( xTaskGetTickCount());
  while (pdTICKS_TO_MS( xTaskGetTickCount()) - timestamp < isr_wait);
  portEXIT_CRITICAL_ISR(&spinlock);
  // fine della sezione critica

  Serial.println("Finita la ISR");
}


//*****************************************************************************
// Tasks

// Task L (bassa priorità), sul Core 1
void doTaskL(void *parameters) {
  
  TickType_t timestamp;
  char str[20];

  // Ciclo infinito del task
  while (1) {

    // Stampa un messaggio
    Serial.println("L");
    
    // monopolizza la CPU facendo nulla per un certo intervallo di tempo
    timestamp = pdTICKS_TO_MS(xTaskGetTickCount() );
    while (pdTICKS_TO_MS( xTaskGetTickCount()) - timestamp < task_wait);
  }
}

// Task H (alta priorità), sul Core 0
void doTaskH(void *parameters) {
  
  TickType_t timestamp;
  char str[20];

  // Ciclo infinito del task
  while (1) {

    // Stampa un messaggio
    Serial.print("spinning...");
    // inizia la sezione critica
    portENTER_CRITICAL(&spinlock);
    Serial.println("H");
    portEXIT_CRITICAL(&spinlock);
    // fine della sezione critica
    
    // monopolizza la CPU facendo nulla per un certo intervallo di tempo
    timestamp = pdTICKS_TO_MS(xTaskGetTickCount() );
    while (pdTICKS_TO_MS( xTaskGetTickCount()) - timestamp < task_wait);
  }
}

//************************************************************************************
// Main (sul core 1, con priorità 1)

// configurazione del sistema
void setup()
{
  // Configurazione della seriale
  Serial.begin(115200);

  // breve pausa
  vTaskDelay(pdMS_TO_TICKS(1000));
  Serial.println();
  Serial.println("FreeRTOS Esempio di spinlock e multicore");

  // Configura il timer Hardware sul core 1, triggera la ISR ogni timer_max_count us
  timer = timerBegin(0, timer_divider, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, timer_max_count, true);
  timerAlarmEnable(timer);

  // Crea e avvia i due task;

  // Start Task L (sul core 1)
  xTaskCreatePinnedToCore(doTaskL,
                          "Task L",
                          3072,
                          NULL,
                          1,
                          NULL,
                          APP_CPU_NUM);

  // Start Task H (sul core 0)
  xTaskCreatePinnedToCore(doTaskH,
                          "Task H",
                          3072,
                          NULL,
                          2,
                          NULL,
                          PRO_CPU_NUM);

  // Elimina il task con "Setup e Loop"
  vTaskDelete(NULL);
}

void loop()
{
  // lasciare vuoto
}
