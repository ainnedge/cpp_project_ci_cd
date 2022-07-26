#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/*
 * Device nodes formed for PCIe cards.
 */
#define DEVICE_CARD0_CHHC       "/dev/alpha_card0_baroffst"
#define DEVICE_CARD0_H2C2H      "/dev/alpha_card0_viadma"


#define DALPHA_DMA_BAR			2
#define MAX_PAYLOAD_UNIT		(65536)		/* PCIe bus one time pay load bytes size from user application. */
#define MIN_PAYLOAD_UNIT		(4)			/* Minimum number of bytes in one PCIe transaction. */


#define	PCI_CARD_NUM			(4)		/* Total supported PCIe cards */
#define	PCI_ADDR_BAR_NUM		(5)		/* Max BAR numbers */
#define	PCI_ADDR_ALIGN			(0x3)	/* 4 Bytes allignmnet for EP Address */

/*
 * PCIe Non-DMA BAR management.
 */
enum rbar {VUC118_RDBAR0, VUC118_RDBAR1, VUC118_RDBAR2, VUC118_RDBAR3, VUC118_RDBAR4, VUC118_RDBAR5 };
enum wbar {VUC118_WRBAR0, VUC118_WRBAR1, VUC118_WRBAR2, VUC118_WRBAR3, VUC118_WRBAR4, VUC118_WRBAR5 };
enum octalcmd{RD_BR0_OCTL = 10, RD_BR1_OCTL, RD_BR2_OCTL, RD_BR3_OCTL, RD_BR4_OCTL, RD_BR5_OCTL,
	WR_BR0_OCTL, WR_BR1_OCTL, WR_BR2_OCTL, WR_BR3_OCTL, WR_BR4_OCTL, WR_BR5_OCTL
};

/* 
 * Multiple Card Access devices node.
 */
#define	BASE_ID			100				/* PCIe EP Number on bus */
int32_t barfd[4][2] = {{-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}};
char* danodes[4][2] = {
				{DEVICE_CARD0_CHHC, DEVICE_CARD0_H2C2H},
				{"", ""},
				{"", ""},
				{"", ""}
			};

/*
 * PCIe transaction Interface APIs prototypes.
 */
int32_t pcidr_init(uint32_t);
int32_t pcidrif_open(void);
void pcidrif_close(void);
int32_t pcidr_write(uint32_t bar, uint64_t memaddr, uint64_t bytecnts, void* wbuf);
int32_t pcidr_read(uint32_t bar, uint64_t memaddr, uint64_t bytecnts, void* rbuf);
int32_t host_todev_viadma(uint64_t ep_addr, uint8_t* wrbuf, uint64_t size, char* filename);
int32_t devto_host_viadma(uint64_t ep_addr, uint8_t* rdbuf, uint64_t size, char* filename);

/*
 * Description:
 * Open the PCIe Interface before performing R/W.
 * Argument: Card Index.
 * Return Value:
 * !0 When Failed, 0 on Success.
 */
int32_t pcidrif_open(void)
{
	int32_t ret;
	uint32_t card = 0x00;

	/*
	 * Configure the interface for R/W operation.
	 */
	ret = pcidr_init(card % BASE_ID);

	printf("PCIe interface for Card#%d is opened...\n", card);

	return ret;
}

/*
 * Description:
 * Close the PCIe Interface before shuting down the EP operations.
 * Argument: Card Index.
 * Return Value: None
 */
void pcidrif_close(void)
{
	uint32_t card = 0x00;
	
	card = card % BASE_ID;
	close(barfd[card][0]);
	close(barfd[card][1]);

	printf("PCIe interface for Card#%d is closed...\n", card);

	return;
}

/*
 * Description:
 * Initialized PCIe opened interfcae.
 * Argument: Card Index.
 * Return Value:
 * !0 When Failed, 0 on Success.
 */
int32_t pcidr_init( uint32_t card)
{
	barfd[card][0] = open(danodes[card][0], O_RDWR, 0777);
	if (barfd[card][0] < 0) {
		perror("Error in opening pci BAR interface!!!");
		exit(0);
	}

	barfd[card][1] = open(danodes[card][1], O_RDWR, 0777);
	if (barfd[card][1] < 0) {
		perror("Error in opening pci DMA interface!!!");
		exit(0);
	}

	// printf("PCIe interface(Card#%d) is opened...\n", card);

	return 0;
}

/*
 * Description:
 * Perform PCIe Write operation on memaddr for byte counts.
 * Argument: Card Index, BAR Number, EP Memory Address, Byte counts, Write buffer pointer.
 * Return Value:
 * !0 When Failed, 0 on Success.
 */
int32_t pcidr_write(uint32_t bar, uint64_t memaddr, uint64_t bytecnts, void* wrbuf)
{
	uint8_t* wbuf = (uint8_t*)wrbuf;
	int64_t ret = 0x00;
	uint64_t rmndr;
	uint64_t i, units;
	uint32_t card = 0x00;

	/*
	 * Get the card index.
	 */
	card = card % BASE_ID;

	/*
	 * Validates the received parameters.
	 */
	if((card < 0 || card > PCI_CARD_NUM) || (memaddr & PCI_ADDR_ALIGN) || (bar < 0 || bar > PCI_ADDR_BAR_NUM)) {
		printf("Please check the PCI write inputs!!!\n");
		exit(0);
	}
	
	/*
	 * Validate the PCIe Write Address for Non DMA BAR.
	 */
	if(bar != DALPHA_DMA_BAR) {
		ret = ioctl(barfd[card][0], (RD_BR0_OCTL + 0x06 + bar), memaddr);
		if(ret){
			printf("Error!!! ioctl( WR @ BAR(%d)) = %ld\n", bar, ret);
			exit(0);
		}
	}

	/*
	 * Validate the PCIe Write Address for DMA BAR.
	 */
	if(bar == DALPHA_DMA_BAR) {
		//printf("Accessing DMA BAR(%d) for Write:\n", bar); 
		units = bytecnts / MAX_PAYLOAD_UNIT;
		rmndr = bytecnts % MAX_PAYLOAD_UNIT;

		//printf("units = %d, rmndr = %d\n", units, rmndr);

		for(i = 0 ; i < units; i++) {
			ret += host_todev_viadma(memaddr, wbuf, MAX_PAYLOAD_UNIT, 0);
			wbuf += MAX_PAYLOAD_UNIT;
			memaddr += MAX_PAYLOAD_UNIT;
		}

		if(rmndr) {
			ret += host_todev_viadma(memaddr, wbuf, rmndr, 0);
		}
	}
	else {
		//printf("\nAccessing OFFST BAR(%d) For write:\n", bar);
		ret = write (barfd[card][0], wbuf, bytecnts);
	}

	/*
	 * Check for if write error.
	 */
	if(ret != bytecnts) {
		if(units){
			wbuf -= MAX_PAYLOAD_UNIT;
			memaddr -= MAX_PAYLOAD_UNIT;
		}
		printf("bar = %d, written bytes= %lu, requested bytes = %lu, Write Address = 0x%lX, Write buff = %p\n", bar, ret, bytecnts, memaddr, wrbuf);
		perror(" Write Status at application side");
		return ret;
	}

	return 0;
}

/*
 * Description:
 * Perform PCIe Read operation on memaddr for byte counts.
 * Argument: Card Index, BAR Number, EP Memory Address, Byte counts, Read buffer pointer.
 * Return Value:
 * !0 When Failed, 0 on Success.
 */
int32_t pcidr_read(uint32_t bar, uint64_t memaddr, uint64_t bytecnts, void* rdbuf)
{
	uint8_t* rbuf = (uint8_t*)rdbuf;
	int64_t ret = 0x00;
	uint64_t rmndr;
	uint64_t i, units;
	uint32_t card = 0x00;
		
	/*
	 * Get the card index.
	 */
	card = card % BASE_ID;

	/*
	 * Validates the received parameters.
	 */
	if((card < 0 || card > PCI_CARD_NUM) || (memaddr & PCI_ADDR_ALIGN) || (bar < 0 || bar > PCI_ADDR_BAR_NUM)) {
		printf("Please check the PCI read inputs!!!\n");
		exit(0);
	}

	/*
	 * Validate the PCIe Read Address for Non DMA BAR.
	 */
	if(bar != DALPHA_DMA_BAR) {
		ret = ioctl(barfd[card][0], (RD_BR0_OCTL + 0x00 + bar), memaddr);
		if(ret){
			printf("Error!!! ioctl( RD @ BAR(%d)) = %ld\n", bar, ret);
			exit(0);
		}
	}

	if(bar == DALPHA_DMA_BAR) {
		//printf("Accessing DMA BAR(%d) for Read:\n", bar); 
		units = bytecnts / MAX_PAYLOAD_UNIT;
		rmndr = bytecnts % MAX_PAYLOAD_UNIT;

		//printf("units = %d, rmndr = %d\n", units, rmndr);

		for(i = 0 ; i < units; i++) {
			ret += devto_host_viadma(memaddr, rbuf, MAX_PAYLOAD_UNIT, 0);
			rbuf += MAX_PAYLOAD_UNIT;
			memaddr += MAX_PAYLOAD_UNIT;
		}

		if(rmndr) {
			ret += devto_host_viadma(memaddr, rbuf, rmndr, 0);
		}
	}
	else {
		//printf("Accessing OFFST BAR(%d) for Read:\n", bar);
		ret = read(barfd[card][0], rbuf, bytecnts);
	}

	/*
	 * Check for if read error.
	 */
	if (ret != bytecnts) {
		if(units){
			rbuf -= MAX_PAYLOAD_UNIT;
			memaddr -= MAX_PAYLOAD_UNIT;
		}
		printf("bar = %d, Read bytes= %ld, requested bytes = %lu, Read Address = 0x%lX, Read buff = %p\n", bar, ret, bytecnts, memaddr, rdbuf);
		perror(" Read Status at application side");
		return ret;
	}

	return 0;
}

/*
 * Description:
 * Perform PCIe Write operation on memaddr for byte counts via DMA.
 * Argument: Card Index, EP Memory Address, Write buffer pointer, Byte counts.
 * Return Value:
 * 0 When Failed, Written byte counts on Success.
 */
int32_t host_todev_viadma(uint64_t addr, uint8_t* wrbuf, uint64_t size, char* filename)
{
	int32_t rc;
	uint32_t card = 0x00;	

	//printf(" card = %d, addr = %lX, size = %lu \n", card, addr, size);
/*
 * When attempting to transfer file rather than buffer copy.
 */
#ifdef DALPHA_DATA_FILE_ACCESS
	char *buffer = NULL;
	char *allocated = NULL;
	int32_t dalpha_fd = -1;


	posix_memalign((void **)&allocated, 4096/*alignment*/, size + 4096);
	assert(allocated);
	buffer = allocated;

	/* Bin File from Dalpha */
	if (filename) {
		dalpha_fd = open(filename, O_RDONLY);
		assert(dalpha_fd >= 0);
	}
	/* Fill the buffer with data from file */
	if (dalpha_fd >= 0) {
		rc = read(dalpha_fd, buffer, size);
		if (rc != size) perror("read(dalpha_fd)");
		assert(rc == size);
	}
#endif

	/*
	 * Select & Configure AXI MM EP Address.
	 */
	off_t off = lseek(barfd[card][1], addr, SEEK_SET);

	if(off < 0) {
		printf("AXI Memory Mapped Failed!!!\n");
		return 0;
	}

	/*
	 * Write buffer contents to AXI MM using SGDMA.
	 */
	rc = write(barfd[card][1], wrbuf, size);
	if(rc != size) {
		printf("bar = %d, rc= %d, bytecnts = %lu, memaddr = 0x%lX, wrbuf = %p\n", 1, rc, size, addr, wrbuf);
	}

	/* WARN ON */
	assert(rc == size);

#ifdef DALPHA_DATA_FILE_ACCESS
	if (dalpha_fd >= 0) {
		close(dalpha_fd);
	}

	free(allocated);
#endif

	return rc;
}

/*
 * Description:
 * Perform PCIe Read operation on memaddr for byte counts via DMA.
 * Argument: Card Index, EP Memory Address, Read buffer pointer, Byte counts.
 * Return Value:
 * 0 When Failed, Read byte counts on Success.
 */
int32_t devto_host_viadma(uint64_t addr, uint8_t* rdbuf, uint64_t size, char* filename)
{
	int32_t rc;
	uint32_t card = 0x00;	
	
	//printf(" card = %d, addr = %lX, size = %lu \n", card, addr, size);
/*
 * When attempting to transfer read contents into file rather than buffer copy.
 */
#ifdef DALPHA_DATA_FILE_ACCESS
	int32_t dalpha_fd = -1;
	char *buffer = NULL;
	char *allocated = NULL;

	posix_memalign((void **)&allocated, 4096/*alignment*/, size + 4096);
	assert(allocated);
	buffer = allocated;

	/* Bin File from Dalpha */
	if (filename) {
		dalpha_fd = open(filename, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666);
		assert(dalpha_fd >= 0);
	}

	memset(buffer, 0x00, size);
#endif

	/*
	 * Select & Configure AXI MM EP Address.
	 */
	off_t off = lseek(barfd[card][1], addr, SEEK_SET);
	if(off < 0) {
		printf("AXI Memory Mapped Failed!!!\n");
		return 0;
	}
	/*
	 * Read data from AXI EP via SGDMA
	 */
	rc = read(barfd[card][1], rdbuf, size);
	if ((rc > 0) && (rc < size)) {
		printf("Short read of %d bytes into a %lu bytes buffer!!!\n", rc, size);
	}

#ifdef DALPHA_DATA_FILE_ACCESS
	if ((dalpha_fd >= 0) ) {
		/* Write buffer back to file */
		rc = write(dalpha_fd, buffer, size);
		if(rc != size) {
			printf("bar = %d, rc = %d, bytecnts = %lu, memaddr = 0x%lX, rdbuff = %p\n", 1, rc, size, addr, rdbuf);
		}
		assert(rc == size);
	}

	if (dalpha_fd >=0) {
		close(dalpha_fd);
	}

	free(allocated);
#endif

	return rc;
}

																		/* EOF */
