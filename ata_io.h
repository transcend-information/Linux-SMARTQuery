#ifndef SG_SCSI_RESET
#define SG_SCSI_RESET =0x2284
#endif

#ifndef SG_SCSI_RESET_NOTHING
#define SG_SCSI_RESET_NOTHING 0
#define SG_SCSI_RESET_DEVICE  1

#define SG_SCSI_RESET_BUS     2
#define SG_SCSI_RESET_HOST    3
#endif

#define BYTES_PER_SECTOR     512

/*ATA command*/

#define IDENTIFY_DEVICE     0xEC

/*ATA set feature command*/
#define SET_FEATURES        0xEF
/*sub command*/
#define ENABLE_WRITE_CACHE  0x02
#define DISABLE_WRITE_CACHE 0x82
#define SET_TRANSFER_MODE   0x03
#define LOOK_AHEAD_ENABLE   0xAA
#define LOOK_AHEAD_DISABLE  0x55

#define SET_MULTIPLE_MODE   0xC6

/*ATA security command*/
/*PIO-data-out*/
#define SET_PASSWORD        0xF1
#define DISABLE_PASSWORD    0xF6
#define UNLOCK              0xF2
#define ERASE               0xF4 /*execute ERASE_PREPARE before ERASE*/
/*no-data*/
#define ERASE_PREPARE       0xF3
#define FREEZE_LOCK         0xF5

/*ATA R/W command*/
/*PIO-DATA_IN*/
#define READ_BUFFER         0xE4
#define READ_MULTIPLE       0xC4
#define READ_MULTIPLE_EXT   0x29
#define READ_SECTOR         0x20
#define READ_SECTOR_EXT     0x24
/*PIO-DATA_OUT*/
#define WRITE_BUFFER        0xE8
#define WRITE_MULTIPLE      0xC5
#define WRITE_MULTIPLE_EXT  0x39
#define WRITE_SECTOR        0x30
#define WRITE_SECTOR_EXT    0x34

/*PIO-NO_DATA*/
#define READ_VERIFY_SECTOR   0x40 
#define READ_VERIFY_SECTOR_EXT 0x42

/*DMA*/
#define READ_DMA            0xC8
#define READ_DMA_EXT        0x25
#define WRITE_DMA           0xCA
#define WRITE_DMA_EXT       0x35

#define DATA_SET_MANAGEMENT 0x06

/*DMA queue*/
#define READ_DMA_QUEUE      0xC7
#define READ_DMA_QUEUE_EXT  0x26
#define WRITE_DMA_QUEUE     0xCC 
#define WRITE_DMA_QUEUE_EXT 0x36


/*ATA power management command*/
/*no-data*/
#define IDLE               0xE3
#define IDLE_IMMEDIATE     0xE1
#define SLEEP              0xE6
#define STANDBY            0xE2
#define STANBY_IMMEDIATE   0xE0
#define CHECK_POWER_MODE   0xE5


/*ATA SMART command*/
#define SMART              0xB0
/*SMART feature register values*/
#define SMART_READ_DATA    0xD0
#define SMART_READ_LOG     0xD5
#define SMART_WRITE_LOG    0xD6
#define SMART_ENABLE_OPS   0xD8
#define SMART_DISABLE_OPS  0xD9
#define SMART_RET_STATUS   0xDA

/*ATA flush cache command*/
#define FLUSH_CACHE        0xE7
#define FLUSH_CACHE_EXT    0xEA

/*ATA read/set MAX addr command*/
#define READ_NATIVE_MAX_ADDR       0xF8
#define READ_NATIVE_MAX_ADDR_EXT   0x27
#define SET_MAX_ADDR               0xF9
#define SET_MAX_ADDR_EXT           0x37
/*sub-command*/
#define SET_MAX_FREEZE_LOCK        0x04
#define SET_MAX_LOCK               0x02
#define SET_MAX_SET_PASSWORD       0x01
#define SET_MAX_UNLOCK             0x03

/*ATA device configuration*/
#define DEVICE_CONFIG              0xB1
/*subcommand*/
#define CONFIG_SET                 0xC3
#define CONFIG_IDENTIFY            0xC2
#define CONFIG_FREEZE              0xC1
#define CONFIG_RESTORE             0xC0

/*sub-command*/
#define CONFIG_FREEZE_LOCK         0xC1
#define CONFIG_IDENTIFY            0xC2
#define CONFIG_RESTORE             0xC0
#define CONFIG_SET                 0xC3

/*SG_IO Protocol
 *defined accroding to SAT
*/
#define HARDWARE_RESET   0
#define SOFTWARE_RESET   1
#define NON_DATA         3
#define PIO_DATA_IN      4
#define PIO_DATA_OUT     5
#define DMA              6
#define DMA_QUEUED       7
#define EXE_DEV_DIAG     8
#define DEVICE_RESET     9

/*******************************************/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <scsi/sg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
//#include <getopt.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <scsi/sg.h>
#include <linux/hdreg.h>

/*****************************************************************************/

typedef struct word_{
	
	unsigned char low;
	unsigned char high;
	
}word;
/*****************************************************************************/

/*device struct*/
typedef struct security_status_{ 
     
      /* from ATA command: Identify device, word128 */
      unsigned char level;
      unsigned char frozen;
      unsigned char locked;
      unsigned char enabled;

} security_status;

/***********************************************************************/

typedef struct device_config_{

     word word0;
     word mdma;
     word udma;
     word max_lba[4];
     word command_set; 
     word sata[2];
     word reserved[245];
     word chechsum;

} device_config;

/*****************************************************************************/

typedef struct err_ret_{

     unsigned char error;
     unsigned char count[2];
     unsigned char lba_low[2];
     unsigned char lba_mid[2];
     unsigned char lba_high[2];
     unsigned char device;
     unsigned char status;

}err_ret;

/************************************************************************************/

typedef struct idnetify_info_{

     word word_num[256];
     

}idnetify_info;
/************************************************************************************/

typedef struct lba_range_{

     unsigned char lba_addr[6];
     unsigned char lba_count[2];

}lba_range;

typedef struct lba_range_entry_{

     lba_range entry[64];

}lba_range_entry;

/************************************************************************************/

/*basic ATA_IO function*/
/*DATA_IN */
/* input: device_num, ATA_command, feature, lba, sec_count, Buffer, return error value */
int ata_data_in(int , unsigned char , unsigned short , 
                     unsigned long long, unsigned short , unsigned char *, err_ret* );

/*DATA_OUT*/
/* input: device_num, ATA_command, feature, lba, sec_count, Buffer */
int ata_data_out(int , unsigned char , unsigned short , 
                     unsigned long long, unsigned short , unsigned char *, err_ret* );

/*No_DATA*/
/* input: device_num, ATA_command, feature, lba, sec_count */
int ata_no_data(int , unsigned char, unsigned short, 
                                  unsigned long long, unsigned short, err_ret* );

/*ATA command using ATA_IO function*/
int identify_device(int);

int get_security_status(int, security_status*);

unsigned long long get_lba(int);
unsigned long long get_lba2(int);
/*SMART funciton*/
int get_smart_data(int, char*);
int smart_enable(int);
int smart_disable(int);

/*ATA read write command*/
int read_mbr(int, unsigned char*);
int print_lba(int ,unsigned long long);
int read_par(int);

int write_mbr(int, unsigned char*);
int clear_mbr(int);

int full_read(int);/*return 0: read without error*/
int full_read_write(int ,int,  unsigned char); /*read write without fail*/

int read_multiple_test (int, unsigned long long, unsigned char);
int write_multiple_test (int, unsigned long long, unsigned char);
int read_dma_test (int, unsigned long long, unsigned char);
int write_dma_test (int, unsigned long long, unsigned char);


/*ATA security command*/
int security_set_password(int ,unsigned char, unsigned char, char*);
int security_unlock(int, unsigned char, char* );
int security_erase(int, unsigned char, char*);
int security_disable_password(int , unsigned char, char*);
int security_frozen(int);
int security_show_status(int);

/*ATA set feature command*/
int write_cache_set(int, unsigned char );
int set_transfer_mode(int, char*, unsigned char);
int look_ahead_set(int, unsigned char);

/*ATA power management command*/
int check_pow_mode(int, unsigned char*);
int pow_standby(int, unsigned char);
int pow_standy_immediate(int);
int pow_idle(int, unsigned char);
int pow_idle_immediate(int);
int pow_sleep(int);

/*ATA Device Configuration command*/
int dev_config_freeze(int sg_fd);
int dev_config_identify(int, device_config*);
int dev_config_restore(int);
int dev_config_set(int, device_config*);
void print_device_config(device_config*);

/*ATA set MAX command*/
int set_max_addr_ext(int, unsigned long long, unsigned char);


/*set multiple mode*/
int set_multiple_mode(int, unsigned char);

int trim(int, unsigned char, lba_range_entry*);
