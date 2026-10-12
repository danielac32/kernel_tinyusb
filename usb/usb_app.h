/* usb_app.h - TinyUSB device app: CDC (console) + MSC (W25Q128) */

#ifndef USB_APP_H
#define USB_APP_H

#include <stdint.h>

uint16_t usb_available(void);
int      usb_getc(void);
int      usb_cdc_connected(void);
int      usb_sd_init(void);
uint32_t usb_sd_size(void);
int      usbtask(void);

#endif /* USB_APP_H */