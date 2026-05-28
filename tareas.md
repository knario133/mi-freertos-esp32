# Framework de Comunicación Inter-Core (ESP32-WROOM-32 + ESP-IDF/FreeRTOS)

## 1) Diseño de la Capa de Intercambio (`shared_data.h`)

### 1.1 Aislamiento de dominios por núcleo
- **Core 0 — Dominio Sistema/Comunicaciones**
  - Redes, protocolo, telemetría, parsing de comandos, logging no crítico.
  - Produce setpoints y consume snapshots de estado.
- **Core 1 — Dominio Control/Sensores**
  - Muestreo periódico, control de lazo, drivers de tiempo crítico.
  - Produce snapshots y consume setpoints.

### 1.2 Estructura de datos optimizada para bajo overhead de coherencia de caché
- `intercore_shared_data_t` define **mailboxes por canal** (`QueueHandle_t mailbox[N]`) con longitud 1.
- Cada canal usa `xQueueOverwrite` (`latest-wins`) para evitar colas largas y contención.
- Estados simples se modelan con atómicos:
  - `s_system_flags` (`_Atomic uint32_t`) para banderas booleanas de estado.
  - `s_fast_counter` (`_Atomic uint32_t`) para contador compartido monotónico.
- Resultado: mínima sincronización explícita en app, coste predecible y sin `Mutex/Semaphore` en lógica de negocio.

### 1.3 API pública encapsulada
- Escritura:
  - `system_data_push(...)`
  - `system_data_push_from_isr(...)`
- Lectura:
  - `system_data_pull(...)`
- Estado atómico:
  - `system_flag_set/clear/is_set(...)`
  - `system_counter_increment/get()`

> Regla: ninguna tarea de aplicación accede directamente a `QueueHandle_t` ni a variables globales compartidas.

---

## 2) Políticas de pinning (`xTaskCreatePinnedToCore`)

### Inicialización exacta recomendada
1. Inicializar capa compartida con `intercore_shared_data_init(&shared)`.
2. Crear tarea de control/sensores en Core 1 con prioridad mayor:
   - `xTaskCreatePinnedToCore(control_sensor_task, ..., PRIO_CONTROL_LOOP, ..., 1)`
3. Crear tarea sistema/comunicaciones en Core 0 con prioridad menor:
   - `xTaskCreatePinnedToCore(system_comm_task, ..., PRIO_SYSTEM_COMM, ..., 0)`

### Política de prioridades
- `PRIO_CONTROL_LOOP > PRIO_SYSTEM_COMM`
- El lazo de control nunca debe depender de bloqueos de comunicación.

---

## 3) Optimizaciones de memoria y latencia

### Funciones candidatas a IRAM_ATTR
- `system_data_push(...)`
- `system_data_push_from_isr(...)`
- ISRs que llamen wrappers inter-core.

### Criterios
- Caminos ejecutados desde ISR o de alta frecuencia en control deben residir en RAM para latencia estable.
- Mantener payloads pequeños y contiguos para reducir tráfico de caché y copia.

---

## 4) Cero Bloqueo entre Core 1 y Core 0

### Mecanismo
- Core 1 publica snapshots con `xQueueOverwrite` (no espera espacio).
- Core 0 publica setpoints con `xQueueOverwrite` (no espera consumidor).
- Señales de disponibilidad/estado mediante atómicos (`system_flag_*`), sin semáforos.

### Resultado
- Una tarea de lectura de periféricos en Core 1 **no queda BLOCKED** por tráfico de Core 0.
- Si no hay dato nuevo, Core 1 continúa con el último estado operativo y su ciclo temporal.

---

## 5) Reglas de implementación del componente

1. **No tocar Kernel**: no modificar `freertos/FreeRTOS-Kernel*`.
2. **Patrón**: `Lock-Free Message Passing` con mailboxes de overwrite + atómicos.
3. **Criterio de validación**:
   - Código de aplicación sin `xSemaphoreTake` ni `xSemaphoreGive`.
   - Acceso a compartidos sólo vía wrappers `system_data_push/pull` y atómicos encapsulados.
4. **Enfoque de estabilidad del chip**:
   - Afinidad de núcleos estricta por dominio.
   - Evitar bloqueos cruzados entre control y comunicaciones.
