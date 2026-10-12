# kernel_stm32f411_xinu_tinyusb

Kernel Xinu para STM32F411 (blue pill / pyboard style) que usa **TinyUSB** como
stack USB (CDC + MSC) en lugar del stack USB propietario original.

Este proyecto parte de `kernel_stm32f411` y toma la librería TinyUSB ya
verificada en hardware del proyecto `core_stm32f411_blank`.

## Resumen de los cambios

### 1. Stack USB: propietario → TinyUSB

- Eliminado el stack viejo: `usb/Src/*` (usbd_core, usbd_init, usbd_stm32f4_otgfs),
  `usb/cdc/usb_cdc_api.c`, `usb/msc/msc.c`, `usb/Inc`, `usb/class`, `usb/dwc*
  (USB_OTG)`.
- Integrado TinyUSB completo en `usb/` (device, class/cdc, class/msc, common,
  osal, portable/synopsys/dwc2).
- `Makefile`:
  - `INCLUDES`: `-I usb/Inc -I usb/class -I tinyscript` → `-I usb -I tinyscript`.
  - `SRCUSB`: ahora compila `tusb.c`, `usbd.c`, `tusb_fifo.c`, `cdc_device.c`,
    `msc_device.c`, `dcd_dwc2.c`, `dwc2_common.c`, `usb_app.c`,
    `usb_descriptors.c`.
  - Dependencia `$(USBOBJ): usb/tusb_config.h` para recompilar cuando cambia la
    configuración (evita objetos stale / `SET_CONFIGURATION -110`).

### 2. Dispositivo USB (`usb/usb_app.c` + `usb/usb_descriptors.c`)

- Descriptores: `TUD_CONFIG(1, 3, 0, 0x0062)`, `TUD_CDC(0, 4, 0x81, 8, 0x02,
  0x82, 64)`, `TUD_MSC(2, 5, 0x03, 0x83, 64)`.
- `tusb_config.h`: `CFG_TUD_CDC=1`, `CFG_TUD_MSC=1`, buffers 256,
  `CFG_TUD_MSC_BUFSIZE=512`, VID `0xcafe` PID `0x4237`.
- IRQ: se definen fuertes `OTG_FS_IRQHandler` **y** `OTG_FS_Handler` →
  `tud_int_handler(0)`. El vector table (`system/vectors.c`) apunta a
  `OTG_FS_Handler` (alias weak a `default_IRQ_Handler` que nuestra definición
  fuerte reemplaza).
- `tusb_time_millis_api()` → `count1000` (contador de 1 ms del kernel,
  actualizado por `TIM2_Handler`).
- MSC sobre W25Q128: usa `w25qxxx_drv.read/write` y `flash_spi_getcardinfo()`
  (inicializado por `initFlash` dentro de `sysinit()`).

### 3. Consola de Xinu por CDC (nuevo)

- `system/kprintf.c`: `kputc()` ahora hace `tud_cdc_write(&c, 1)` +
  `tud_cdc_write_flush()`, protegido con `tud_ready()` (no bloquea y descarta
  cuando el host no está conectado).
- `usb/usb_app.c`:
  - `usbtask()`: proceso Xinu que en loop hace `tud_task()`, vuelca el RX del
    CDC hacia la consola (`ttyhandler(1, c, 0)`) y `yield()`.
  - `usb_available()`/`usb_getc()`: envoltorios sobre `tud_cdc_available()` /
    `tud_cdc_read()`.
- `system/clkhandler.c`: el poll de USB que hacía `TIM2_Handler` se eliminó
  (ahora lo hace el proceso `usbtask`).
- `system/initialize.c` `nulluser`: tras `clkinit()`/TIM2, se llama
  `tusb_init()` y `resume(create(usbtask, 2048, 40, "usbtask", 0))`.

### 4. Correcciones del kernel

- **`idleTicks`**: faltaba la definición (usada por `shell/xsh_cpu.c`); el
  baseline no enlazaba. Ahora definida en `system/initialize.c` y se incrementa
  en el `nullprocess`.
- `system/platinit.c`: se quitan `cdc_init_usbd()`/`usbd_enable()`/`usbd_connect()`
  (clock + PA11/PA12 AF10 + `NVIC_EnableIRQ(OTG_FS_IRQn)` se mantienen).
- Eliminados `#include <usb_cdc_conf.h>` de `boot.c`, `syscall.c` y
  `syscall-service.c` (este último + `stdbool.h`, que traía transitivamente).

## Estructura relevante

```
system/initialize.c   nulluser/nullprocess, idleTicks, tusb_init(), usbtask
system/kprintf.c      kputc → tud_cdc_write
system/clkhandler.c   TIM2_Handler (solo clock + preempt)
system/platinit.c     clock + GPIO PA11/PA12 + NVIC OTG_FS
system/vectors.c      OTG_FS_Handler (weak, reemplazada por nuestra fuerte)
usb/usb_app.c         CDC console + MSC + usbtask + IRQ
usb/usb_descriptors.c descriptores CDC (consola) + MSC (flash)
usb/tusb_config.h     configuración TinyUSB
```

## Construir y flashear

```
make kernel.bin
st-flash --connect-under-reset --reset write kernel.bin 0x08010000
```

El USB aparece como dispositivo compuesto: puerto serie virtual (consola Xinu)
e interfaz Mass Storage de 16 MB (~15,2 MiB FAT16) sobre la W25Q128.

## Verificación previa en hardware (referencia `core_stm32f411_blank`)

- CDC: eco ping-pong 20/20 caracteres sin pérdidas.
- MSC: montó `/dev/sda` con FAT16 legible en el host.

---

## Estado actual (arranque y tareas)

El sistema arranca así (`system/initialize.c`):

1. `nulluser()`: GPIO, `meminit()`, `platinit()`, reloj TIM2 + preempt,
   `tusb_init()` (TinyUSB device) y salto al proceso Null.
2. `nullprocess()`: `syscall_init()`, crea las 4 tareas del sistema y las
   lanza:
   - `ready(usbtask)` → `ready(blink)` → `ready(shell)` → `resume(initFat32)`.
   - `initFat32` (prio 50) inicializa la W25Q128, monta FAT32 e imprime
     `boot: <config>` (lee `/config/kernel` de forma segura).

Layout actual de prioridades:

| Pid | Proceso | Prio | Función |
|----|---------|------|---------|
| 0  | Null process | 10 | Idle |
| 1  | fat32 | 50 | Montar FS (one-shot, muere al terminar) |
| 2  | usbtask | 52 | `tud_task()` + bombeo CDC→consola |
| 3  | blink | 52 | Parpadea PC13 con `sleepms(500)` |
| 4  | shell | 52 | Consola interactiva |
| —  | hijos de comandos | 20 | `ps`, `help`, `cat`, etc. (no builtin) |

## Consola al conectar (banner + bootinfo)

El banner ya no se imprime al encender: la shell espera a que un terminal
abra el CDC (DTR) con `usb_cdc_connected()` y entonces muestra, en orden:

1. `bootinfo()` — build date, memoria libre, layout de texto/datos.
2. Banner de Xinu (texto plano, sin códigos ANSI) + prompt `xsh $`.

El bloque de info de memoria se movió de `nulluser` a `bootinfo()` (público),
que imprime por consola (`printf`) al conectarse; se eliminó el `kprintf` de
boot.

## Cómo crear tus propias tareas

Toda tarea es una función `int` que se lanza con `create()` + `ready()`.
Ejemplo (desde `nullprocess` o desde cualquier proceso):

```c
int mytask(void) {
    while (1) {
        /* trabajo... */
        sleepms(100);   /* NUNCA delay() aqui            */
        /* cierra el ciclo bloqueando siempre           */
    }
    return OK;
}

/* en nullprocess(): */
ready(create(mytask, 4096, 40, "mytask", 0, NULL));
```

### Reglas de prioridad (scheduler de este port)

El kernel es de **prioridad estricta + round-robin entre iguales**:
- `insert()` coloca procesos de igual prioridad al final de la cola → entre
  iguales se turnan por tick.
- `PendSV` retorna sin cambiar de contexto si el proceso actual tiene mayor
  prioridad que el primero de la cola de listos → **`yield()` no cede el CPU
  a procesos de menor prioridad**.
- `delay()` es **busy-wait**: jamás lo uses dentro de una tarea mientras
  existan tareas de menor prioridad esperando CPU (productor de inanición).
  Usa `sleepms()`/`sleep()` que bloquean de verdad.

Guía práctica:

| Prioridad | Uso |
|-----------|-----|
| 60–80 | Servicios críticos de tiempo real (evitar salvo necesidad) |
| 50–59 | Servicios del sistema (`usbtask`/`shell`/`blink` = 52, `fat32` = 50) |
| 30–50 | **Tareas de aplicación normales** |
| 10–30 | Background; bajo 20 solo corre cuando la shell está idle |

Puntos clave:

- Si tu tarea debe responder aunque la shell esté ejecutando un comando no
  builtin, dale prioridad **> 20** (los hijos de comandos corren a 20,
  `SHELL_CMDPRIO` en `include/shell.h`).
- Tareas de igual prioridad compute-bound (sin dormir) se reparten el CPU por
  round-robin de 1 tick.
- Stack: usa 2048/4096 bytes de base. Con `printf`/`fopen` se necesita margen.
  `SHELL_CMDSTK` es `4096/2`.

## Shell y comandos de archivos

Comandos de FS disponibles (todos sobre la W25Q128 vía fat_filelib):

| Comando | Descripción |
|---------|-------------|
| `ls [dir]` | Lista el directorio actual u otro (resuelto con `full_path`) |
| `cd <dir>` / `pwd` | Cambiar/mostrar directorio |
| `cat <file>... [> out]` | Muestra archivos o los concatena a otro |
| `cp <src> <dst>` | Copia un archivo (512 B por bloque) |
| `mv <src> <dst>` | Mueve/renombra (copia + elimina) |
| `touch <file>` / `mkdir <dir>` | Crear archivo vacío / directorio |
| `rm <file>` | Borra un archivo (`rmdir` no está soportado por la librería) |
| `fsstat` | Tamaño del flash + estado de montaje FAT32 |

Nota: `full_path()` devuelve puntero al buffer global `path`; si necesitas
dos rutas a la vez, cópialas a buffers locales (ver `xsh_cp.c`).

## Almacenamiento: dos discos en MSC (2 LUNs)

El dispositivo USB es Mass Storage de **2 LUNs** en la misma interfaz:

- **LUN 0** → W25Q128 (`/dev/sda`): también es el FS del kernel (fat_filelib).
- **LUN 1** → tarjeta SD por SPI2 (`/dev/sdb`).

El numero de LUNs se controla sobrescribiendo `tud_msc_get_maxlun_cb()` (devuelve 2)
en `usb/usb_app.c`; los callbacks de capacidad/lectura/escritura despachan por LUN.

Puntos clave:

- La SD se inicializa en `initFat32()` (`usb_sd_init()`): `test_unit_ready` de
  LUN 1 devuelve `false` hasta entonces → el host espera (poll de ready) y no
  consulta capacidad antes de tiempo. Si no hay tarjeta, LUN 1 queda not-ready.
- Pines asumidos por el driver SD (`spi/Src/spi.c` `hal_sd_spi_init`):
  **SPI2**: SCK=PB13, MISO=PB14, MOSI=PB15, CS=PB12. Si tu SD está en otros
  pines, ajustalos ahí.
- El FS del kernel sigue montando la W25Q128 (`disk.c`); la SD es un disco
  adicional que ve el host. Para dar formato a la SD como FAT32 usa el host
  (p.ej. `sudo mkfs.vfat /dev/sdb`) o una herramienta SD.
- `fsstat` muestra el tamano de la flash y de la SD.

## Makefile

- `-MMD -MP` + `-include $(OBJDIR)/*.d`: editar un `.h` recompila quien lo
  incluya (evita objetos stale, causó el banner con códigos ANSI viejos).
- `make` → `kernel.bin`; `make flash` flashea; `make clean` reconstruye todo.