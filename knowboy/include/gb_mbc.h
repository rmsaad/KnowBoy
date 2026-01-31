/**
 * @file gb_mbc.h
 * @brief API for the gameboy memory bank controller functionaliy.
 *
 * @author Rami Saad
 * @date 2021-07-08
 */

#ifndef INCLUDE_GB_MBC_H_
#define INCLUDE_GB_MBC_H_

#include <stdint.h>

void gbc_mbc_init(void);
void gb_mbc_set_cartridge_info(uint8_t code, uint8_t rom_size, uint8_t ram_size);
uint8_t gb_mbc_read_rom_bank(uint16_t address);
void gb_mbc_write_register(uint16_t address, uint8_t data);
uint8_t gb_mbc_read_ram_bank(uint16_t address);
void gb_mbc_write_ram_bank(uint16_t address, uint8_t data);
#endif /* INCLUDE_GB_MBC_H_ */
