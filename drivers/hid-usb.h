
#include <usb.h> /* libusb */

/* try to open the next available device matching the given MatchFlags
 * (in case mode=MODE_OPEN), or try to open the exact same device as
 * before (in case mode=MODE_REOPEN). */
extern int libusb_open(HIDDevice *curDevice, MatchFlags_t *flg, unsigned char *ReportDesc, int mode);
void libusb_close(void);

extern usb_dev_handle *udev;

//extern int usb_get_descriptor(int type, int len, char *report);
extern int libusb_get_report(int ReportId, unsigned char *raw_buf, int ReportSize );

extern int libusb_set_report(int ReportId, unsigned char *raw_buf, int ReportSize );
extern int libusb_get_string(int StringIdx, char *string);
extern int libusb_get_interrupt(unsigned char *buf, int bufsize, int timeout);
