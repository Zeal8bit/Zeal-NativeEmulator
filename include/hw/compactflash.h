#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <hw/device.h>

#if CONFIG_COMPACTFLASH_SIZE_KB
#define COMPACTFLASH_SIZE_KB CONFIG_COMPACTFLASH_SIZE_KB
#else
#define COMPACTFLASH_SIZE_KB 64*1024 // Default size is 64MB
#endif

typedef enum {
     IDE_STAT_BUSY = 7,
     IDE_STAT_RDY  = 6,
     IDE_STAT_DWF  = 5,
     IDE_STAT_DSC  = 4,
     IDE_STAT_DRQ  = 3,
     IDE_STAT_CORR = 2,
     IDE_STAT_IDX  = 1,
     IDE_STAT_ERR  = 0
} compactflash_status_t;

typedef enum {
    IDE_REG_DATA    = 0,
    IDE_REG_FEATURE = 1,
    IDE_REG_ERROR   = 1, // Same as feature
    IDE_REG_SEC_CNT = 2,
    IDE_REG_LBA_0   = 3,
    IDE_REG_LBA_8   = 4,
    IDE_REG_LBA_16  = 5, 
    IDE_REG_LBA_24  = 6,
    IDE_REG_COMMAND = 7,
    IDE_REG_STATUS  = 7 // Same as command
} compactflash_registers_t;

typedef enum {
    IDE_CMD_NOP             = 0x00,
    IDE_CMD_RECAL           = 0x10,
    IDE_CMD_READ_SECTOR     = 0x20,
    IDE_CMD_READ_SECTOR_NR  = 0x21,
    IDE_CMD_WRITE_SECTOR    = 0x30,
    IDE_CMD_WRITE_SECTOR_NR = 0x31,
    IDE_CMD_READ_BUFFER     = 0xE4,
    IDE_CMD_WRITE_BUFFER    = 0xE8,
    IDE_CMD_IDENTIFY        = 0xEC,
    IDE_CMD_SET_FEATURE     = 0xEF,
} compactflash_commands_t;

typedef enum {
    IDE_FEAT_ENABLE_8BIT = 0x01,
    IDE_FEAT_DISABLE_8BIT = 0x81,
} compactflash_feature_t;

typedef enum {
    IDE_ERR_AMNF  = 0, // Address Mark Not Found
    IDE_ERR_TKZNF = 1, // Track 0 Not Found
    IDE_ERR_ABRT  = 2, // Command Aborted
    IDE_ERR_MCR   = 3, // Media Change Request
    IDE_ERR_IDNF  = 4, // ID Not Found
    IDE_ERR_MC    = 5, // Media Change
    IDE_ERR_UNC   = 6, // Uncorrectable Data Error
    IDE_ERR_BBK   = 7, // Bad Block Detected
} compactflash_error_t;

typedef enum {
    IDE_DATA_IDLE = 0,
    IDE_CMD = 1,
    IDE_DATA_IN = 2,
    IDE_DATA_OUT = 3,
    IDE_DATA_ERROR = 4,
} compactflash_state_t;

typedef struct {
    // device_t
    device_t    parent;
    size_t      size; // in bytes

    // CompactFlash specific
    int fd;
    long data_ofs;
    uint8_t sector_buffer[512];
    uint16_t sector_buffer_idx;
    compactflash_state_t state;
    bool master, lba_mode;
    uint8_t status, sec_cnt, sec_cur, feature, error;
    uint8_t lba_0, lba_8, lba_16, lba_24;
    uint16_t identity[256];
} compactflash_t;

int compactflash_init(compactflash_t* compactflash, char *file_name);
