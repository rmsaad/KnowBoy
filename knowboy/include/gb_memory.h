/**
 * @file gb_memory.h
 * @brief API and MACROs related to the gameboy memory map.
 *
 * @author Rami Saad
 * @date 2021-03-28
 */

#ifndef INCLUDE_GB_MEMORY_H_
#define INCLUDE_GB_MEMORY_H_

#include <stdbool.h>
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

#define BIT_POS_0 (0x01 << 0)
#define BIT_POS_1 (0x01 << 1)
#define BIT_POS_2 (0x01 << 2)
#define BIT_POS_3 (0x01 << 3)
#define BIT_POS_4 (0x01 << 4)
#define BIT_POS_5 (0x01 << 5)
#define BIT_POS_6 (0x01 << 6)
#define BIT_POS_7 (0x01 << 7)

/* NR52 */
#define CH1_ON	 BIT_POS_0
#define CH2_ON	 BIT_POS_1
#define CH3_ON	 BIT_POS_2
#define CH4_ON	 BIT_POS_3
#define AUDIO_ON BIT_POS_7

/* NR51 */
#define CH1_RIGHT BIT_POS_0
#define CH2_RIGHT BIT_POS_1
#define CH3_RIGHT BIT_POS_2
#define CH4_RIGHT BIT_POS_3
#define CH1_LEFT  BIT_POS_4
#define CH2_LEFT  BIT_POS_5
#define CH3_LEFT  BIT_POS_6
#define CH4_LEFT  BIT_POS_7

/* NR50 */
#define VOL_RIGHT_OFFSET 0
#define VOL_LEFT_OFFSET	 4
#define VOL_RIGHT	 (BIT_POS_0 + BIT_POS_1 + BIT_POS_2)
#define VIN_RIGHT	 BIT_POS_3
#define VOL_LEFT	 (BIT_POS_4 + BIT_POS_5 + BIT_POS_6)
#define VIN_LEFT	 BIT_POS_7

/* NR10 */
#define CH1_PERIOD_SWEEP_STEP_OFFSET 0
#define CH1_PERIOD_SWEEP_DIR_OFFSET  3
#define CH1_PERIOD_SWEEP_PACE_OFFSET 4
#define CH1_PERIOD_SWEEP_STEP	     (BIT_POS_0 + BIT_POS_1 + BIT_POS_2)
#define CH1_PERIOD_SWEEP_DIR	     BIT_POS_3
#define CH1_PERIOD_SWEEP_PACE	     (BIT_POS_4 + BIT_POS_5 + BIT_POS_6)

/* NR11 */
#define CH1_INITIAL_LEN_TIMER_OFFSET 0
#define CH1_WAVE_DUTY_OFFSET	     6
#define CH1_INITIAL_LEN_TIMER                                                                      \
	(BIT_POS_0 + BIT_POS_1 + BIT_POS_2 + BIT_POS_3 + BIT_POS_4 + BIT_POS_5)
#define CH1_WAVE_DUTY (BIT_POS_6 + BIT_POS_7)

/* NR12 */
#define CH1_SWEEP_PACE_OFFSET	0
#define CH1_ENVELOPE_DIR_OFFSET 3
#define CH1_INITIAL_VOL_OFFSET	4
#define CH1_SWEEP_PACE		(BIT_POS_0 + BIT_POS_1 + BIT_POS_2)
#define CH1_ENVELOPE_DIR	(BIT_POS_3)
#define CH1_INITIAL_VOL		(BIT_POS_4 + BIT_POS_5 + BIT_POS_6 + BIT_POS_7)

/* NR13 */
#define CH1_PERIOD_LOW_OFFSET 0
#define CH1_PERIOD_LOW                                                                             \
	(BIT_POS_0 + BIT_POS_1 + BIT_POS_2 + BIT_POS_3 + BIT_POS_4 + BIT_POS_5 + BIT_POS_6 +       \
	 BIT_POS_7)

/* NR14 */
#define CH1_PERIOD_HIGH_OFFSET 0
#define CH1_LEN_EN_OFFSET      6
#define CH1_TRIGGER_OFFSET     7
#define CH1_PERIOD_HIGH	       (BIT_POS_0 + BIT_POS_1 + BIT_POS_2)
#define CH1_LEN_EN	       BIT_POS_6
#define CH1_TRIGGER	       BIT_POS_7

/* NR21 */
#define CH2_INITIAL_LEN_TIMER_OFFSET 0
#define CH2_WAVE_DUTY_OFFSET	     6
#define CH2_INITIAL_LEN_TIMER                                                                      \
	(BIT_POS_0 + BIT_POS_1 + BIT_POS_2 + BIT_POS_3 + BIT_POS_4 + BIT_POS_5)
#define CH2_WAVE_DUTY (BIT_POS_6 + BIT_POS_7)

/* NR22 */
#define CH2_SWEEP_PACE_OFFSET	0
#define CH2_ENVELOPE_DIR_OFFSET 3
#define CH2_INITIAL_VOL_OFFSET	4
#define CH2_SWEEP_PACE		(BIT_POS_0 + BIT_POS_1 + BIT_POS_2)
#define CH2_ENVELOPE_DIR	(BIT_POS_3)
#define CH2_INITIAL_VOL		(BIT_POS_4 + BIT_POS_5 + BIT_POS_6 + BIT_POS_7)

/* NR23 */
#define CH2_PERIOD_LOW_OFFSET 0
#define CH2_PERIOD_LOW                                                                             \
	(BIT_POS_0 + BIT_POS_1 + BIT_POS_2 + BIT_POS_3 + BIT_POS_4 + BIT_POS_5 + BIT_POS_6 +       \
	 BIT_POS_7)

/* NR24 */
#define CH2_PERIOD_HIGH_OFFSET 0
#define CH2_LEN_EN_OFFSET      6
#define CH2_TRIGGER_OFFSET     7
#define CH2_PERIOD_HIGH	       (BIT_POS_0 + BIT_POS_1 + BIT_POS_2)
#define CH2_LEN_EN	       BIT_POS_6
#define CH2_TRIGGER	       BIT_POS_7

/* NR30 */
#define CH3_DAQ_ON_OFFSET 7
#define CH3_DAQ_ON	  BIT_POS_7

/* NR31 */
#define CH3_INITIAL_LEN_TIMER_OFFSET 0
#define CH3_INITIAL_LEN_TIMER                                                                      \
	(BIT_POS_0 + BIT_POS_1 + BIT_POS_2 + BIT_POS_3 + BIT_POS_4 + BIT_POS_5 + BIT_POS_6 +       \
	 BIT_POS_7)

/* NR32 */
#define CH3_OUTPUT_LEVEL_OFFSET 5
#define CH3_OUTPUT_LEVEL	(BIT_POS_5 + BIT_POS_6)

/* NR33*/
#define CH3_PERIOD_LOW_OFFSET 0
#define CH3_PERIOD_LOW                                                                             \
	(BIT_POS_0 + BIT_POS_1 + BIT_POS_2 + BIT_POS_3 + BIT_POS_4 + BIT_POS_5 + BIT_POS_6 +       \
	 BIT_POS_7)

/* NR34*/
#define CH3_PERIOD_HIGH_OFFSET 0
#define CH3_LEN_EN_OFFSET      6
#define CH3_TRIGGER_OFFSET     7
#define CH3_PERIOD_HIGH	       (BIT_POS_0 + BIT_POS_1 + BIT_POS_2)
#define CH3_LEN_EN	       BIT_POS_6
#define CH3_TRIGGER	       BIT_POS_7

/* NR41 */
#define CH4_INITIAL_LEN_TIMER_OFFSET 0
#define CH4_INITIAL_LEN_TIMER                                                                      \
	(BIT_POS_0 + BIT_POS_1 + BIT_POS_2 + BIT_POS_3 + BIT_POS_4 + BIT_POS_5)

/* NR42 */
#define CH4_SWEEP_PACE_OFFSET	0
#define CH4_ENVELOPE_DIR_OFFSET 3
#define CH4_INITIAL_VOL_OFFSET	4
#define CH4_SWEEP_PACE		(BIT_POS_0 + BIT_POS_1 + BIT_POS_2)
#define CH4_ENVELOPE_DIR	(BIT_POS_3)
#define CH4_INITIAL_VOL		(BIT_POS_4 + BIT_POS_5 + BIT_POS_6 + BIT_POS_7)

/* NR43 */
#define CH4_CLOCK_DIV_OFFSET   0
#define CH4_LFSR_WIDTH_OFFSET  3
#define CH4_CLOCK_SHIFT_OFFSET 4
#define CH4_CLOCK_DIV	       (BIT_POS_0 + BIT_POS_1 + BIT_POS_2)
#define CH4_LFSR_WIDTH	       (BIT_POS_3)
#define CH4_CLOCK_SHIFT	       (BIT_POS_4 + BIT_POS_5 + BIT_POS_6 + BIT_POS_7)

/* NR44 */
#define CH4_LEN_EN_OFFSET  6
#define CH4_TRIGGER_OFFSET 7
#define CH4_LEN_EN	   BIT_POS_6
#define CH4_TRIGGER	   BIT_POS_7

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
void gb_memory_init(const uint8_t *boot_rom, const uint8_t *game_rom, bool boot_skip);

#endif /* INCLUDE_GB_MEMORY_H_ */
