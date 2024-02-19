/**
 * @file gb_memory.c
 * @brief Gameboy memory map functionality.
 *
 * This file Handles all reads and writes to locations in gameboy's memory map.
 *
 * @author Rami Saad
 * @date 2021-03-28
 */

#include "gb_common.h"
#include "gb_mbc.h"
#include "gb_memory.h"
#include "gb_papu.h"
#include "logging.h"

#include <stdint.h>
#include <string.h>

registers_t reg;
memory_t mem;
uint8_t current_opcode;
static uint8_t joypad_sel_dir = 0;
static uint8_t joypad_sel_but = 0;
static uint8_t timer_stop_start = 0;
static uint8_t clock_mode = 0;
static uint8_t data_trans_flag = 0;
const uint8_t *rom;

static gb_memory_controls_t gb_memory_controls;

/*Function Prototypes*/
uint8_t gb_memory_joypad(void);

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
 * @brief Set the current opcode for Debug Printing to LCD screen. See gb_memory_print();
 * @param op current opcode.
 * @return Nothing
 */
void gb_memory_set_op(uint8_t op)
{
	current_opcode = op;
}

/**
 * @brief Initialize certain Gameboy registers with their correct information.
 * @details At start up the Joypad Register should read 0xCF to denote that no Joypad buttons are
 * being pressed. The IF register should read 0xE1 to set the appropriate flags.
 * @return Nothing
 */
void gb_memory_init(const uint8_t *boot_rom, const uint8_t *game_rom)
{
	// memset(mem.ram, 0, 0xFFFF);
	gbc_mbc_init();
	rom = game_rom;
	memset(&mem.ram[0], 0x00, 0xFFFF);
	gb_memory_load(game_rom, 32768);
	gb_memory_load(boot_rom, 256);
	gb_mbc_set_controller_type(mem.ram[0x147]);
	LOG_INF("CONTROLLER TYPE: %x", mem.ram[0x147]);
	reg.PC = 0;
	reg.AF = 0;
	reg.BC = 0;
	reg.DE = 0;
	reg.HL = 0;
	reg.SP = 0;
	clock_mode = 0;
	timer_stop_start = 0;
	data_trans_flag = 0;
	mem.ram[JOY_ADDR] = 0xCF;
	mem.ram[IF_ADDR] = 0xE1;
	gb_memory_write(TAC_ADDR, 0xF8);
}

/**
 * @brief Loads data of amount bytes into Memory maps from mem.ram[0] to mem.ram[bytes - 1].
 * @param data data to be loaded into memory map.
 * @param size amount of bytes.
 * @return Nothing
 */
void gb_memory_load(const void *data, uint32_t bytes)
{
	memcpy(mem.ram, data, bytes);
}

/**
 * @brief Handles any writes to the Joypad Register 0xFF00
 * @param data data trying to be written to Joypad Register.
 * @return Joypad Keys pressed
 */
uint8_t gb_memory_joypad(void)
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
			mem.ram[DIV_ADDR] = 0;
			return;
		}

		else if (address == TAC_ADDR) {
			timer_stop_start = CHK_BIT(data, 2);
			clock_mode = (CHK_BIT(data, 1) * 2) + CHK_BIT(data, 0);
			mem.ram[address] = data;
			return;
		}

		else if (address == STC_ADDR) {
			if (CHK_BIT(data, 7)) {
				data_trans_flag = 1;
			}
			mem.ram[address] = data;
			return;
		}

		else if (address == BOOT_EN_ADDR) {
			if (data == 1) {
				LOG_INF("BOOT ROM SWITCH !!!!!!!!!!!!!!!!");
				LOG_HEXDUMP_INF(gb_memory_get_rom_pointer(), 256);
				gb_memory_load(gb_memory_get_rom_pointer(), 256);
			}
		}

		else if (address == NR52_ADDR) {
			//            if(CHK_BIT(data, 7)) {
			//                mem.ram[address] |= 0x80;
			//            }else {
			//                mem.ram[address] &= ~(0x80);
			//            }
			//            return;
			if (data) {
				mem.ram[NR52_ADDR] = 0xFF;
			} else {
				mem.ram[NR52_ADDR] = 0x00;
			}
			return;
		}

		else if (address == NR14_ADDR && CHK_BIT(data, 7)) {
			mem.ram[address] = data;
			gb_papu_trigger_ch1(mem.ram[NR11_ADDR] & 0x3f);
			return;
		}

		else if (address == NR24_ADDR && CHK_BIT(data, 7)) {
			mem.ram[address] = data;
			gb_papu_trigger_ch2(mem.ram[NR21_ADDR] & 0x3f);
			return;
		}

		else if (address == NR34_ADDR && CHK_BIT(data, 7)) {
			mem.ram[address] = data;
			gb_papu_trigger_ch3(mem.ram[NR31_ADDR]);
			return;
		}

		else if (address == NR44_ADDR && CHK_BIT(data, 7)) {
			mem.ram[address] = data;
			gb_papu_trigger_ch4(mem.ram[NR41_ADDR] & 0x3f);
			return;
		}
	}

	if ((address >= CARTROM_BANK0 && address < VRAM_BASE)) {
		gb_mbc_write(address, data);
		return;
	}

	if (address >= ECHORAM_BASE && address < OAM_BASE) {
		mem.ram[address - 0x2000] = data;
		return;
	}

	mem.ram[address] = data;
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
		mem.ram[address - 0x2000] |= (0x1 << bit);

	if ((address >= CARTROM_BANK0 && address < VRAM_BASE))
		return;

	mem.ram[address] |= (0x1 << bit);
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
		mem.ram[address - 0x2000] &= ~(0x1 << bit);

	if ((address >= CARTROM_BANK0 && address < VRAM_BASE))
		return;

	mem.ram[address] &= ~(0x1 << bit);
}

/**
 * @brief Read from location in memory map
 * @param address memory map address
 * @return data byte of data located in memory map
 */
uint8_t gb_memory_read(uint16_t address)
{

	if ((address >= CARTROM_BANK0 && address < VRAM_BASE) && mem.ram[BOOT_EN_ADDR] != 0) {
		return gb_mbc_read_bank_x(address);
	}

	else if (address >= IO_BASE) {
		if (address == JOY_ADDR) {
			return gb_memory_joypad();
		}

		if (address == SB_ADDR) {
			return 0xFF;
		}
	}

	else if (address >= ECHORAM_BASE && address < OAM_BASE)
		return mem.ram[address - 0x2000];

	return mem.ram[address];
}

/**
 * @brief Read from 2 sequential locations in memory map
 * @param address memory map address
 * @return Short information of data in memory map
 */
uint16_t gb_memory_read_short(uint16_t address)
{
	return CAT_BYTES(mem.ram[address], mem.ram[address + 1]);
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
		mem.ram[DIV_ADDR]++;

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
			mem.ram[TIMA_ADDR]++;
		}

		if (timer_tima + curDuration > 0x1FE) {
			mem.ram[TIMA_ADDR]++;
		}

		timer_tima += curDuration;

		if (mem.ram[TIMA_ADDR] < 5 && old_tima == 0xFF) {
			mem.ram[TIMA_ADDR] = mem.ram[TMA_ADDR];
			gb_memory_set_bit(IF_ADDR, 2);
		}

		old_tima = mem.ram[TIMA_ADDR];
	}
}

/**
 * @brief Print out all the Register value information to the Screen for debugging purposes.
 * @returns Nothing
 */
void gb_memory_print(void)
{
	LOG_INF("opcode: %x, PC: %x, AF: %x, BC: %x, DE: %x, HL: %x, SP: %x", current_opcode,
		reg.PC, reg.AF, reg.BC, reg.DE, reg.HL, reg.SP);
}
