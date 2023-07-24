# FreeRTOS Esercizio 30: Esempio illustrativo di ISR con sezioni critiche, task e spinlock in sistemi multicore

Il programma di esempio analizza l'effetto di uno spinlock condiviso tra ISR e task che girano su due core distinti della ESP32.

Un __timer HW__, associato al __Core1__, emette un interrupt ogni 2 secondi.

Nella `ISR` corrispondente, eseguita dal __Core1__, viene programmata una sezione critica protetta da `spinlock`.

Sempre sul __Core1__ gira anche il __TaskL__, che non impiega lo `spinlock`.

Sul __Core0__ gira il __TaskH__, con priorità maggiore di __TaskL__; esso contiene una sezione critica protetta con lo stesso `spinlock` impiegato dalla `ISR`.

L'analisi della criticità è piuttosto semplice: sia la `ISR` che TaskH condividono lo stesso `spinlock`. Il __TaskH__ (sul __Core0__) ha preso lo `spinlock` ed entra nella propria sezione critica; a questo punto scade il timer hardware e parte l'esecuzione della `ISR` (sul __Core 1__) che tenta di acquisire lo `spinlock` ma resta bloccata poiché lo `spinlock` è preso dal __TaskH__. 

Il __Core 1__ cicla ripetutamente attendendo il rilascio dello `spinlock` da parte di __TaskH__.

Ma dopo poco tempo interviene un _watchdog_, elemento di salvaguardia associato alla durata delle `ISR`, che non venendo aggiornato entro il suo tempo massimo, provoca il core-dump del sistema e il riavvio della ESP32.
