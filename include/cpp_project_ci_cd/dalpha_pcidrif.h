#ifndef _PCIDRV_IFACE_H_
#define _PCIDRV_IFACE_H_

/*
 * Open & close the PCI I/F APIs.
 */
extern int32_t pcidrif_open(void);
extern void pcidrif_close(void);

/* 
 * Write to/from PCIe EP Devices Addresses. 
 */
extern int32_t pcidr_write(uint32_t bar, uint64_t epaddr, uint64_t bytecnts, void *wrbuf);
extern int32_t pcidr_read(uint32_t bar, uint64_t epaddr, uint64_t bytecnts, void *rdbuf);
#endif		//_PCIDRV_IFACE_H_
