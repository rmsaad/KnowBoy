/**
 * @file gb_apu.c
 * @brief Gameboy Audio Functionality.
 *
 * This file emulates all functionality of the gameboy audio.
 *
 * @author Rami Saad
 * @date 2021-07-11
 */

#include "gb_apu.h"
#include "gb_common.h"
#include "gb_memory.h"
#include "logging.h"

#include <string.h>

// Audio buffers
static uint16_t *gb_apu_buf = NULL;
static uint16_t *gb_apu_buf_pos = NULL;

// Gameboy memory struct
extern memory_t mem;

static const uint8_t duties[4][8] = {
	{0, 0, 0, 0, 0, 0, 0, 1}, // 00
	{1, 0, 0, 0, 0, 0, 0, 1}, // 01
	{1, 0, 0, 0, 0, 1, 1, 1}, // 10
	{0, 1, 1, 1, 1, 1, 1, 0}  // 11
};

// 4.194 MHZ --> 44100 HZ Conversion (4.194 / 0.0441 = 95)
static uint8_t audio_freq_convert_factor = 95;

// Frame Sequence Step 0 - 7 (Incremented every 8192 T States)
static uint8_t frame_sequence_step = 0;

// Frame Sequence TState (Incremented every T State up to 8192)
static uint16_t frame_sequence_cycle = 0;

// NR10
static uint8_t ch1_sweep_pace = 0;
static uint8_t ch1_sweep_dir = 0;
static int16_t ch1_sweep_step = 0;
static uint8_t ch1_sweep_enable = 0;
static uint8_t ch1_sweep_timer = 0;
static uint32_t ch1_sweep_shadow = 0;
static int16_t ch1_sweep_negate = 0;

// NR11
static uint8_t ch1_wave_duty = 0;
static uint8_t ch1_length_counter = 0;
static uint8_t ch1_duty_pos = 0;

// NR12
static uint8_t ch1_init_vol = 0;
static uint8_t ch1_envelope_dir = 0;
static uint8_t ch1_envelope_pace = 0;
static bool ch1_dac_on = false;
static uint16_t ch1_volume = 0;
static uint8_t ch1_envelope = 0;

// NR13
static uint16_t ch1_freq = 0;
static int32_t ch1_timer = 0;

// NR14
static uint8_t ch1_len_enable = 0;

// NR21
static uint8_t ch2_wave_duty = 0;
static uint8_t ch2_length_counter = 0;
static uint8_t ch2_duty_pos = 0;

// NR22
static uint8_t ch2_init_vol = 0;
static uint8_t ch2_envelope_dir = 0;
static uint8_t ch2_envelope_pace = 0;
static bool ch2_dac_on = false;
static uint16_t ch2_volume = 0;
static uint8_t ch2_envelope = 0;

// NR23
static uint16_t ch2_freq = 0;
static int32_t ch2_timer = 0;

// NR24
static uint8_t ch2_len_enable = 0;

// NR30
static bool ch3_dac_on = false;

// NR31
static uint16_t ch3_length_counter = 0;
static uint8_t ch3_wave_pos = 0;

// NR32
static uint8_t ch3_output_lvl = 0;
static uint16_t ch3_volume = 0;
static uint8_t ch3_envelope = 0;

// NR33
static uint16_t ch3_freq = 0;
static int32_t ch3_timer = 0;
static bool ch3_wave_avail = false;

// NR34
static uint8_t ch3_len_enable = 0;

// NR41
static uint8_t ch4_length_counter = 0;

// NR42
static uint8_t ch4_init_vol = 0;
static uint8_t ch4_envelope_dir = 0;
static uint8_t ch4_envelope_pace = 0;
static bool ch4_dac_on = false;
static uint16_t ch4_volume = 0;
static uint8_t ch4_envelope = 0;

// NR43
static uint8_t ch4_clock_shift = 0;
static uint8_t ch4_lfsr_width = 0;
static uint8_t ch4_clock_div = 0;
static int32_t ch4_timer = 0;
static const uint8_t ch4_divisor[8] = {8, 16, 32, 48, 64, 80, 96, 112};
static uint16_t ch4_lfsr = 0;

// NR44
static uint8_t ch4_len_enable = 0;

static void gb_apu_step_ch1(void)
{

	// length
	if (frame_sequence_step % 2 == 0 && ch1_len_enable && ch1_length_counter) {
		ch1_length_counter--;
		if (ch1_length_counter <= 0) {
			mem.map[NR52_ADDR] &= ~CH1_ON;
		}
	}

	//  handle sweep (frequency sweep)
	if (frame_sequence_step == 2 || frame_sequence_step == 6) {

		if (ch1_sweep_timer > 0)
			--ch1_sweep_timer;

		if (ch1_sweep_timer == 0) {

			ch1_sweep_timer = ch1_sweep_pace;

			if (ch1_sweep_pace == 0)
				ch1_sweep_timer = 8;

			if (ch1_sweep_enable && ch1_sweep_pace) {

				ch1_sweep_negate = ch1_sweep_dir ? -1 : 1;

				uint32_t newfreq =
					ch1_sweep_shadow +
					((ch1_sweep_shadow >> ch1_sweep_step) * ch1_sweep_negate);

				if (newfreq < 2048 && ch1_sweep_step) {
					ch1_sweep_shadow = newfreq;
					ch1_freq = newfreq;
					mem.map[NR13_ADDR] = ch1_sweep_shadow & 0xff;
					mem.map[NR14_ADDR] = (mem.map[NR14_ADDR] & ~0x07) |
							     ((ch1_sweep_shadow >> 8) & 0x07);

					if ((ch1_sweep_shadow +
					     ((ch1_sweep_shadow >> ch1_sweep_step) *
					      ch1_sweep_negate)) > 2047) {
						ch1_sweep_enable = 0;
						mem.map[NR52_ADDR] &= ~CH1_ON;
					}
				}

				if ((newfreq > 2047) ||
				    ((ch1_sweep_shadow + ((ch1_sweep_shadow >> ch1_sweep_step) *
							  ch1_sweep_negate)) > 2047)) {
					ch1_sweep_enable = 0;
					mem.map[NR52_ADDR] &= ~CH1_ON;
				}
			}
		}
	}

	//  handle envelope (volume envelope)
	if (frame_sequence_step == 7 && ch1_dac_on && ch1_envelope_pace) {
		--ch1_envelope;

		if (ch1_envelope <= 0) {
			ch1_envelope = ch1_envelope_pace;

			int8_t vol = ch1_volume + (ch1_envelope_dir ? 1 : -1);

			if (vol >= 0x0 && vol <= 0xF) {
				ch1_volume = vol;
			}
		}
	}
}

static void gb_apu_step_ch2(void)
{
	// length
	if (frame_sequence_step % 2 == 0 && ch2_len_enable && ch2_length_counter) {
		ch2_length_counter--;
		if (ch2_length_counter <= 0) {
			mem.map[NR52_ADDR] &= ~CH2_ON;
		}
	}

	// envelope
	if (frame_sequence_step == 7 && ch2_dac_on && ch2_envelope_pace) {
		ch2_envelope--;

		if (ch2_envelope <= 0) {

			ch2_envelope = ch2_envelope_pace;

			// get louder or quieter
			int8_t vol = ch2_volume + ((ch2_envelope_dir) ? 1 : -1);

			if (vol >= 0x0 && vol <= 0xF) {
				ch2_volume = vol;
			}
		}
	}
}

static void gb_apu_step_ch3(void)
{

	//  handle length
	if (frame_sequence_step % 2 == 0 && ch3_len_enable && ch3_length_counter) {
		ch3_length_counter--;
		if (ch3_length_counter == 0) {
			mem.map[NR52_ADDR] &= ~CH3_ON;
		}
	}
}

static void gb_apu_step_ch4(void)
{

	//  handle length
	if (frame_sequence_step % 2 == 0 && ch4_len_enable && ch4_length_counter) {
		ch4_length_counter--;
		if (ch4_length_counter == 0) {
			mem.map[NR52_ADDR] &= ~CH4_ON;
		}
	}

	//  handle envelope (volume envelope)
	if (frame_sequence_step == 7 && ch4_envelope_pace) {
		--ch4_envelope;

		if (ch4_envelope <= 0) {

			ch4_envelope = ch4_envelope_pace;

			int8_t vol = ch4_volume + ((ch4_envelope_dir) ? 1 : -1);

			if (vol >= 0x0 && vol <= 0xF)
				ch4_volume = vol;
		}
	}
}

void gb_apu_init(uint16_t *buf, uint16_t *buf_pos, uint16_t buf_size)
{
	gb_apu_buf = buf;
	gb_apu_buf_pos = buf_pos;
	memset(gb_apu_buf, 0x00, buf_size);
}

void gb_apu_step(void)
{
	uint8_t current_cylces = 4;

	while (current_cylces--) {

		ch1_timer--;
		if (ch1_timer <= 0x00) {
			ch1_timer = (2048 - ch1_freq) * 4;
			ch1_duty_pos++;
			ch1_duty_pos %= 8;
		}

		ch2_timer--;
		if (ch2_timer <= 0x00) {
			ch2_timer = (2048 - ch2_freq) * 4;
			ch2_duty_pos++;
			ch2_duty_pos %= 8;
		}

		ch3_timer--;
		if (ch3_timer <= 0x00) {
			ch3_timer = (2048 - ch3_freq) * 2;
			ch3_wave_pos++;
			ch3_wave_pos %= 32;
			ch3_wave_avail = true;
		}

		ch4_timer--;
		if (ch4_timer <= 0x00) {
			ch4_timer = ch4_divisor[ch4_clock_div] << ch4_clock_shift;

			//  handle lfsr
			uint8_t xor_res = (ch4_lfsr & 0x1) ^ ((ch4_lfsr & 0x2) >> 1);
			ch4_lfsr >>= 1;
			ch4_lfsr |= (xor_res << 14);
			if (ch4_lfsr_width) {
				ch4_lfsr |= (xor_res << 6);
				ch4_lfsr &= 0x7F;
			}
		}

		// FS Step
		frame_sequence_cycle++;
		if (frame_sequence_cycle == 8192) {
			frame_sequence_cycle = 0;

			frame_sequence_step++;
			frame_sequence_step %= 8;

			gb_apu_step_ch1();
			gb_apu_step_ch2();
			gb_apu_step_ch3();
			gb_apu_step_ch4();
		}

		// 95
		if (!--audio_freq_convert_factor) {
			audio_freq_convert_factor = 95;

			gb_apu_buf[*gb_apu_buf_pos] = 0;
			gb_apu_buf[*gb_apu_buf_pos + 1] = 0;

			if (mem.map[NR52_ADDR] & AUDIO_ON) {

				// ch1
				if (mem.map[NR52_ADDR] & CH1_ON) {

					if (mem.map[NR51_ADDR] & CH1_LEFT) {
						gb_apu_buf[*gb_apu_buf_pos] =
							((duties[ch1_wave_duty][ch1_duty_pos] == 1)
								 ? ch1_volume
								 : 0);
					}

					if ((mem.map[NR51_ADDR] & CH1_RIGHT)) {
						gb_apu_buf[*gb_apu_buf_pos + 1] =
							((duties[ch1_wave_duty][ch1_duty_pos] == 1)
								 ? ch1_volume
								 : 0);
					}
				}

				// ch2
				if (mem.map[NR52_ADDR] & CH2_ON) {

					if (mem.map[NR51_ADDR] & CH2_LEFT) {
						gb_apu_buf[*gb_apu_buf_pos] +=
							((duties[ch2_wave_duty][ch2_duty_pos] == 1)
								 ? ch2_volume
								 : 0);
					}
					if (mem.map[NR51_ADDR] & CH2_RIGHT) {
						gb_apu_buf[*gb_apu_buf_pos + 1] +=
							((duties[ch2_wave_duty][ch2_duty_pos] == 1)
								 ? ch2_volume
								 : 0);
					}
				}

				// ch3
				if ((mem.map[NR52_ADDR] & CH3_ON)) {

					uint8_t wave = mem.map[WPRAM_BASE + (ch3_wave_pos / 2)];

					if (ch3_wave_pos % 2) {
						wave = wave & 0xf;
					} else {
						wave = wave >> 4;
					}

					if (ch3_output_lvl)
						wave = wave >> (ch3_output_lvl - 1);
					else
						wave = wave >> 4;

					if (mem.map[NR51_ADDR] & CH3_LEFT) {
						gb_apu_buf[*gb_apu_buf_pos] += wave;
					}

					if (mem.map[NR51_ADDR] & CH3_RIGHT) {
						gb_apu_buf[*gb_apu_buf_pos + 1] += wave;
					}
				}

				// ch4
				if ((mem.map[NR52_ADDR] & CH4_ON)) {

					if (mem.map[NR51_ADDR] & CH4_LEFT) {
						gb_apu_buf[*gb_apu_buf_pos] +=
							((ch4_lfsr & 0x1) ? ch4_volume : 0);
					}
					if (mem.map[NR51_ADDR] & CH4_RIGHT) {
						gb_apu_buf[*gb_apu_buf_pos + 1] +=
							((ch4_lfsr & 0x1) ? ch4_volume : 0);
					}
				}

				gb_apu_buf[*gb_apu_buf_pos] <<=
					((mem.map[NR50_ADDR] & VOL_LEFT) >> VOL_LEFT_OFFSET);

				gb_apu_buf[*gb_apu_buf_pos + 1] <<=
					((mem.map[NR50_ADDR] & VOL_RIGHT) >> VOL_RIGHT_OFFSET);
			}
			*gb_apu_buf_pos += 2;
		}
	}
}

static void gb_apu_set_dac_ch1(uint8_t dac_mask)
{
	ch1_dac_on = (dac_mask != 0) ? true : false;
	if (!ch1_dac_on) {
		mem.map[NR52_ADDR] &= ~CH1_ON;
	}
}

static void gb_apu_set_dac_ch2(uint8_t dac_mask)
{
	ch2_dac_on = (dac_mask != 0) ? true : false;
	if (!ch2_dac_on) {
		mem.map[NR52_ADDR] &= ~CH2_ON;
	}
}

static void gb_apu_set_dac_ch3(uint8_t dac_mask)
{
	ch3_dac_on = (dac_mask != 0) ? true : false;
	if (!ch3_dac_on) {
		mem.map[NR52_ADDR] &= ~CH3_ON;
	}
}

static void gb_apu_set_dac_ch4(uint8_t dac_mask)
{
	ch4_dac_on = (dac_mask != 0) ? true : false;
	if (!ch4_dac_on) {
		mem.map[NR52_ADDR] &= ~CH4_ON;
	}
}

static void gb_apu_update_ch1_counter(void)
{
	if (ch1_length_counter != 0 && frame_sequence_step % 2 == 0) {
		ch1_length_counter--;
	}

	if (ch1_length_counter == 0) {
		mem.map[NR52_ADDR] &= ~CH1_ON;
	}
}

static void gb_apu_update_ch2_counter(void)
{
	if (ch2_length_counter != 0 && frame_sequence_step % 2 == 0) {
		ch2_length_counter--;
	}

	if (ch2_length_counter == 0) {
		mem.map[NR52_ADDR] &= ~CH2_ON;
	}
}

static void gb_apu_update_ch3_counter(void)
{
	if (ch3_length_counter != 0 && frame_sequence_step % 2 == 0) {
		ch3_length_counter--;
	}

	if (ch3_length_counter == 0) {
		mem.map[NR52_ADDR] &= ~CH3_ON;
	}
}

static void gb_apu_update_ch4_counter(void)
{
	if (ch4_length_counter != 0 && frame_sequence_step % 2 == 0) {
		ch4_length_counter--;
	}

	if (ch4_length_counter == 0) {
		mem.map[NR52_ADDR] &= ~CH4_ON;
	}
}

static void gb_apu_trigger_ch1(void)
{
	if (ch1_dac_on) {
		mem.map[NR52_ADDR] |= CH1_ON;
	}

	if (ch1_length_counter == 0) {
		ch1_length_counter = 64;
		if (ch1_len_enable && frame_sequence_step % 2 == 0) {
			ch1_length_counter--;
		}
	}

	ch1_timer = (2048 - ch1_freq) * 4;
	ch1_sweep_shadow = ch1_freq;
	ch1_envelope = ch1_envelope_pace;
	ch1_volume = ch1_init_vol;

	ch1_sweep_timer = ch1_sweep_pace;

	if (ch1_sweep_pace == 0)
		ch1_sweep_timer = 8;

	ch1_sweep_negate = 1;

	if (ch1_sweep_pace || ch1_sweep_step)
		ch1_sweep_enable = 1;
	else
		ch1_sweep_enable = 0;

	if (ch1_sweep_step) {

		ch1_sweep_negate = (ch1_sweep_dir) ? -1 : 1;

		uint32_t newfreq = ch1_sweep_shadow +
				   ((ch1_sweep_shadow >> ch1_sweep_step) * ch1_sweep_negate);

		if (newfreq > 2047) {
			mem.map[NR52_ADDR] &= ~CH1_ON;
			ch1_sweep_enable = 0;
		}
	}
}

static void gb_apu_trigger_ch2(void)
{
	if (ch2_dac_on) {
		mem.map[NR52_ADDR] |= CH2_ON;
	}

	if (ch2_length_counter == 0) {
		ch2_length_counter = 64;
		if (ch2_len_enable && frame_sequence_step % 2 == 0) {
			ch2_length_counter--;
		}
	}

	ch2_timer = (2048 - ch2_freq) * 4;
	ch2_envelope = ch2_envelope_pace;
	ch2_volume = ch2_init_vol;
}

static void gb_apu_trigger_ch3(void)
{
	if (ch3_dac_on) {
		mem.map[NR52_ADDR] |= CH3_ON;
	}

	if (ch3_length_counter == 0) {
		ch3_length_counter = 256;
		if (ch3_len_enable && frame_sequence_step % 2 == 0) {
			ch3_length_counter--;
		}
	}

	if (ch3_timer == 4 && ch3_wave_avail) {
		if ((ch3_wave_pos >> 1) <= 0x3) {
			mem.map[WPRAM_BASE + 0x0] = mem.map[WPRAM_BASE + (ch3_wave_pos >> 1)];
		} else if ((ch3_wave_pos >> 1) <= 0x7) {
			mem.map[WPRAM_BASE + 0x0] = mem.map[WPRAM_BASE + 0x4];
			mem.map[WPRAM_BASE + 0x1] = mem.map[WPRAM_BASE + 0x5];
			mem.map[WPRAM_BASE + 0x2] = mem.map[WPRAM_BASE + 0x6];
			mem.map[WPRAM_BASE + 0x3] = mem.map[WPRAM_BASE + 0x7];
		} else if ((ch3_wave_pos >> 1) <= 0xB) {
			mem.map[WPRAM_BASE + 0x0] = mem.map[WPRAM_BASE + 0x8];
			mem.map[WPRAM_BASE + 0x1] = mem.map[WPRAM_BASE + 0x9];
			mem.map[WPRAM_BASE + 0x2] = mem.map[WPRAM_BASE + 0xA];
			mem.map[WPRAM_BASE + 0x3] = mem.map[WPRAM_BASE + 0xB];
		} else if ((ch3_wave_pos >> 1) <= 0xF) {
			mem.map[WPRAM_BASE + 0x0] = mem.map[WPRAM_BASE + 0xC];
			mem.map[WPRAM_BASE + 0x1] = mem.map[WPRAM_BASE + 0xD];
			mem.map[WPRAM_BASE + 0x2] = mem.map[WPRAM_BASE + 0xE];
			mem.map[WPRAM_BASE + 0x3] = mem.map[WPRAM_BASE + 0xF];
		}
	}

	ch3_timer = (2048 - ch3_freq) * 2;
	ch3_timer += 4;
	ch3_wave_pos = 0;
	ch3_wave_avail = false;
}

static void gb_apu_trigger_ch4(void)
{
	if (ch4_dac_on) {
		mem.map[NR52_ADDR] |= CH4_ON;
	}

	if (ch4_length_counter == 0) {
		ch4_length_counter = 64;
		if (ch4_len_enable && frame_sequence_step % 2 == 0) {
			ch4_length_counter--;
		}
	}

	ch4_timer = ch4_divisor[ch4_clock_div] << ch4_clock_shift;
	ch4_lfsr = 0x7fff;
	ch4_envelope = ch4_envelope_pace;
	ch4_volume = ch4_init_vol;
}

void gb_apu_reset(void)
{
	ch1_sweep_pace = 0;
	ch1_sweep_dir = 0;
	ch1_sweep_step = 0;
	ch1_sweep_enable = 0;
	ch1_sweep_timer = 0;
	ch1_sweep_shadow = 0;
	ch1_sweep_negate = 0;
	ch1_wave_duty = 0;
	ch1_duty_pos = 0;
	ch1_init_vol = 0;
	ch1_envelope_dir = 0;
	ch1_envelope_pace = 0;
	ch1_dac_on = false;
	ch1_volume = 0;
	ch1_envelope = 0;
	ch1_freq = 0;
	ch1_timer = 0;
	ch1_len_enable = 0;

	ch2_wave_duty = 0;
	ch2_duty_pos = 0;
	ch2_init_vol = 0;
	ch2_envelope_dir = 0;
	ch2_envelope_pace = 0;
	ch2_dac_on = false;
	ch2_volume = 0;
	ch2_envelope = 0;
	ch2_freq = 0;
	ch2_timer = 0;
	ch2_len_enable = 0;

	ch3_dac_on = false;
	ch3_wave_pos = 0;
	ch3_output_lvl = 0;
	ch3_volume = 0;
	ch3_envelope = 0;
	ch3_freq = 0;
	ch3_timer = 0;
	ch3_wave_avail = false;
	ch3_len_enable = 0;

	ch4_init_vol = 0;
	ch4_envelope_dir = 0;
	ch4_envelope_pace = 0;
	ch4_dac_on = false;
	ch4_volume = 0;
	ch4_envelope = 0;
	ch4_clock_shift = 0;
	ch4_lfsr_width = 0;
	ch4_clock_div = 0;
	ch4_timer = 0;
	ch4_lfsr = 0;
	ch4_len_enable = 0;

	// ch1_length_counter = 0;
	// ch2_length_counter = 0;
	// ch3_length_counter = 0;
	// ch4_length_counter = 0;
}

uint8_t gb_apu_memory_read(uint16_t address)
{
	switch (address) {
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
		if ((mem.map[NR52_ADDR] & CH3_ON)) {
			if (ch3_timer == 2 && ch3_wave_avail) {
				return mem.map[WPRAM_BASE + (ch3_wave_pos >> 1)];
			} else {
				return 0xFF;
			}

		} else {
			return 0x00 | mem.map[address];
		}
	default:
		return mem.map[address];
	}
}

void gb_apu_memory_write(uint16_t address, uint8_t data)
{
	if (address >= NR10_ADDR && address < WPRAM_BASE) {
		bool apu_power = CHK_BIT(mem.map[NR52_ADDR], AUDIO_ON_OFFSET);

		switch (address) {

		case NR10_ADDR:
			ch1_sweep_pace = (data & CH1_SWEEP_PACE) >> CH1_SWEEP_PACE_OFFSET;
			ch1_sweep_dir = (data & CH1_SWEEP_DIR) >> CH1_SWEEP_DIR_OFFSET;
			ch1_sweep_step = (data & CH1_SWEEP_STEP) >> CH1_SWEEP_STEP_OFFSET;
			if (apu_power) {
				if (ch1_sweep_negate == -1 && !ch1_sweep_dir) {
					mem.map[NR52_ADDR] &= ~CH1_ON;
				}
				mem.map[address] = data;
			}
			return;

		case NR11_ADDR:
			ch1_wave_duty = (data & CH1_WAVE_DUTY) >> CH1_WAVE_DUTY_OFFSET;
			ch1_length_counter = 64 - (data & CH1_INITIAL_LEN_TIMER);
			mem.map[address] = (apu_power) ? data : data & CH1_INITIAL_LEN_TIMER;
			return;

		case NR12_ADDR:
			if (apu_power) {
				ch1_init_vol = (data & CH1_INITIAL_VOL) >> CH1_INITIAL_VOL_OFFSET;
				ch1_envelope_dir = (data & CH1_ENV_DIR) >> CH1_ENV_DIR_OFFSET;
				ch1_envelope_pace = (data & CH1_ENV_PACE) >> CH1_ENV_PACE_OFFSET;
				gb_apu_set_dac_ch1(data & (CH1_ENV_DIR + CH1_INITIAL_VOL));
				mem.map[address] = data;
			}
			return;

		case NR13_ADDR:
			if (apu_power) {
				ch1_freq = (ch1_freq & ~0x00FF) | (data & CH1_PERIOD_LOW);
				mem.map[address] = data;
			}
			return;

		case NR14_ADDR:
			if (apu_power) {
				bool ch1_len_on = !ch1_len_enable && (data & CH1_LEN_EN);
				ch1_len_enable = (data & CH1_LEN_EN) >> CH1_LEN_EN_OFFSET;
				ch1_freq = (ch1_freq & ~0xFF00) | ((data & CH1_PERIOD_HIGH) << 8);
				if (ch1_len_on) {
					gb_apu_update_ch1_counter();
				}
				if (CHK_BIT(data, 7)) {
					gb_apu_trigger_ch1();
				}
				mem.map[address] = data;
			}
			return;

		case NR21_ADDR:
			ch2_wave_duty = (data & CH2_WAVE_DUTY) >> CH2_WAVE_DUTY_OFFSET;
			ch2_length_counter = 64 - (data & CH2_INITIAL_LEN_TIMER);
			mem.map[address] = (apu_power) ? data : data & CH2_INITIAL_LEN_TIMER;
			return;

		case NR22_ADDR:
			if (apu_power) {
				ch2_init_vol = (data & CH2_INITIAL_VOL) >> CH2_INITIAL_VOL_OFFSET;
				ch2_envelope_dir = (data & CH2_ENV_DIR) >> CH2_ENV_DIR_OFFSET;
				ch2_envelope_pace = (data & CH2_ENV_PACE) >> CH2_ENV_PACE_OFFSET;
				gb_apu_set_dac_ch2(data & (CH2_ENV_DIR + CH2_INITIAL_VOL));
				mem.map[address] = data;
			}
			return;

		case NR23_ADDR:
			if (apu_power) {
				ch2_freq = (ch2_freq & ~0x00FF) | (data & CH2_PERIOD_LOW);
				mem.map[address] = data;
			}
			return;

		case NR24_ADDR:
			if (apu_power) {
				bool ch2_len_on = !ch2_len_enable && (data & CH2_LEN_EN);
				ch2_len_enable = (data & CH2_LEN_EN) >> CH2_LEN_EN_OFFSET;
				ch2_freq = (ch2_freq & ~0xFF00) | ((data & CH2_PERIOD_HIGH) << 8);
				if (ch2_len_on) {
					gb_apu_update_ch2_counter();
				}
				if (CHK_BIT(data, 7)) {
					gb_apu_trigger_ch2();
				}
				mem.map[address] = data;
			}
			return;

		case NR30_ADDR:
			if (apu_power) {
				gb_apu_set_dac_ch3(data & CH3_DAC_ON);
				mem.map[address] = data;
			}
			return;

		case NR31_ADDR:
			ch3_length_counter = 256 - (data & CH3_INITIAL_LEN_TIMER);
			mem.map[address] = data;
			return;

		case NR32_ADDR:
			if (apu_power) {
				ch3_output_lvl = (data & CH3_OUTPUT_LVL) >> CH3_OUTPUT_LVL_OFFSET;
				mem.map[address] = data;
			}
			return;

		case NR33_ADDR:
			if (apu_power) {
				ch3_freq = (ch3_freq & ~0x00FF) | (data & CH3_PERIOD_LOW);
				mem.map[address] = data;
			}
			return;

		case NR34_ADDR:
			if (apu_power) {
				bool ch3_len_on = !ch3_len_enable && (data & CH3_LEN_EN);
				ch3_len_enable = (data & CH3_LEN_EN) >> CH3_LEN_EN_OFFSET;
				ch3_freq = (ch3_freq & ~0xFF00) | ((data & CH3_PERIOD_HIGH) << 8);
				if (ch3_len_on) {
					gb_apu_update_ch3_counter();
				}
				if (CHK_BIT(data, 7)) {
					gb_apu_trigger_ch3();
				}
				mem.map[address] = data;
			}
			return;

		case NR41_ADDR:
			ch4_length_counter = 64 - (data & CH4_INITIAL_LEN_TIMER);
			mem.map[address] = data;
			return;

		case NR42_ADDR:
			if (apu_power) {
				ch4_init_vol = (data & CH4_INITIAL_VOL) >> CH4_INITIAL_VOL_OFFSET;
				ch4_envelope_dir = (data & CH4_ENV_DIR) >> CH4_ENV_DIR_OFFSET;
				ch4_envelope_pace = (data & CH4_ENV_PACE) >> CH4_ENV_PACE_OFFSET;
				gb_apu_set_dac_ch4(data & (CH4_ENV_DIR + CH4_INITIAL_VOL));
				mem.map[address] = data;
			}
			return;

		case NR43_ADDR:
			if (apu_power) {
				ch4_clock_shift = (data & CH4_CLK_SHIFT) >> CH4_CLK_SHIFT_OFFSET;
				ch4_lfsr_width = (data & CH4_LFSR_WIDTH) >> CH4_LFSR_WIDTH_OFFSET;
				ch4_clock_div = (data & CH4_CLK_DIV) >> CH4_CLK_DIV_OFFSET;
				mem.map[address] = data;
			}
			return;

		case NR44_ADDR:
			if (apu_power) {
				bool ch4_len_on = !ch4_len_enable && (data & CH4_LEN_EN);
				ch4_len_enable = (data & CH4_LEN_EN) >> CH4_LEN_EN_OFFSET;
				mem.map[address] = data;
				if (ch4_len_on) {
					gb_apu_update_ch4_counter();
				}
				if (CHK_BIT(data, 7)) {
					gb_apu_trigger_ch4();
				}
			}
			return;

		case NR52_ADDR:
			bool apu_power_on = CHK_BIT(data, AUDIO_ON_OFFSET) && !apu_power;
			bool apu_power_off = !CHK_BIT(data, AUDIO_ON_OFFSET) && apu_power;
			if (apu_power_on) {
				frame_sequence_step = 7;
				mem.map[address] |= AUDIO_ON;
			} else if (apu_power_off) {
				memset(&mem.map[NR10_ADDR], 0x00, (NR52_ADDR - NR10_ADDR) + 1);
				gb_apu_reset();
			}
			return;

		default:
			if (apu_power) {
				mem.map[address] = data;
			}
			return;
		}

	} else if (address >= WPRAM_BASE && address < LCDC_ADDR) {
		if ((mem.map[NR52_ADDR] & CH3_ON)) {
			if (ch3_timer == 2 && ch3_wave_avail) {
				mem.map[WPRAM_BASE + (ch3_wave_pos >> 1)] = data;
				return;
			} else {
				return;
			}

		} else {
			mem.map[address] = data;
			return;
		}
	}
}
