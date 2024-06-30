/**
 * @file gb_mbc.c
 * @brief Gameboy memory bank controller Functionality.
 *
 * This file emulates all functionality of the gameboy's memory bank controller embedded into each
 * game's cartridge.
 *
 * @author Rami Saad
 * @date 2021-06-11
 */

#include "gb_memory.h"
#include "logging.h"

#include <stdint.h>

static uint8_t gb_mbc_controller_code = 0;
static uint8_t gb_mbc_ram_enable = 0;
static uint8_t gb_mbc_offset_low = 0x01;
static uint8_t gb_mbc_offset_high = 0x0;
static uint8_t gb_mbc_bank_mode = 0x0;

extern const uint8_t *rom;

void gbc_mbc_init(void)
{
	gb_mbc_controller_code = 0;
	gb_mbc_ram_enable = 0;
	gb_mbc_offset_low = 0x01;
	gb_mbc_offset_high = 0x0;
	gb_mbc_bank_mode = 0x0;
}

/**
 * @brief sets the cartridge type for use in this file corresponding to data stored at the memory
 * location 0x147
 * @param code data stored at memory location 0x147
 * @returns Nothing
 */
void gb_mbc_set_controller_type(uint8_t code)
{
	gb_mbc_controller_code = code;
}

/**
 * @brief This function will return data from a ROM location in the memory map depending on the MBC
 * type
 * @param address memory map address
 * @returns data stored at specified ROM address
 */
uint8_t gb_mbc_read_bank_x(uint16_t address)
{
	if (address < CARTROM_BANKX) {
		return (uint8_t)rom[address];
	} else if (gb_mbc_controller_code == 0) {
		return (uint8_t)rom[address];
	} else {
		return (uint8_t)
			rom[((gb_mbc_offset_high + gb_mbc_offset_low - 1) * 0x4000) + (address)];
	}
}

/**
 * @brief Data is written to MBC register when the address falls in range 0x0000 to 0x7FFF.
 * @details When the range 0x0000 - 0x1FFF is written to with value 0x0A, the Gameboy external RAM
 * is enabled. Any other value written to this range will result in the external RAM being disabled.
 * When the range 0x2000 - 0x3FFF is written to, the lower 5 bits correspond to the switch-able ROM
 * bank that read operation are to be read from. When the range 0x4000 - 0x5FFF is written to,
 * depending on the mode it will either specify the top 2 bits of the ROM bank to be read from or it
 * specify the current RAM bank. When the range falls between 0x6000 - 0x7FFF if the value 0 is
 * written then ROM mode is selected and if the value 1 is written then RAM mode is selected.
 * @param address memory map address
 * @param data byte to be written to MBC register
 * @returns Nothing
 */
void gb_mbc_write(uint16_t address, uint8_t data)
{
	if (gb_mbc_controller_code > 0) {
		if (address < 0x2000) {
			if (data == 0x0A) {
				gb_mbc_ram_enable = 1;
			} else {
				gb_mbc_ram_enable = 0;
			}
		} else if (address < 0x4000) {
			gb_mbc_offset_low = (data & 0x1F);
			if (gb_mbc_offset_low == 0) {
				gb_mbc_offset_low = 1;
			}
		} else if (address < 0x6000) {
			if (gb_mbc_bank_mode == 0) {
				gb_mbc_offset_high = ((data & 0x03) << 5);

			} else {
			}
		} else {
			gb_mbc_bank_mode = (data & 0x01);
		}
	}
}
