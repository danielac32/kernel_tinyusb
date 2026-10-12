#if 0
/* usb_app.c - TinyUSB device: CDC (Xinu console) + MSC (W25Q128 SPI flash)
 *
 * Integration with the Xinu kernel:
 *   - OTG_FS IRQ drives tud_int_handler()
 *   - Tiberius console (tty) is fed from CDC RX by the usbtask process
 *   - kprintf/kputc write to CDC via tud_cdc_write()
 *   - MSC exposes the W25Q128 as a Mass Storage drive
 */

#include "tusb.h"
#include "w25qxxx.h"
#include <string.h>

volatile uint32_t cdc_rx_bytes = 0; /* counts bytes consumed by tud_cdc_rx_cb */

/* DIAG (SWD-readable): boot milestone from initialize.c */
extern volatile uint32 dbg_magic;

/* ------------------------------------------------------------------ */
/*  xinu clock tick (ms)                                               */
/* ------------------------------------------------------------------ */
extern uint32 count1000;

uint32_t tusb_time_millis_api(void) {
  return count1000;
}

/* ------------------------------------------------------------------ */
/*  USB IRQ handler (vector table entry: OTG_FS_IRQHandler)            */
/* ------------------------------------------------------------------ */
void OTG_FS_IRQHandler(void) { tud_int_handler(0); }
TU_ATTR_WEAK void tusb_apply_dcd_update(void) {}

/* ================================================================== */
/*  CDC console backend                                               */
/* ================================================================== */

/* Number of bytes pending in the CDC RX fifo. */
uint16_t usb_available(void) {
  return (uint16_t)tud_cdc_available();
}

/* Read one byte from CDC RX, or -1 if no data. */
int usb_getc(void) {
  uint8_t b;
  return (tud_cdc_read(&b, 1) == 1) ? (int)b : -1;
}

/* True once a terminal has opened the CDC port (DTR asserted). */
int usb_cdc_connected(void) {
  return tud_cdc_connected() ? 1 : 0;
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)itf; (void)dtr; (void)rts;
}
void tud_cdc_set_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p) {
  (void)itf; (void)p;
}

/* Pump CDC RX into the Xinu console (tty) and run the TinyUSB device
 * task. This runs as a Xinu process (prio 52, round-robin con shell/blink). */
extern void ttyhandler(uint32 xnum, char c, int flag);
extern int sleepms(int32);

int usbtask(void) {
  for (;;) {
    tud_task();
    dbg_magic = 7;   /* DIAG: usbtask alive (SWD-readable) */

    while (usb_available()) {
      int c = usb_getc();
      if (c < 0) break;
      ttyhandler(1, (char)c, 0);
    }

    /* sleepms: bloquea 1 ms y deja correr a procesos de menor/igual
     * prioridad (yield() no cede el CPU si somos los de mayor prioridad,
     * por el early-return de PendSV). */
    sleepms(1);
  }
  return 0;
}

/* ================================================================== */
/*  MSC over W25Q128                                                    */
/* ================================================================== */

/* ================================================================== */
/*  MSC over W25Q128 (LUN 0) + SD card via SPI2 (LUN 1)                */
/* ================================================================== */

#include "sd-spi.h"
extern const sd_card_drv_t sd_card_drv;

#define LUN_FLASH 0   /* LUN 0: W25Q128 */
#define LUN_SD    1   /* LUN 1: tarjeta SD via SPI2 */

static uint32_t msc_block_count;
static uint16_t msc_block_size;

static bool     sd_present = false;
static uint32_t sd_block_count;
static uint16_t sd_block_size;

/* Analiza la tarjeta SD (LUN 1). Devuelve tamano en sectores o -1. */
int usb_sd_init(void) {
  sd_info_t *si;

  sd_present = false;
  sd_card_drv.init();
  if (sd_card_drv.reset() != sd_err_ok)
    return -1;
  si = sd_card_drv.getcardinfo();
  if (si == NULL)
    return -1;
  sd_block_count = si->card_size;
  sd_block_size  = si->sect_size;
  sd_present = true;
  return (int)sd_block_count;
}

/* Tamano de la SD en sectores (0 si no hay tarjeta). */
uint32_t usb_sd_size(void) {
  return sd_present ? sd_block_count : 0;
}

/* Dos LUNs: 0 = W25Q128, 1 = SD. GET MAX LUN devuelve n-1. */
uint8_t tud_msc_get_maxlun_cb(void) { return 2; }

/* Tamano de bloque del LUN dado (512 B en ambos) */
static uint16_t msc_lun_block_size(uint8_t lun) {
  return (lun == LUN_FLASH) ? msc_block_size : sd_block_size;
}

uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *r,
                             uint32_t bufsize) {
  (void)bufsize;
  const char *vendor, *product;
  const char rev[] = "1.0 ";

  if (lun == LUN_FLASH) {
    vendor  = "DANIEL  ";
    product = "FLASH XINU    ";
  } else {
    vendor  = "DANIEL  ";
    product = "SDCARD XINU   ";
  }
  r->peripheral_device_type = 0;
  r->peripheral_qualifier   = 0;
  r->is_removable           = 1;
  r->version                = 6;
  r->response_data_format   = 2;
  r->additional_length      = 0x1F;
  for (uint8_t i = 0; i < 8;  i++) r->vendor_id[i]  = (i < strlen(vendor))  ? vendor[i]  : ' ';
  for (uint8_t i = 0; i < 16; i++) r->product_id[i] = (i < strlen(product)) ? product[i] : ' ';
  for (uint8_t i = 0; i < 4;  i++) r->product_rev[i] = rev[i];
  return sizeof(scsi_inquiry_resp_t);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  return (lun == LUN_FLASH) ? true : sd_present;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
  if (lun == LUN_FLASH) {
    flash_info_t *info = flash_spi_getcardinfo();
    msc_block_count = (info) ? info->card_size : 0;
    msc_block_size  = (info) ? info->sect_size : 512;
  } else {
    msc_block_count = sd_present ? sd_block_count : 0;
    msc_block_size  = sd_block_size ? sd_block_size : 512;
  }
  *block_count = msc_block_count;
  *block_size  = msc_block_size;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t pc, bool start,
                           bool load_eject) {
  (void)lun; (void)pc; (void)start; (void)load_eject;
  return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
  (void)offset;
  uint32_t blocks = bufsize / msc_lun_block_size(lun);

  if (lun == LUN_FLASH) {
    w25qxxx_drv.read(buffer, lba, blocks);
  } else {
    if (!sd_present) return -1;
    if (sd_card_drv.read(buffer, lba, blocks) != sd_err_ok) return -1;
  }
  return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
  (void)offset;
  uint32_t blocks = bufsize / msc_lun_block_size(lun);

  if (lun == LUN_FLASH) {
    w25qxxx_drv.write(buffer, lba, blocks);
  } else {
    if (!sd_present) return -1;
    if (sd_card_drv.write(buffer, lba, blocks) != sd_err_ok) return -1;
  }
  return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) { (void)lun; return true; }

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const cmd[16],
                        void *buffer, uint16_t bufsize) {
  (void)lun; (void)cmd; (void)buffer; (void)bufsize;
  return -1;
}
#else
/* usb_app.c - TinyUSB device: CDC (Xinu console) + MSC (W25Q128 SPI flash)
 *
 * Integration with the Xinu kernel:
 *   - OTG_FS IRQ drives tud_int_handler()
 *   - Tiberius console (tty) is fed from CDC RX by the usbtask process
 *   - kprintf/kputc write to CDC via tud_cdc_write()
 *   - MSC exposes the W25Q128 as a Mass Storage drive
 *
 * LUN 0 (W25Q128): compartido entre host USB y Xinu -> protocolo de
 *                  propiedad (flash_detached).
 * LUN 1 (SD):      solo lo usa el host. Xinu no la toca, no hace
 *                  falta control de propiedad.
 */

#include "tusb.h"
#include "w25qxxx.h"
#include <string.h>

volatile uint32_t cdc_rx_bytes = 0; /* counts bytes consumed by tud_cdc_rx_cb */

/* DIAG (SWD-readable): boot milestone from initialize.c */
extern volatile uint32 dbg_magic;

/* ------------------------------------------------------------------ */
/*  xinu clock tick (ms)                                               */
/* ------------------------------------------------------------------ */
extern uint32 count1000;

uint32_t tusb_time_millis_api(void) {
  return count1000;
}

/* ------------------------------------------------------------------ */
/*  USB IRQ handler (vector table entry: OTG_FS_IRQHandler)            */
/* ------------------------------------------------------------------ */
void OTG_FS_IRQHandler(void) { tud_int_handler(0); }
TU_ATTR_WEAK void tusb_apply_dcd_update(void) {}

/* ================================================================== */
/*  CDC console backend                                               */
/* ================================================================== */

/* Number of bytes pending in the CDC RX fifo. */
uint16_t usb_available(void) {
  return (uint16_t)tud_cdc_available();
}

/* Read one byte from CDC RX, or -1 if no data. */
int usb_getc(void) {
  uint8_t b;
  return (tud_cdc_read(&b, 1) == 1) ? (int)b : -1;
}

/* True once a terminal has opened the CDC port (DTR asserted). */
int usb_cdc_connected(void) {
  return tud_cdc_connected() ? 1 : 0;
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
  (void)itf; (void)dtr; (void)rts;
}
void tud_cdc_set_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p) {
  (void)itf; (void)p;
}

/* Pump CDC RX into the Xinu console (tty) and run the TinyUSB device
 * task. This runs as a Xinu process (prio 52, round-robin con shell/blink). */
extern void ttyhandler(uint32 xnum, char c, int flag);
extern int sleepms(int32);

int usbtask(void) {
  for (;;) {
    tud_task();
    dbg_magic = 7;   /* DIAG: usbtask alive (SWD-readable) */

    while (usb_available()) {
      int c = usb_getc();
      if (c < 0) break;
      ttyhandler(1, (char)c, 0);
    }

    /* sleepms: bloquea 1 ms y deja correr a procesos de menor/igual
     * prioridad (yield() no cede el CPU si somos los de mayor prioridad,
     * por el early-return de PendSV). */
    sleepms(1);
  }
  return 0;
}

/* ================================================================== */
/*  MSC over W25Q128 (LUN 0) + SD card via SPI2 (LUN 1)                */
/* ================================================================== */

#include "sd-spi.h"
extern const sd_card_drv_t sd_card_drv;

#define LUN_FLASH 0   /* LUN 0: W25Q128 (compartido host/Xinu) */
#define LUN_SD    1   /* LUN 1: SD, solo el host               */

static uint32_t msc_block_count;
static uint16_t msc_block_size;

static bool     sd_present = false;
static uint32_t sd_block_count;
static uint16_t sd_block_size;

/* ------------------------------------------------------------------ */
/*  Propiedad del LUN 0 (W25Q128): host USB vs shell Xinu              */
/*                                                                     */
/*  flash_detached = true -> la flash es del shell Xinu. El host ve    */
/*  el LUN como "no medium" (sense NOT READY / ASC 0x3A) y deja de     */
/*  hacer I/O, sin necesidad de re-enumerar el USB.                    */
/*                                                                     */
/*  volatile: la flag la setea el shell (otro proceso) y la leen los   */
/*  callbacks MSC en usbtask. Un bool volatile es atomico en ARM.      */
/* ------------------------------------------------------------------ */
static volatile bool flash_detached = false;

/* API publica para el shell:
 *   void msc_flash_detach(void);
 *   void msc_flash_attach(void);
 *   bool msc_flash_is_detached(void);
 */
void msc_flash_detach(void) { flash_detached = true; }
void msc_flash_attach (void) { flash_detached = false; }
bool msc_flash_is_detached(void) { return flash_detached; }

/* Analiza la tarjeta SD (LUN 1). Devuelve tamano en sectores o -1. */
int usb_sd_init(void) {
  sd_info_t *si;

  sd_present = false;
  sd_card_drv.init();
  if (sd_card_drv.reset() != sd_err_ok)
    return -1;
  si = sd_card_drv.getcardinfo();
  if (si == NULL)
    return -1;
  sd_block_count = si->card_size;
  sd_block_size  = si->sect_size;
  sd_present = true;
  return (int)sd_block_count;
}

/* Tamano de la SD en sectores (0 si no hay tarjeta). */
uint32_t usb_sd_size(void) {
  return sd_present ? sd_block_count : 0;
}

/* Dos LUNs: 0 = W25Q128, 1 = SD. GET MAX LUN devuelve n-1. */
uint8_t tud_msc_get_maxlun_cb(void) { return 2; }

/* Tamano de bloque del LUN dado (512 B en ambos) */
static uint16_t msc_lun_block_size(uint8_t lun) {
  uint16_t sz = (lun == LUN_FLASH) ? msc_block_size : sd_block_size;
  return (sz == 0) ? 512 : sz;   /* evita division por cero */
}

uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *r,
                             uint32_t bufsize) {
  (void)bufsize;
  const char *vendor, *product;
  const char rev[] = "1.0 ";

  if (lun == LUN_FLASH) {
    vendor  = "DANIEL  ";
    product = "FLASH XINU    ";
  } else {
    vendor  = "DANIEL  ";
    product = "SDCARD XINU   ";
  }
  r->peripheral_device_type = 0;
  r->peripheral_qualifier   = 0;
  r->is_removable           = 1;
  r->version                = 6;
  r->response_data_format   = 2;
  r->additional_length      = 0x1F;
  for (uint8_t i = 0; i < 8;  i++) r->vendor_id[i]  = (i < strlen(vendor))  ? vendor[i]  : ' ';
  for (uint8_t i = 0; i < 16; i++) r->product_id[i] = (i < strlen(product)) ? product[i] : ' ';
  for (uint8_t i = 0; i < 4;  i++) r->product_rev[i] = rev[i];
  return sizeof(scsi_inquiry_resp_t);
}

/* detached => "no medium": el host deja de tocar el LUN. */
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  if (lun == LUN_FLASH)
    return !flash_detached;
  return sd_present;
}

/* Capacidad 0 si la flash esta del lado del shell. */
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
  if (lun == LUN_FLASH) {
    if (flash_detached) {
      *block_count = 0;
      *block_size  = 0;
      return;
    }
    flash_info_t *info = flash_spi_getcardinfo();
    msc_block_count = (info) ? info->card_size : 0;
    msc_block_size  = (info) ? info->sect_size : 512;
  } else {
    msc_block_count = sd_present ? sd_block_count : 0;
    msc_block_size  = sd_block_size ? sd_block_size : 512;
  }
  *block_count = msc_block_count;
  *block_size  = msc_block_size;
}

/* START STOP UNIT con load_eject = 'eject' del host. Si Linux eyecta
 * la flash, la propiedad pasa automaticamente al shell. En la SD no
 * hay nada que hacer (solo la usa el host). */
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t pc, bool start,
                           bool load_eject) {
  (void)pc;

  if (lun == LUN_FLASH && load_eject && !start)
    flash_detached = true;   /* host eyecto: queda del lado de Xinu */

  return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
  (void)offset;
  uint32_t blocks;

  if (lun == LUN_FLASH) {
    if (flash_detached) return -1;   /* el host no puede tocarla */
    blocks = bufsize / msc_lun_block_size(lun);
    w25qxxx_drv.read(buffer, lba, blocks);
  } else {
    if (!sd_present) return -1;
    blocks = bufsize / msc_lun_block_size(lun);
    if (sd_card_drv.read(buffer, lba, blocks) != sd_err_ok) return -1;
  }
  return (int32_t)bufsize;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
  (void)offset;
  uint32_t blocks;

  if (lun == LUN_FLASH) {
    if (flash_detached) return -1;
    blocks = bufsize / msc_lun_block_size(lun);
    w25qxxx_drv.write(buffer, lba, blocks);
  } else {
    if (!sd_present) return -1;
    blocks = bufsize / msc_lun_block_size(lun);
    if (sd_card_drv.write(buffer, lba, blocks) != sd_err_ok) return -1;
  }
  return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
  if (lun == LUN_FLASH)
    return !flash_detached;
  return sd_present;
}

/* PREVENT ALLOW MEDIUM REMOVAL (0x5E): responder exito para que las
 * herramientas de montaje del host no se quejen. */
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const cmd[16],
                        void *buffer, uint16_t bufsize) {
  (void)lun; (void)buffer; (void)bufsize;

  if (cmd[0] == SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL)
    return 0;

  return -1;
}
#endif