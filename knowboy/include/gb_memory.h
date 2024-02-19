/**
 * @file gb_memory.h
 * @brief API and MACROs related to the gameboy memory map.
 *
 * @author Rami Saad
 * @date 2021-03-28
 */

#ifndef INCLUDE_GB_MEMORY_H_
#define INCLUDE_GB_MEMORY_H_

#include <stdint.h>
#include <stdio.h>

// ppu modes
#define STAT_MODE_0 0 // HBLANK
#define STAT_MODE_1 1 // VBLANK
#define STAT_MODE_2 2 // accessing OAM
#define STAT_MODE_3 3 // accessing VRAM

#define CARTROM_BANK0 0x0000
#define CARTROM_BANKX 0x4000
#define VRAM_BASE     0x8000
#define CARTRAM_BASE  0xA000
#define GBRAM_BANK0   0xC000
#define ECHORAM_BASE  0xE000
#define OAM_BASE      0xFE00
#define IO_BASE	      0xFF00

// Drawing Related Register Addresses
#define JOY_ADDR  0xFF00
#define SB_ADDR	  0xFF01
#define STC_ADDR  0xFF02
#define DIV_ADDR  0xFF04
#define TIMA_ADDR 0xFF05
#define TMA_ADDR  0xFF06
#define TAC_ADDR  0xFF07

#define NR10_ADDR  0xFF10
#define NR11_ADDR  0xFF11
#define NR12_ADDR  0xFF12
#define NR13_ADDR  0xFF13
#define NR14_ADDR  0xFF14
#define NR21_ADDR  0xFF16
#define NR22_ADDR  0xFF17
#define NR23_ADDR  0xFF18
#define NR24_ADDR  0xFF19
#define NR30_ADDR  0xFF1A
#define NR31_ADDR  0xFF1B
#define NR32_ADDR  0xFF1C
#define NR33_ADDR  0xFF1D
#define NR34_ADDR  0xFF1E
#define NR41_ADDR  0xFF20
#define NR42_ADDR  0xFF21
#define NR43_ADDR  0xFF22
#define NR44_ADDR  0xFF23
#define NR50_ADDR  0xFF24
#define NR51_ADDR  0xFF25
#define NR52_ADDR  0xFF26
#define WPRAM_BASE 0xFF30

#define LCDC_ADDR    0xFF40
#define STAT_ADDR    0xFF41
#define SCY_ADDR     0xFF42
#define SCX_ADDR     0xFF43
#define LY_ADDR	     0xFF44
#define LYC_ADDR     0xFF45
#define DMA_ADDR     0xFF46
#define BGP_ADDR     0xFF47
#define OBP0_ADDR    0xFF48
#define OBP1_ADDR    0xFF49
#define WY_ADDR	     0xFF4A
#define WX_ADDR	     0xFF4B
#define BOOT_EN_ADDR 0xFF50

#define IF_ADDR 0xFF0F
#define IE_ADDR 0xFFFF

// LCDC BIT 4, TILE DATA BASE ADDRESSES
#define TILE_DATA_UNSIGNED_ADDR 0x8000
#define TILE_DATA_SIGNED_ADDR	0x8800

// LCDC BIT 3, TILE MAP OFFSETS BASES ADDRESSES
#define TILE_MAP_LOCATION_LOW  0x9800
#define TILE_MAP_LOCATION_HIGH 0x9C00

typedef struct {
	union {
		struct {
			uint8_t F;
			uint8_t A;
		};
		uint16_t AF;
	};

	union {
		struct {
			uint8_t C;
			uint8_t B;
		};
		uint16_t BC;
	};

	union {
		struct {
			uint8_t E;
			uint8_t D;
		};
		uint16_t DE;
	};

	union {
		struct {
			uint8_t L;
			uint8_t H;
		};
		uint16_t HL;
	};

	uint16_t SP;
	uint16_t PC;
} registers_t;

typedef struct {
	uint8_t ram[65536];
} memory_t;

typedef uint8_t (*gb_memory_controls_t)(uint8_t *, uint8_t *);

/* Public function prototypes -----------------------------------------------*/
void gb_memory_set_control_function(gb_memory_controls_t controls);
const uint8_t *gb_memory_get_rom_pointer(void);
void gb_memory_load(const void *data, uint32_t size);
void gb_memory_write(uint16_t address, uint8_t data);
void gb_memory_write_short(uint16_t address, uint16_t data);
uint8_t gb_memory_read(uint16_t address);
uint16_t gb_memory_read_short(uint16_t address);
void gb_memory_inc_timers(uint8_t duration);
void gb_memory_set_bit(uint16_t address, uint8_t bit);
void gb_memory_reset_bit(uint16_t address, uint8_t bit);
void gb_memory_print(void);
void gb_memory_set_op(uint8_t op);
void gb_memory_init(const uint8_t *boot_rom, const uint8_t *game_rom);

#endif /* INCLUDE_GB_MEMORY_H_ */
