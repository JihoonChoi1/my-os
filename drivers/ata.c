#include "ata.h"
#include "ports.h"

// External for debugging (kernel.c)
extern void print_string(char* str);
extern void print_hex(uint32_t n);

void ata_wait_bsy() {
    while(port_byte_in(ATA_STATUS) & ATA_SR_BSY);
}

void ata_wait_drq() {
    while(!(port_byte_in(ATA_STATUS) & ATA_SR_DRQ));
}

// 400ns delay function (Status port read 4 times)
void ata_wait_400ns() {
    for(int i = 0; i < 4; i++) {
        port_byte_in(ATA_STATUS);
    }
}

// Read One Sector (512 Bytes) using LBA28
void ata_read_sector(uint32_t lba, uint8_t *buffer) {
    ata_wait_bsy();
    
    // Select Drive (Master) + LBA High 4 bits
    // 0xE0 = 11100000 (Mode=LBA, Drive=0)
    port_byte_out(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    
    ata_wait_400ns();

    // Sector Count = 1
    port_byte_out(ATA_SECTOR_CNT, 1);
    
    // Send LBA Address (Low -> Mid -> High)
    port_byte_out(ATA_LBA_LO, (uint8_t)(lba));
    port_byte_out(ATA_LBA_MID, (uint8_t)(lba >> 8));
    port_byte_out(ATA_LBA_HI, (uint8_t)(lba >> 16));
    
    // Send Command (Read PIO)
    port_byte_out(ATA_COMMAND, ATA_CMD_READ_PIO);

    ata_wait_400ns();
    
    ata_wait_bsy();
    ata_wait_drq();
    
    // Read Data (256 Words = 512 Bytes)
    for (int i = 0; i < 256; i++) {
        uint16_t data = port_word_in(ATA_DATA);
        buffer[i * 2] = (uint8_t)(data & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)((data >> 8) & 0xFF);
    }
}
