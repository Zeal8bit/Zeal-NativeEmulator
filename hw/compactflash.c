#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
/* Required for file operations */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "hw/compactflash.h"
#include "utils/paths.h"

static uint8_t compactflash_read_data(compactflash_t* cf);
static void compactflash_write_data(compactflash_t* cf, uint8_t value);
static void compactflash_process_command(compactflash_t* cf, uint8_t cmd);

static uint16_t le16(uint16_t v)
{
        uint8_t *p = (uint8_t *)&v;
        return p[0] | (p[1] << 8);
}

static uint8_t io_read(device_t* dev, uint32_t addr)
{
    compactflash_t* cf = (compactflash_t*) dev;
    if (!cf->master) return 0;
    switch (addr) {
        case IDE_REG_STATUS:  return cf->status; break;
        case IDE_REG_ERROR:   return cf->error; break;
        case IDE_REG_SEC_CNT: return cf->sec_cnt; break;
        case IDE_REG_LBA_0:   return cf->lba_0; break;
        case IDE_REG_LBA_8:   return cf->lba_8; break; 
        case IDE_REG_LBA_16:  return cf->lba_16; break;
        case IDE_REG_LBA_24:  return cf->lba_24; break;
        case IDE_REG_DATA:    return compactflash_read_data(cf); break;
        default:              return 0;
    }
}

static void io_write(device_t* dev, uint32_t addr, uint8_t value)
{
    compactflash_t* cf = (compactflash_t*) dev;
    switch (addr) {
        case IDE_REG_DATA:    compactflash_write_data(cf, value); break;
        case IDE_REG_COMMAND: compactflash_process_command(cf, value); break;
        case IDE_REG_FEATURE: cf->feature = value; break;
        case IDE_REG_SEC_CNT: cf->sec_cnt = value; break;
        case IDE_REG_LBA_0:   cf->lba_0 = value; break;
        case IDE_REG_LBA_8:   cf->lba_8 = value; break;
        case IDE_REG_LBA_16:  cf->lba_16 = value; break;
        case IDE_REG_LBA_24:
            cf->lba_24 = value; // we'll filter upper bits later
            cf->master = (value & 0x10) == 0;   // Bit 4 indicates master/slave
            cf->lba_mode = (value & 0x40) != 0; // Bit 6 indicates LBA mode
            break;
        default:
            printf("[COMPACTFLASH] Unsupported write, reg: 0x%02x, data: 0x%02x\n", addr, value);
            break;
        }
}

static uint32_t compactflash_data_ofs(compactflash_t* cf)
{
    if (!cf->master) {
        printf("[COMPACTFLASH] Slave device does not support data access.\n");
        return -1;
    }
    if (!cf->lba_mode) {
        printf("[COMPACTFLASH] CHS mode not supported.\n");
        return -1;
    }
    uint32_t sector = ((cf->lba_24 & 0xF) << 24) | (cf->lba_16 << 16)
        | (cf->lba_8 << 8) | (cf->lba_0 << 0);
    if (sector+cf->sec_cnt >= COMPACTFLASH_SIZE_KB * 2) {
        printf("[COMPACTFLASH] Sector out of bounds: %u, cnt %u\n", sector, cf->sec_cnt);
        return -1;
    }
    return sector*512;
}

static void data_state(compactflash_t* cf, compactflash_state_t state)
{
    cf->state = state;
    switch (state) {
        case IDE_DATA_IDLE:
        case IDE_CMD:
            cf->status &= ~(1 << IDE_STAT_DRQ);
            break;
        case IDE_DATA_IN:
        case IDE_DATA_OUT:
            cf->status |= (1 << IDE_STAT_DRQ);
            cf->sector_buffer_idx = 0;
            cf->sec_cur = 0;
            break;
        case IDE_DATA_ERROR:
            cf->status &= ~(1 << IDE_STAT_DRQ);
            cf->status |= (1 << IDE_STAT_ERR);
            cf->error = (1 << IDE_ERR_IDNF);
            break;
    }
}

static uint8_t compactflash_read_data(compactflash_t* cf)
{
    if (cf->state != IDE_DATA_IN)
        return 0;
    uint8_t data = cf->sector_buffer[cf->sector_buffer_idx];
    if (++cf->sector_buffer_idx == 512) {
        if (cf->sec_cnt == 0)
            cf->sec_cnt = 0xff;
        if (++cf->sec_cur < cf->sec_cnt) {
            cf->data_ofs += 512;
            cf->sector_buffer_idx = 0;
            int bytes_read = read(cf->fd, cf->sector_buffer, 512);
            assert(bytes_read != -1);
            if (bytes_read < 512)
                memset(cf->sector_buffer + bytes_read, 0, 512 - bytes_read);
        } else
            data_state(cf, IDE_DATA_IDLE);
    }
    return data;
}

static void compactflash_write_data(compactflash_t* cf, uint8_t value)
{
    if (cf->state != IDE_DATA_OUT)
        return;
    cf->sector_buffer[cf->sector_buffer_idx] = value;
    if (++cf->sector_buffer_idx == 512) {
        assert(write(cf->fd, cf->sector_buffer, 512) != -1);
        if (cf->sec_cnt == 0)
            cf->sec_cnt = 0xff;
        if (++cf->sec_cur < cf->sec_cnt) {
            cf->data_ofs += 512;
            cf->sector_buffer_idx = 0;
        } else
            data_state(cf, IDE_DATA_IDLE);
    }
}

static void compactflash_process_command(compactflash_t* cf, uint8_t cmd)
{
    cf->status &= ~(1 << IDE_STAT_ERR);
    cf->error = 0;
    cf->state = IDE_CMD;

    switch (cmd) {
        case IDE_CMD_NOP:
        case IDE_CMD_SET_FEATURE:
            break;

        case IDE_CMD_IDENTIFY:
            memcpy(cf->sector_buffer, cf->identity, 512);
            data_state(cf, IDE_DATA_IN);
            break;

        case IDE_CMD_READ_SECTOR:
        case IDE_CMD_READ_SECTOR_NR:
            cf->data_ofs = compactflash_data_ofs(cf);
 
        // Fall-through
        case IDE_CMD_READ_BUFFER:
            if (cf->data_ofs == -1)
                return data_state(cf, IDE_DATA_ERROR);
            assert(lseek(cf->fd, cf->data_ofs, SEEK_SET) >= 0);
            int bytes_read = read(cf->fd, cf->sector_buffer, 512);
            assert(bytes_read != -1);
            if (bytes_read < 512)
                memset(cf->sector_buffer + bytes_read, 0, 512 - bytes_read);
            data_state(cf, IDE_DATA_IN);
            break;

        case IDE_CMD_WRITE_SECTOR:
        case IDE_CMD_WRITE_SECTOR_NR:
            cf->data_ofs = compactflash_data_ofs(cf);

        // Fall-through
        case IDE_CMD_WRITE_BUFFER:
            if (cf->data_ofs == -1)
                return data_state(cf, IDE_DATA_ERROR);
            assert(lseek(cf->fd, cf->data_ofs, SEEK_SET) >= 0);
            data_state(cf, IDE_DATA_OUT);
            break;
    }
}

int compactflash_init(compactflash_t* cf, char *file_name)
{
    cf->fd = open(file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    assert(cf->fd != -1);

    memset(cf->sector_buffer, 0x00, sizeof(cf->sector_buffer));
    cf->data_ofs = 0;
    cf->sector_buffer_idx = 0;
    cf->status = (1 << IDE_STAT_RDY) | (1<<IDE_STAT_DSC);
    cf->lba_0 = cf->lba_8 = cf->lba_16 = 0;
    cf->lba_24 = 0xE0;
    cf->sec_cnt = 1;
    cf->sec_cur = 0;
    cf->error = 0;
    cf->feature = 0;
    cf->master = true;
    data_state(cf, IDE_DATA_IDLE);
    
    // Simplified identify data according to the CompactFlash spec, so no CHS
    uint32_t total_sectors = COMPACTFLASH_SIZE_KB * 2;
    memset(cf->identity, 0, sizeof(cf->identity));
    cf->identity[0] = le16(0x848A);                  // CFA magic value
    cf->identity[47] = 0;                            // READ/WRITE MULTIPLE not implemented
    cf->identity[49] = le16(1 << 9);                 // LBA supported
    cf->identity[60] = le16(total_sectors & 0xFFFF); // LBA: current capacity in sectors
    cf->identity[61] = le16(total_sectors >> 16);    // LBA: current capacity in sectors
    cf->identity[83] = le16(1 << 2);                 // CFA Feature Set bit

    cf->size = 8;
    device_init_io(DEVICE(cf), "compactflash_dev", io_read, io_write, cf->size);
    return 0;
}
