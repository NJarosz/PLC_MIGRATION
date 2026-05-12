#include "mfrc522.h"
#include "main.h"
#include "spi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ── Register map ───────────────────────────────────────────────────────────
#define REG_COMMAND      0x01
#define REG_COM_IRQ      0x04
#define REG_DIV_IRQ      0x05
#define REG_ERROR        0x06
#define REG_STATUS2      0x08
#define REG_FIFO_DATA    0x09
#define REG_FIFO_LEVEL   0x0A
#define REG_CONTROL      0x0C
#define REG_BIT_FRAMING  0x0D
#define REG_COLL         0x0E
#define REG_MODE         0x11
#define REG_TX_MODE      0x12
#define REG_RX_MODE      0x13
#define REG_TX_CONTROL   0x14
#define REG_TX_ASK       0x15
#define REG_CRC_RESULT_H 0x21
#define REG_CRC_RESULT_L 0x22
#define REG_MOD_WIDTH    0x24
#define REG_T_MODE       0x2A
#define REG_T_PRESCALER  0x2B
#define REG_T_RELOAD_H   0x2C
#define REG_T_RELOAD_L   0x2D

// ── Chip commands ──────────────────────────────────────────────────────────
#define CMD_IDLE         0x00
#define CMD_CALC_CRC     0x03
#define CMD_TRANSCEIVE   0x0C
#define CMD_MF_AUTHENT   0x0E
#define CMD_SOFT_RESET   0x0F

// ── PICC / MIFARE commands ─────────────────────────────────────────────────
#define PICC_REQA        0x26
#define PICC_HLTA        0x50
#define PICC_SEL_CL1     0x93
#define PICC_MF_AUTH_A   0x60
#define PICC_MF_READ     0x30

static const uint8_t KEY_A_DEFAULT[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ── SPI helpers ────────────────────────────────────────────────────────────
static inline void cs_low(void)  { HAL_GPIO_WritePin(RFID_CS_Port, RFID_CS_Pin, GPIO_PIN_RESET); }
static inline void cs_high(void) { HAL_GPIO_WritePin(RFID_CS_Port, RFID_CS_Pin, GPIO_PIN_SET);   }

static void write_reg(uint8_t reg, uint8_t val) {
    uint8_t addr = (reg << 1) & 0x7E;
    cs_low();
    HAL_SPI_Transmit(&hspi2, &addr, 1, 1);
    HAL_SPI_Transmit(&hspi2, &val,  1, 1);
    cs_high();
}

static uint8_t read_reg(uint8_t reg) {
    uint8_t addr = ((reg << 1) & 0x7E) | 0x80;
    uint8_t val  = 0;
    cs_low();
    HAL_SPI_Transmit(&hspi2, &addr, 1, 1);
    HAL_SPI_Receive (&hspi2, &val,  1, 1);
    cs_high();
    return val;
}

static void set_bits(uint8_t reg, uint8_t mask) { write_reg(reg, read_reg(reg) |  mask); }
static void clr_bits(uint8_t reg, uint8_t mask) { write_reg(reg, read_reg(reg) & ~mask); }

// ── CRC via chip ───────────────────────────────────────────────────────────
static MFRC522_Status_t calc_crc(uint8_t *data, uint8_t len, uint8_t *out) {
    write_reg(REG_COMMAND, CMD_IDLE);
    clr_bits(REG_DIV_IRQ, 0x04);
    set_bits(REG_FIFO_LEVEL, 0x80);
    for (uint8_t i = 0; i < len; i++) write_reg(REG_FIFO_DATA, data[i]);
    write_reg(REG_COMMAND, CMD_CALC_CRC);

    uint32_t deadline = HAL_GetTick() + 20;
    while (!(read_reg(REG_DIV_IRQ) & 0x04)) {
        if (HAL_GetTick() > deadline) return MFRC522_ERR;
    }
    write_reg(REG_COMMAND, CMD_IDLE);
    out[0] = read_reg(REG_CRC_RESULT_L);
    out[1] = read_reg(REG_CRC_RESULT_H);
    return MFRC522_OK;
}

// ── Core transceive ────────────────────────────────────────────────────────
static MFRC522_Status_t transceive(
    uint8_t  chip_cmd,
    uint8_t *tx, uint8_t tx_len,
    uint8_t *rx, uint8_t *rx_len,
    uint8_t *valid_bits,
    uint32_t timeout_ms)
{
    uint8_t tx_last_bits = valid_bits ? *valid_bits : 0;
    uint8_t wait_irq     = (chip_cmd == CMD_MF_AUTHENT) ? 0x10 : 0x30;

    write_reg(REG_COMMAND, CMD_IDLE);
    clr_bits(REG_COM_IRQ, 0x80);
    set_bits(REG_FIFO_LEVEL, 0x80);
    for (uint8_t i = 0; i < tx_len; i++) write_reg(REG_FIFO_DATA, tx[i]);
    write_reg(REG_BIT_FRAMING, tx_last_bits);
    write_reg(REG_COMMAND, chip_cmd);
    if (chip_cmd == CMD_TRANSCEIVE) set_bits(REG_BIT_FRAMING, 0x80);

    uint32_t deadline = HAL_GetTick() + timeout_ms;
    uint8_t irq;
    do {
        irq = read_reg(REG_COM_IRQ);
        if (HAL_GetTick() > deadline) {
            clr_bits(REG_BIT_FRAMING, 0x80);
            return MFRC522_NOTAG;
        }
    } while (!(irq & 0x01) && !(irq & wait_irq));
    clr_bits(REG_BIT_FRAMING, 0x80);

    if (!(irq & wait_irq))       return MFRC522_NOTAG;
    if (read_reg(REG_ERROR) & 0x13) return MFRC522_ERR;

    if (rx && rx_len) {
        uint8_t n = read_reg(REG_FIFO_LEVEL);
        if (n > *rx_len) n = *rx_len;
        *rx_len = n;
        for (uint8_t i = 0; i < n; i++) rx[i] = read_reg(REG_FIFO_DATA);
        if (valid_bits) *valid_bits = read_reg(REG_CONTROL) & 0x07;
    }

    if (read_reg(REG_ERROR) & 0x08) return MFRC522_ERR;  // collision
    return MFRC522_OK;
}

// ── PICC: REQA ────────────────────────────────────────────────────────────
static bool picc_reqa(void) {
    uint8_t atqa[2], len = 2, bits = 7;
    write_reg(REG_TX_MODE,  0x00);
    write_reg(REG_RX_MODE,  0x00);
    write_reg(REG_MOD_WIDTH, 0x26);
    uint8_t cmd = PICC_REQA;
    // 2 ms timeout — fast fail when no card present
    return transceive(CMD_TRANSCEIVE, &cmd, 1, atqa, &len, &bits, 2) == MFRC522_OK
           && len == 2;
}

// ── PICC: Anti-collision + SELECT (single-size UID, 4 bytes) ──────────────
static bool picc_select(MFRC522_UID_t *uid) {
    uint8_t buf[9], len, bits = 0;

    // Anti-collision
    buf[0] = PICC_SEL_CL1;
    buf[1] = 0x20;
    len    = 5;
    clr_bits(REG_COLL, 0x80);
    if (transceive(CMD_TRANSCEIVE, buf, 2, &buf[2], &len, &bits, 5) != MFRC522_OK) return false;
    if (len != 5) return false;

    uint8_t bcc = buf[2] ^ buf[3] ^ buf[4] ^ buf[5];
    if (bcc != buf[6]) return false;

    uid->size   = 4;
    uid->uid[0] = buf[2];
    uid->uid[1] = buf[3];
    uid->uid[2] = buf[4];
    uid->uid[3] = buf[5];

    // SELECT
    buf[0] = PICC_SEL_CL1;
    buf[1] = 0x70;
    buf[6] = bcc;
    uint8_t crc[2];
    if (calc_crc(buf, 7, crc) != MFRC522_OK) return false;
    buf[7] = crc[0];
    buf[8] = crc[1];

    uint8_t sak[3];
    len = sizeof(sak);
    if (transceive(CMD_TRANSCEIVE, buf, 9, sak, &len, &bits, 5) != MFRC522_OK) return false;
    if (len != 3) return false;

    uid->sak = sak[0];
    return true;
}

// ── PICC: HLTA ────────────────────────────────────────────────────────────
static void picc_halt(void) {
    uint8_t buf[4] = {PICC_HLTA, 0, 0, 0};
    calc_crc(buf, 2, &buf[2]);
    transceive(CMD_TRANSCEIVE, buf, 4, NULL, NULL, NULL, 5);
}

// ── MIFARE: authenticate + read block ────────────────────────────────────
static MFRC522_Status_t mifare_auth(uint8_t block, MFRC522_UID_t *uid) {
    uint8_t buf[12];
    buf[0] = PICC_MF_AUTH_A;
    buf[1] = block;
    memcpy(&buf[2], KEY_A_DEFAULT, 6);
    memcpy(&buf[8], &uid->uid[uid->size - 4], 4);
    return transceive(CMD_MF_AUTHENT, buf, 12, NULL, NULL, NULL, 10);
}

static MFRC522_Status_t mifare_read(uint8_t block, uint8_t *out, uint8_t *out_len) {
    uint8_t buf[4] = {PICC_MF_READ, block, 0, 0};
    calc_crc(buf, 2, &buf[2]);
    return transceive(CMD_TRANSCEIVE, buf, 4, out, out_len, NULL, 10);
}

// ── Public API ─────────────────────────────────────────────────────────────
void MFRC522_Init(void) {
    HAL_GPIO_WritePin(RFID_RST_Port, RFID_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RFID_RST_Port, RFID_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(50);

    write_reg(REG_COMMAND, CMD_SOFT_RESET);
    HAL_Delay(50);

    // Timer: auto, prescaler → ~40 kHz, reload 1000 → ~25 ms timeout
    write_reg(REG_T_MODE,      0x80);
    write_reg(REG_T_PRESCALER, 0xA9);
    write_reg(REG_T_RELOAD_H,  0x03);
    write_reg(REG_T_RELOAD_L,  0xE8);
    write_reg(REG_TX_ASK, 0x40);   // 100% ASK
    write_reg(REG_MODE,   0x3D);   // CRC preset 0x6363

    // Antenna on
    uint8_t tx = read_reg(REG_TX_CONTROL);
    if ((tx & 0x03) != 0x03) set_bits(REG_TX_CONTROL, 0x03);
}

// Returns employee ID (> 0) on successful card read, 0 if no card or error.
// Call from Inputs_Update() every 50 ms or so.
// Halts the card after reading so the same tap isn't re-detected.
uint32_t MFRC522_ReadEmployeeID(void) {
    if (!picc_reqa()) return 0;

    printf("[RFID] card detected\r\n");

    MFRC522_UID_t uid;
    if (!picc_select(&uid)) {
        printf("[RFID] select failed\r\n");
        return 0;
    }
    printf("[RFID] UID: %02X %02X %02X %02X\r\n",
           uid.uid[0], uid.uid[1], uid.uid[2], uid.uid[3]);

    if (mifare_auth(RFID_EMPLOYEE_BLOCK, &uid) != MFRC522_OK) {
        printf("[RFID] auth failed (block %d) — wrong key or wrong sector?\r\n", RFID_EMPLOYEE_BLOCK);
        clr_bits(REG_STATUS2, 0x08);
        picc_halt();
        return 0;
    }
    printf("[RFID] auth OK\r\n");

    uint8_t block[18], block_len = sizeof(block);
    MFRC522_Status_t st = mifare_read(RFID_EMPLOYEE_BLOCK, block, &block_len);
    clr_bits(REG_STATUS2, 0x08);
    picc_halt();

    if (st != MFRC522_OK || block_len < 4) {
        printf("[RFID] read failed (status=%d, len=%d)\r\n", st, block_len);
        return 0;
    }

    printf("[RFID] block %d raw: %02X %02X %02X %02X\r\n",
           RFID_EMPLOYEE_BLOCK, block[0], block[1], block[2], block[3]);

    /* Cards store the ID as an ASCII decimal string (e.g. "1003" = 0x31 0x30 0x30 0x33) */
    char id_str[5];
    memcpy(id_str, block, 4);
    id_str[4] = '\0';
    uint32_t id = (uint32_t)atoi(id_str);
    printf("[RFID] employee ID = %lu\r\n", (unsigned long)id);
    return id;
}
