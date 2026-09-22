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
#include <stdlib.h>
#include <string.h>

typedef enum {
	ROM_ONLY = 0x00,
	MBC1 = 0x01,
	MBC1_RAM = 0x02,
	MBC1_RAM_BATTERY = 0x03,
	MBC2 = 0x05,
	MBC2_BATTERY = 0x06,
	ROM_RAM = 0x08,
	ROM_RAM_BATTERY = 0x09,
	MMM01 = 0x0B,
	MMM01_RAM = 0x0C,
	MMM01_RAM_BATTERY = 0x0D,
	MBC3_TIMER_BATTERY = 0x0F,
	MBC3_TIMER_RAM_BATTERY = 0x10,
	MBC3 = 0x11,
	MBC3_RAM = 0x12,
	MBC3_RAM_BATTERY = 0x13,
	MBC5 = 0x19,
	MBC5_RAM = 0x1A,
	MBC5_RAM_BATTERY = 0x1B,
	MBC5_RUMBLE = 0x1C,
	MBC5_RUMBLE_RAM = 0x1D,
	MBC5_RUMBLE_RAM_BATTERY = 0x1E,
	MBC6 = 0x20,
	MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22,
	POCKET_CAMERA = 0xFC,
	BANDAI_TAMA5 = 0xFD,
	HUC3 = 0xFE,
	HUC1_RAM_BATTERY = 0xFF,
} cartridge_type_t;

#define ROM_BANK_SIZE 16384
#define RAM_BANK_SIZE 8192

#define NINTENDO_LOGO_ADDR 0x0104
#define NINTENDO_LOGO_SIZE 0x30

static cartridge_type_t gb_mbc_controller_code = ROM_ONLY;
static bool gb_mbc_multicart = false;
static uint8_t gb_mbc_rom_bank_count = 0;
static uint8_t gb_mbc_ram_bank_count = 0;
static uint8_t gb_mbc_ram_enable = 0;
static uint8_t gb_mbc_bank1 = 0x01;
static uint8_t gb_mbc_bank2 = 0x0;
static uint8_t gb_mbc_bank_mode = 0x0;
static uint8_t *gb_mbc_bank_ram = NULL;

extern const uint8_t *rom;
extern memory_t mem;

static const uint16_t gb_mbc_rom_bank_lut[] = {
	2,   // 32KB
	4,   // 64KB
	8,   // 128KB
	16,  // 256KB
	32,  // 512KB
	64,  // 1MB
	128, // 2MB
	256, // 4MB
	512  // 8MB
};

static const uint8_t gb_mbc_ram_bank_lut[] = {
	0,  // No RAM
	1,  // not used
	1,  // 8KB   (1 bank of 8KB)
	4,  // 32KB  (4 banks of 8KB each)
	16, // 128KB (16 banks of 8KB each)
	8   // 64KB  (8 banks of 8KB each)
};

void gbc_mbc_init(void)
{
	gb_mbc_controller_code = ROM_ONLY;
	gb_mbc_ram_enable = 0;
	gb_mbc_bank1 = 0x01;
	gb_mbc_bank2 = 0x0;
	gb_mbc_bank_mode = 0x0;
}

/**
 * @brief sets the cartridge type for use in this file corresponding to data stored at the memory
 * location 0x147
 * @param code cartridge type code at memory location 0x147
 * @param rom_size rom size data stored at memory location 0x148
 * @param ram_size ram size data stored at memory location 0x149
 * @returns Nothing
 */
void gb_mbc_set_cartridge_info(uint8_t code, uint8_t rom_size, uint8_t ram_size)
{
	gb_mbc_controller_code = (cartridge_type_t)code;
	gb_mbc_rom_bank_count = gb_mbc_rom_bank_lut[rom_size];
	gb_mbc_ram_bank_count = gb_mbc_ram_bank_lut[ram_size];
	gb_mbc_multicart = false;
	if (gb_mbc_ram_bank_count > 0) {
		gb_mbc_bank_ram = (uint8_t *)malloc(gb_mbc_ram_bank_count * RAM_BANK_SIZE);
	}

	if (gb_mbc_controller_code == MBC1 || gb_mbc_controller_code == MBC1_RAM ||
	    gb_mbc_controller_code == MBC1_RAM_BATTERY) {
		if (gb_mbc_rom_bank_count == 64) {
			uint8_t nintendo_logo_count = 1;
			for (int i = 1; i < 4; i++) {
				if (memcmp(&rom[NINTENDO_LOGO_ADDR],
					   &rom[i * 16 * ROM_BANK_SIZE + NINTENDO_LOGO_ADDR],
					   NINTENDO_LOGO_SIZE) == 0) {
					nintendo_logo_count++;
				}
				if (nintendo_logo_count >= 3) {
					gb_mbc_multicart = true;
				}
			}
		}
	}
}

/**
 * @brief returns data from a ROM location in the memory map depending on the MBC type
 * @param address memory map address
 * @returns data stored at specified ROM address
 */
uint8_t gb_mbc_read_rom_bank(uint16_t address)
{
	uint32_t bank = 0;
	switch (gb_mbc_controller_code) {
	case ROM_ONLY:
		return (uint8_t)rom[address];
	case MBC1:
	case MBC1_RAM:
	case MBC1_RAM_BATTERY:
		if (address < CARTROM_BANKX) {
			if (gb_mbc_multicart) {
				bank = (gb_mbc_bank_mode == 0) ? 0 : (gb_mbc_bank2 << 4);
			} else {
				bank = (gb_mbc_bank_mode == 0) ? 0 : (gb_mbc_bank2 << 5);
			}

			if (bank >= gb_mbc_rom_bank_count) {
				bank = bank % gb_mbc_rom_bank_count;
			}
			uint32_t addr = (uint32_t)(bank * ROM_BANK_SIZE) + address;
			return rom[addr];
		} else {
			if (gb_mbc_multicart) {
				bank = (gb_mbc_bank2 << 4) + (gb_mbc_bank1 & 0x0F);
			} else {
				bank = (gb_mbc_bank2 << 5) + gb_mbc_bank1;
			}
			if (bank >= gb_mbc_rom_bank_count) {
				bank = bank % gb_mbc_rom_bank_count;
			}
			uint32_t addr =
				(uint32_t)(bank * ROM_BANK_SIZE) + (address & (ROM_BANK_SIZE - 1));
			return rom[addr];
		}

	default:
		return (uint8_t)rom[address];
	}
}

/**
 * @brief Data is written to MBC register when the address falls in range 0x0000 to 0x7FFF.
 * @details When the range 0x0000 - 0x1FFF is written to with value 0x0A, the Gameboy
 * external RAM is enabled. Any other value written to this range will result in the
 * external RAM being disabled. When the range 0x2000 - 0x3FFF is written to, the lower 5
 * bits correspond to the switch-able ROM bank that read operation are to be read from. When
 * the range 0x4000 - 0x5FFF is written to, depending on the mode it will either specify the
 * top 2 bits of the ROM bank to be read from or it specify the current RAM bank. When the
 * range falls between 0x6000 - 0x7FFF if the value 0 is written then ROM mode is selected
 * and if the value 1 is written then RAM mode is selected.
 * @param address memory map address
 * @param data byte to be written to MBC register
 * @returns Nothing
 */
void gb_mbc_write_register(uint16_t address, uint8_t data)
{
	if (gb_mbc_controller_code != ROM_ONLY) {
		if (address < 0x2000) {
			if ((data & 0x0F) == 0x0A) {
				gb_mbc_ram_enable = 1;
			} else {
				gb_mbc_ram_enable = 0;
			}
		} else if (address < 0x4000) {
			gb_mbc_bank1 = (data & 0x1F);
			if (gb_mbc_bank1 == 0) {
				gb_mbc_bank1 = 1;
			}
		} else if (address < 0x6000) {
			gb_mbc_bank2 = (data & 0x03);
		} else if (address < 0x8000) {
			gb_mbc_bank_mode = (data & 0x01);
		}
	}
}

/**
 * @brief returns data from a RAM location in the memory map depending on the MBC type
 * @param address memory map address
 * @returns data stored at specified RAM address
 */
uint8_t gb_mbc_read_ram_bank(uint16_t address)
{
	if (gb_mbc_ram_enable && gb_mbc_bank_ram != NULL) {
		uint32_t bank = (gb_mbc_bank_mode == 0) ? 0 : gb_mbc_bank2;
		if (bank >= gb_mbc_ram_bank_count) {
			bank = bank % gb_mbc_ram_bank_count;
		}
		return gb_mbc_bank_ram[bank * RAM_BANK_SIZE + (address & (RAM_BANK_SIZE - 1))];
	} else {
		return 0xFF;
	}
}

/**
 * @brief writes data to a RAM location in the memory map depending on the MBC type
 * @param address memory map address
 * @param data byte to be written to RAM location
 * @returns Nothing
 */
void gb_mbc_write_ram_bank(uint16_t address, uint8_t data)
{
	if (gb_mbc_ram_enable && gb_mbc_bank_ram != NULL) {
		uint32_t bank = (gb_mbc_bank_mode == 0) ? 0 : gb_mbc_bank2;
		if (bank >= gb_mbc_ram_bank_count) {
			bank = bank % gb_mbc_ram_bank_count;
		}
		gb_mbc_bank_ram[bank * RAM_BANK_SIZE + (address & (RAM_BANK_SIZE - 1))] = data;
	} else {
		return;
	}
}
