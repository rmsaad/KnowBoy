/**
 * @file gb_memory.c
 * @brief Gameboy memory map functionality.
 *
 * This file Handles all reads and writes to locations in gameboy's memory map.
 *
 * @author Rami Saad
 * @date 2021-03-28
 */

#include "gb_apu.h"
#include "gb_common.h"
#include "gb_mbc.h"
#include "gb_memory.h"
#include "logging.h"

#include <stdint.h>
#include <string.h>

memory_t mem;
const uint8_t *rom;
static uint8_t joypad_sel_dir = 0;
static uint8_t joypad_sel_but = 0;
static uint8_t timer_stop_start = 0;
static uint8_t clock_mode = 0;
static uint8_t data_trans_flag = 0;

static gb_memory_controls_t gb_memory_controls;

/*Function Prototypes*/
static uint8_t gb_memory_joypad(void);

/**
 * @brief Sets function used in gb_memory_joypad() without needing to include control.h
 * @param gb_memory_controls_function function pointer holding address of ucControlsJoypad()
 * function in controls.c
 * @return None
 */
void gb_memory_set_control_function(gb_memory_controls_t controls)
{
	gb_memory_controls = controls;
}

/**
 * @brief Returns pointer to Start of Gameboy ROM
 * @return Gameboy ROM
 */
const uint8_t *gb_memory_get_rom_pointer(void)
{
	return rom;
}

/**
 * @brief Initialize certain Gameboy registers with their correct information.
 * @details At start up the Joypad Register should read 0xCF to denote that no Joypad buttons are
 * being pressed. The IF register should read 0xE1 to set the appropriate flags.
 * @return Nothing
 */
void gb_memory_init(const uint8_t *boot_rom, const uint8_t *game_rom, bool boot_skip)
{
	gbc_mbc_init();
	rom = game_rom;
	memset(&mem.map[0], 0x00, 0xFFFF);
	gb_memory_load(game_rom, 32768);
	gb_mbc_set_controller_type(mem.map[0x147]);
	gb_memory_write(TAC_ADDR, 0xF8);
	mem.map[JOY_ADDR] = 0xCF;
	mem.map[IF_ADDR] = 0xE1;

	if (boot_skip) {
		mem.map[LCDC_ADDR] = 0x91;
		mem.map[STAT_ADDR] = 0x01;
		mem.map[DIV_ADDR] = 0xAB;
		mem.map[NR11_ADDR] = 0x80;
		mem.map[NR12_ADDR] = 0xF3;
		mem.map[NR13_ADDR] = 0xC1;
		mem.map[NR14_ADDR] = 0x87;
		mem.map[NR50_ADDR] = 0x77;
		mem.map[NR51_ADDR] = 0xF3;
		mem.map[NR52_ADDR] = 0xF1;
		mem.map[LY_ADDR] = 0x99;
		mem.map[BGP_ADDR] = 0xFC;
		mem.map[BOOT_EN_ADDR] = 0x01;
		mem.map[0xFFFA] = 0x39;
		mem.map[0xFFFB] = 0x01;
		mem.map[0xFFFC] = 0x2E;
		mem.reg.PC = 0x0100;
		mem.reg.AF = 0x01B0;
		mem.reg.BC = 0x0013;
		mem.reg.DE = 0x0008;
		mem.reg.HL = 0x014D;
		mem.reg.SP = 0xFFFE;
	} else {
		gb_memory_load(boot_rom, 256);
		mem.reg.PC = 0;
		mem.reg.AF = 0;
		mem.reg.BC = 0;
		mem.reg.DE = 0;
		mem.reg.HL = 0;
		mem.reg.SP = 0;
		clock_mode = 0;
		timer_stop_start = 0;
		data_trans_flag = 0;
	}
}

/**
 * @brief Loads data of amount bytes into Memory maps from mem.map[0] to mem.map[bytes - 1].
 * @param data data to be loaded into memory map.
 * @param size amount of bytes.
 * @return Nothing
 */
void gb_memory_load(const void *data, uint32_t bytes)
{
	memcpy(mem.map, data, bytes);
}

/**
 * @brief Handles any writes to the Joypad Register 0xFF00
 * @param data data trying to be written to Joypad Register.
 * @return Joypad Keys pressed
 */
static uint8_t gb_memory_joypad(void)
{
	return gb_memory_controls(&joypad_sel_dir, &joypad_sel_but);
}

/**
 * @brief Write data to location in memory map specified by address variable
 * @param address memory map address
 * @param data byte of data
 * @returns Nothing
 */
void gb_memory_write(uint16_t address, uint8_t data)
{

	if (address >= IO_BASE) {

		if (address == JOY_ADDR) {
			joypad_sel_dir = data & 0x10;
			joypad_sel_but = data & 0x20;
			return;
		}

		else if (address == DMA_ADDR) {
			for (uint16_t i = 0; i < 40 * 4; i++)
				gb_memory_write(OAM_BASE + i, gb_memory_read((data << 8) + i));
			return;
		}

		else if (address == DIV_ADDR) {
			mem.map[DIV_ADDR] = 0;
			return;
		}

		else if (address == TAC_ADDR) {
			timer_stop_start = CHK_BIT(data, 2);
			clock_mode = (CHK_BIT(data, 1) * 2) + CHK_BIT(data, 0);
			mem.map[address] = data;
			return;
		}

		else if (address == STC_ADDR) {
			if (CHK_BIT(data, 7)) {
				data_trans_flag = 1;
				LOG_DBG("SER: %c", mem.map[SB_ADDR]);
			}
			mem.map[address] = data;
			return;
		}

		else if (address == BOOT_EN_ADDR) {
			if (data == 1) {
				gb_memory_load(gb_memory_get_rom_pointer(), 256);
			}
		}

		else if (address == NR52_ADDR) {
			if (CHK_BIT(data, 7)) {
				mem.map[address] |= 0xF0;
			} else {
				mem.map[address] &= ~(0xF0);
			}
			return;
		}

		else if (address == NR14_ADDR && CHK_BIT(data, 7)) {
			mem.map[address] = data;
			gb_apu_trigger_ch1();
			return;
		}

		else if (address == NR24_ADDR && CHK_BIT(data, 7)) {
			mem.map[address] = data;
			gb_apu_trigger_ch2();
			return;
		}

		else if (address == NR34_ADDR && CHK_BIT(data, 7)) {
			mem.map[address] = data;
			gb_apu_trigger_ch3();
			return;
		}

		else if (address == NR44_ADDR && CHK_BIT(data, 7)) {
			mem.map[address] = data;
			gb_apu_trigger_ch4();
			return;
		}
	}

	if ((address >= CARTROM_BANK0 && address < VRAM_BASE)) {
		gb_mbc_write(address, data);
		return;
	}

	if (address >= ECHORAM_BASE && address < OAM_BASE) {
		mem.map[address - 0x2000] = data;
		return;
	}

	mem.map[address] = data;
}

/**
 * @brief Write a short value to location in the memory map
 * @param address memory map address
 * @param data 2 bytes of data
 * @returns Nothing
 */
void gb_memory_write_short(uint16_t address, uint16_t data)
{
	gb_memory_write(address, data & 0xFF);
	gb_memory_write(address + 1, data >> 8);
}

/**
 * @brief Set a bit in location specified by memory map address
 * @param address memory map address
 * @param bit which bit to set (0 - 7)
 * @returns Nothing
 */
void gb_memory_set_bit(uint16_t address, uint8_t bit)
{
	if (address >= ECHORAM_BASE && address < OAM_BASE)
		mem.map[address - 0x2000] |= (0x1 << bit);

	if ((address >= CARTROM_BANK0 && address < VRAM_BASE))
		return;

	mem.map[address] |= (0x1 << bit);
}

/**
 * @brief Reset a bit in location specified by memory map address
 * @param address memory map address
 * @param bit which bit to reset (0 - 7)
 * @returns Nothing
 */
void gb_memory_reset_bit(uint16_t address, uint8_t bit)
{

	if (address >= ECHORAM_BASE && address < OAM_BASE)
		mem.map[address - 0x2000] &= ~(0x1 << bit);

	if ((address >= CARTROM_BANK0 && address < VRAM_BASE))
		return;

	mem.map[address] &= ~(0x1 << bit);
}

/**
 * @brief Read from location in memory map
 * @param address memory map address
 * @return data byte of data located in memory map
 */
uint8_t gb_memory_read(uint16_t address)
{

	if ((address >= CARTROM_BANK0 && address < VRAM_BASE) && mem.map[BOOT_EN_ADDR] != 0) {
		return gb_mbc_read_bank_x(address);
	}

	else if (address >= IO_BASE) {
		switch (address) {
		case JOY_ADDR:
			return gb_memory_joypad();
		case SB_ADDR:
			return 0xFF;
		case NR10_ADDR:
			return 0x80 | mem.map[address];
		case NR11_ADDR:
			return 0x3F | mem.map[address];
		case NR12_ADDR:
			return 0x00 | mem.map[address];
		case NR13_ADDR:
			return 0xFF | mem.map[address];
		case NR14_ADDR:
			return 0xBF | mem.map[address];
		case NR20_ADDR:
			return 0xFF | mem.map[address];
		case NR21_ADDR:
			return 0x3F | mem.map[address];
		case NR22_ADDR:
			return 0x00 | mem.map[address];
		case NR23_ADDR:
			return 0xFF | mem.map[address];
		case NR24_ADDR:
			return 0xBF | mem.map[address];
		case NR30_ADDR:
			return 0x7F | mem.map[address];
		case NR31_ADDR:
			return 0xFF | mem.map[address];
		case NR32_ADDR:
			return 0x9F | mem.map[address];
		case NR33_ADDR:
			return 0xFF | mem.map[address];
		case NR34_ADDR:
			return 0xBF | mem.map[address];
		case NR40_ADDR:
			return 0xFF | mem.map[address];
		case NR41_ADDR:
			return 0xFF | mem.map[address];
		case NR42_ADDR:
			return 0x00 | mem.map[address];
		case NR43_ADDR:
			return 0x00 | mem.map[address];
		case NR44_ADDR:
			return 0xBF | mem.map[address];
		case NR50_ADDR:
			return 0x00 | mem.map[address];
		case NR51_ADDR:
			return 0x00 | mem.map[address];
		case NR52_ADDR:
			return 0x70 | mem.map[address];
		case 0XFF27:
		case 0XFF28:
		case 0XFF29:
		case 0XFF2A:
		case 0XFF2B:
		case 0XFF2C:
		case 0XFF2D:
		case 0XFF2E:
		case 0XFF2F:
			return 0xFF | mem.map[address];
		case WPRAM_BASE + 0x0:
		case WPRAM_BASE + 0x1:
		case WPRAM_BASE + 0x2:
		case WPRAM_BASE + 0x3:
		case WPRAM_BASE + 0x4:
		case WPRAM_BASE + 0x5:
		case WPRAM_BASE + 0x6:
		case WPRAM_BASE + 0x7:
		case WPRAM_BASE + 0x8:
		case WPRAM_BASE + 0x9:
		case WPRAM_BASE + 0xA:
		case WPRAM_BASE + 0xB:
		case WPRAM_BASE + 0xC:
		case WPRAM_BASE + 0xD:
		case WPRAM_BASE + 0xE:
		case WPRAM_BASE + 0xF:
			return 0x00 | mem.map[address];
		default:
			return mem.map[address];
		}
	}

	else if (address >= ECHORAM_BASE && address < OAM_BASE)
		return mem.map[address - 0x2000];

	return mem.map[address];
}

/**
 * @brief Read from 2 sequential locations in memory map
 * @param address memory map address
 * @return Short information of data in memory map
 */
uint16_t gb_memory_read_short(uint16_t address)
{
	return CAT_BYTES(mem.map[address], mem.map[address + 1]);
}

/**
 * @brief Ticks all timers currently in use
 * @details Ticks the DIV and possibly the TIMA timers at their correct Hertz
 * @param duration Amount of T states that have passed executing the current instruction.
 * @returns Nothing
 */
void gb_memory_inc_timers(uint8_t duration)
{
	static uint8_t timer_div = 0;
	static uint8_t timer_tima = 0;
	static uint8_t old_tima = 0;
	static uint8_t timer_div_8k = 0;

	if ((timer_div + (duration << 2)) > 0xFF) {
		mem.map[DIV_ADDR]++;

		if (data_trans_flag) {
			timer_div_8k++;
			if (timer_div_8k == 0x10) {
				timer_div_8k = 0;
				gb_memory_reset_bit(STC_ADDR, 7);
				data_trans_flag = 0;
				// gb_memory_set_bit(IF_ADDR, 3);
			}
		}
	}

	timer_div += (duration << 2);

	if (timer_stop_start) {
		uint16_t curDuration = 0;

		switch (clock_mode) {
		case 0x0:
			curDuration = (duration << 0);
			break;
		case 0x1:
			curDuration = (duration << 6);
			break;
		case 0x2:
			curDuration = (duration << 4);
			break;
		case 0x3:
			curDuration = (duration << 2);
			break;
		default:
			break;
		}

		if (timer_tima + curDuration > 0xFF) {
			mem.map[TIMA_ADDR]++;
		}

		if (timer_tima + curDuration > 0x1FE) {
			mem.map[TIMA_ADDR]++;
		}

		timer_tima += curDuration;

		if (mem.map[TIMA_ADDR] < 5 && old_tima == 0xFF) {
			mem.map[TIMA_ADDR] = mem.map[TMA_ADDR];
			gb_memory_set_bit(IF_ADDR, 2);
		}

		old_tima = mem.map[TIMA_ADDR];
	}
}
