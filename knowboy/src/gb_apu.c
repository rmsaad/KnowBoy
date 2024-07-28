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
#include "gb_memory.h"
#include "logging.h"

#include <string.h>

// Audio buffers
static uint16_t *gb_apu_buf = NULL;
static uint16_t *gb_apu_buf_pos = NULL;

static const uint8_t duties[4][8] = {
	{0, 0, 0, 0, 0, 0, 0, 1}, // 00
	{1, 0, 0, 0, 0, 0, 0, 1}, // 01
	{1, 0, 0, 0, 0, 1, 1, 1}, // 10
	{0, 1, 1, 1, 1, 1, 1, 0}  // 11
};

// Dac Status
static bool ch1_dac_on = false;
static bool ch2_dac_on = false;
static bool ch3_dac_on = false;
static bool ch4_dac_on = false;

// Length Counters
static uint8_t ch1_length_counter = 0;
static uint8_t ch2_length_counter = 0;
static uint16_t ch3_length_counter = 0;
static uint8_t ch4_length_counter = 0;

// frequency timers
static int32_t ch1_timer = 0;
static int32_t ch2_timer = 0;
static int32_t ch3_timer = 0;
static int32_t ch4_timer = 0;

// Volume Envelopes
static uint8_t ch1_envelope = 0;
static uint8_t ch2_envelope = 0;
static uint8_t ch3_envelope = 0;
static uint8_t ch4_envelope = 0;

// Inital Volume of Envelope
static uint16_t ch1_volume = 0;
static uint16_t ch2_volume = 0;
static uint16_t ch3_volume = 0;
static uint16_t ch4_volume = 0;

// 4.194 MHZ --> 44100 HZ Conversion (4.194 / 0.0441 = 95)
static uint8_t audio_freq_convert_factor = 95;

// Frame Sequence Step 0 - 7 (Incremented every 8192 T States)
static uint8_t frame_sequence_step = 0;

// Frame Sequence TState (Incremented every T State up to 8192)
static uint16_t frame_sequence_cycle = 0;

// CH1 & CH2 duty position (0 - 7)
static uint8_t ch1_duty_pos = 0;
static uint8_t ch2_duty_pos = 0;
static uint8_t ch3_wave_pos = 0;

// CH1 Sweep Period, Negate, Shift
static uint8_t ch1_sweep_enable = 0;
static uint8_t ch1_sweep_pace = 0;
static uint8_t ch1_sweep_dir = 0;
static uint32_t ch1_sweep_shadow = 0;
static int16_t ch1_sweep_step = 0;
static int16_t ch1_sweep_negate = 0;

static const uint8_t ch4_divisor[8] = {8, 16, 32, 48, 64, 80, 96, 112};
static uint16_t ch4_lfsr = 0;

extern memory_t mem;

static void gb_apu_step_ch1(void)
{

	// length
	if (frame_sequence_step % 2 == 0 && (mem.map[NR14_ADDR] & CH1_LEN_EN) &&
	    ch1_length_counter) {
		ch1_length_counter--;
		if (ch1_length_counter <= 0) {
			mem.map[NR52_ADDR] &= ~CH1_ON;
		}
	}

	//  handle sweep (frequency sweep)
	ch1_sweep_step =
		(mem.map[NR10_ADDR] & CH1_PERIOD_SWEEP_STEP) >> CH1_PERIOD_SWEEP_STEP_OFFSET;
	ch1_sweep_pace =
		(mem.map[NR10_ADDR] & CH1_PERIOD_SWEEP_PACE) >> CH1_PERIOD_SWEEP_PACE_OFFSET;
	ch1_sweep_dir = (mem.map[NR10_ADDR] & CH1_PERIOD_SWEEP_DIR) >> CH1_PERIOD_SWEEP_DIR_OFFSET;

	if ((frame_sequence_step == 2 || frame_sequence_step == 6) && ch1_sweep_pace &&
	    ch1_sweep_step) {

		//  tick sweep down
		--ch1_sweep_pace;
		if (ch1_sweep_pace <= 0) {

			//  reload sweep
			if (ch1_sweep_pace == 0)
				ch1_sweep_pace = 8;

			if (ch1_sweep_enable) {
				ch1_sweep_negate = ch1_sweep_dir ? -1 : 1;

				uint32_t newfreq =
					ch1_sweep_shadow +
					((ch1_sweep_shadow >> ch1_sweep_step) * ch1_sweep_negate);

				if (newfreq < 2048 && ch1_sweep_step) {
					ch1_sweep_shadow = newfreq;

					mem.map[NR13_ADDR] = ch1_sweep_shadow & 0xff;
					gb_memory_write(NR14_ADDR, (ch1_sweep_shadow >> 8) & 0x07);

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
	if (frame_sequence_step == 7 && ch1_dac_on &&
	    ((mem.map[NR12_ADDR] & CH1_SWEEP_PACE) >> CH1_SWEEP_PACE_OFFSET)) {
		--ch1_envelope;

		if (ch1_envelope <= 0) {
			ch1_envelope =
				(mem.map[NR12_ADDR] & CH1_SWEEP_PACE) >> CH1_SWEEP_PACE_OFFSET;

			int8_t vol =
				ch1_volume + ((mem.map[NR12_ADDR] & CH1_ENVELOPE_DIR) ? 1 : -1);

			if (vol >= 0x0 && vol <= 0xF) {
				ch1_volume = vol;
			}
		}
	}
}

static void gb_apu_step_ch2(void)
{
	// length
	if (frame_sequence_step % 2 == 0 && (mem.map[NR24_ADDR] & CH2_LEN_EN) &&
	    ch2_length_counter) {
		ch2_length_counter--;
		if (ch2_length_counter <= 0) {
			mem.map[NR52_ADDR] &= ~CH2_ON;
		}
	}

	// envelope
	if (frame_sequence_step == 7 && ch2_dac_on &&
	    ((mem.map[NR22_ADDR] & CH2_SWEEP_PACE) >> CH2_SWEEP_PACE_OFFSET)) {
		ch2_envelope--;

		if (ch2_envelope <= 0) {

			ch2_envelope =
				(mem.map[NR22_ADDR] & CH2_SWEEP_PACE) >> CH2_SWEEP_PACE_OFFSET;

			// get louder or quieter
			int8_t vol =
				ch2_volume + ((mem.map[NR22_ADDR] & CH2_ENVELOPE_DIR) ? 1 : -1);

			if (vol >= 0x0 && vol <= 0xF) {
				ch2_volume = vol;
			}
		}
	}
}

static void gb_apu_step_ch3(void)
{

	//  handle length
	if (frame_sequence_step % 2 == 0 && (mem.map[NR34_ADDR] & CH3_LEN_EN) &&
	    ch3_length_counter) {
		ch3_length_counter--;
		if (ch3_length_counter == 0) {
			mem.map[NR52_ADDR] &= ~CH3_ON;
		}
	}
}

static void gb_apu_step_ch4(void)
{

	//  handle length
	if (frame_sequence_step % 2 == 0 && (mem.map[NR44_ADDR] & CH4_LEN_EN) &&
	    ch4_length_counter) {
		ch4_length_counter--;
		if (ch4_length_counter == 0) {
			mem.map[NR52_ADDR] &= ~CH4_ON;
		}
	}

	//  handle envelope (volume envelope)
	if (frame_sequence_step == 7) {
		--ch4_envelope;

		if (ch4_envelope <= 0) {

			ch4_envelope = mem.map[NR42_ADDR] & CH4_SWEEP_PACE >> CH4_SWEEP_PACE_OFFSET;
			if ((mem.map[NR42_ADDR] & CH4_SWEEP_PACE) != 0) {

				int8_t vol = ch4_volume +
					     ((mem.map[NR42_ADDR] & CH4_ENVELOPE_DIR) ? 1 : -1);

				if (vol >= 0x0 && vol <= 0xF)
					ch4_volume = vol;
			}
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

		// check timer
		if (ch1_timer-- <= 0x00) {
			uint16_t freq_x = ((mem.map[NR14_ADDR] & CH1_PERIOD_HIGH) << 8 |
					   mem.map[NR13_ADDR] & CH1_PERIOD_LOW);
			ch1_timer = (2048 - freq_x) * 4;
			ch1_duty_pos++;
			ch1_duty_pos %= 8;
		}

		if (ch2_timer-- <= 0x00) {
			uint16_t freq_x = ((mem.map[NR24_ADDR] & CH2_PERIOD_HIGH) << 8 |
					   mem.map[NR23_ADDR] & CH2_PERIOD_LOW);
			ch2_timer = (2048 - freq_x) * 4;
			ch2_duty_pos++;
			ch2_duty_pos %= 8;
		}

		if (ch3_timer-- <= 0x00) {
			uint16_t freq_x = ((mem.map[NR34_ADDR] & CH3_PERIOD_HIGH) << 8 |
					   mem.map[NR33_ADDR] & CH3_PERIOD_LOW);
			ch3_timer = (2048 - freq_x) * 2;
			ch3_wave_pos++;
			ch3_wave_pos %= 32;
		}

		if (ch4_timer-- <= 0x00) {
			ch4_timer = ch4_divisor[mem.map[NR43_ADDR] & CH4_CLOCK_DIV]
				    << ((mem.map[NR43_ADDR] & CH4_CLOCK_SHIFT) >>
					CH4_CLOCK_SHIFT_OFFSET);

			//  handle lfsr
			uint8_t xor_res = (ch4_lfsr & 0x1) ^ ((ch4_lfsr & 0x2) >> 1);
			ch4_lfsr >>= 1;
			ch4_lfsr |= (xor_res << 14);
			if ((mem.map[NR43_ADDR] & CH4_LFSR_WIDTH) >> CH4_LFSR_WIDTH_OFFSET) {
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

					int duty = (mem.map[NR11_ADDR] & CH1_WAVE_DUTY) >>
						   CH1_WAVE_DUTY_OFFSET;

					if (mem.map[NR51_ADDR] & CH1_LEFT) {
						gb_apu_buf[*gb_apu_buf_pos] =
							((duties[duty][ch1_duty_pos] == 1)
								 ? ch1_volume
								 : 0);
					}

					if ((mem.map[NR51_ADDR] & CH1_RIGHT)) {
						gb_apu_buf[*gb_apu_buf_pos + 1] =
							((duties[duty][ch1_duty_pos] == 1)
								 ? ch1_volume
								 : 0);
					}
				}

				// ch2
				if (mem.map[NR52_ADDR] & CH2_ON) {

					int duty = (mem.map[NR21_ADDR] & CH2_WAVE_DUTY) >>
						   CH2_WAVE_DUTY_OFFSET;

					if (mem.map[NR51_ADDR] & CH2_LEFT) {
						gb_apu_buf[*gb_apu_buf_pos] +=
							((duties[duty][ch2_duty_pos] == 1)
								 ? ch2_volume
								 : 0);
					}
					if (mem.map[NR51_ADDR] & CH2_RIGHT) {
						gb_apu_buf[*gb_apu_buf_pos + 1] +=
							((duties[duty][ch2_duty_pos] == 1)
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

					uint8_t vol = ((mem.map[NR32_ADDR] & CH3_OUTPUT_LEVEL) >>
						       CH3_OUTPUT_LEVEL_OFFSET);
					if (vol)
						wave = wave >> (vol - 1);
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

void gb_apu_set_dac_ch1(uint8_t dac_mask)
{
	ch1_dac_on = (dac_mask != 0) ? true : false;
	if (!ch1_dac_on) {
		mem.map[NR52_ADDR] &= ~CH1_ON;
	}
}

void gb_apu_set_dac_ch2(uint8_t dac_mask)
{
	ch2_dac_on = (dac_mask != 0) ? true : false;
	if (!ch2_dac_on) {
		mem.map[NR52_ADDR] &= ~CH2_ON;
	}
}

void gb_apu_set_dac_ch3(uint8_t dac_mask)
{
	ch3_dac_on = (dac_mask != 0) ? true : false;
	if (!ch3_dac_on) {
		mem.map[NR52_ADDR] &= ~CH3_ON;
	}
}

void gb_apu_set_dac_ch4(uint8_t dac_mask)
{
	ch4_dac_on = (dac_mask != 0) ? true : false;
	if (!ch4_dac_on) {
		mem.map[NR52_ADDR] &= ~CH4_ON;
	}
}

void gb_apu_set_length_ch1(void)
{
	ch1_length_counter = 64 - (mem.map[NR11_ADDR] & CH1_INITIAL_LEN_TIMER);
}

void gb_apu_set_length_ch2(void)
{
	ch2_length_counter = 64 - (mem.map[NR21_ADDR] & CH2_INITIAL_LEN_TIMER);
}

void gb_apu_set_length_ch3(void)
{
	ch3_length_counter = 256 - (mem.map[NR31_ADDR] & CH3_INITIAL_LEN_TIMER);
}

void gb_apu_set_length_ch4(void)
{
	ch4_length_counter = 64 - (mem.map[NR41_ADDR] & CH4_INITIAL_LEN_TIMER);
}

void gb_apu_update_ch1_counter(void)
{
	// commit 1
	if (ch1_length_counter != 0 && frame_sequence_step % 2 == 0) {
		ch1_length_counter--;
	}

	// commit 2
	if (ch1_length_counter == 0) {
		mem.map[NR52_ADDR] &= ~CH1_ON;
	}
}

void gb_apu_update_ch2_counter(void)
{
	// commit 1
	if (ch2_length_counter != 0 && frame_sequence_step % 2 == 0) {
		ch2_length_counter--;
	}

	// commit 2
	if (ch2_length_counter == 0) {
		mem.map[NR52_ADDR] &= ~CH2_ON;
	}
}

void gb_apu_update_ch3_counter(void)
{
	// commit 1
	if (ch3_length_counter != 0 && frame_sequence_step % 2 == 0) {
		ch3_length_counter--;
	}

	// commit 2
	if (ch3_length_counter == 0) {
		mem.map[NR52_ADDR] &= ~CH3_ON;
	}
}

void gb_apu_update_ch4_counter(void)
{
	// commit 1
	if (ch4_length_counter != 0 && frame_sequence_step % 2 == 0) {
		ch4_length_counter--;
	}

	// commit 2
	if (ch4_length_counter == 0) {
		mem.map[NR52_ADDR] &= ~CH4_ON;
	}
}

void gb_apu_trigger_ch1(void)
{
	if (ch1_dac_on) {
		mem.map[NR52_ADDR] |= CH1_ON;
	}

	if (ch1_length_counter == 0) {
		ch1_length_counter = 64;
	}

	uint16_t freq_x =
		((mem.map[NR14_ADDR] & CH1_PERIOD_HIGH) << 8 | mem.map[NR13_ADDR] & CH1_PERIOD_LOW);
	ch1_timer = (2048 - freq_x) * 4;

	ch1_envelope = mem.map[NR12_ADDR] & CH1_SWEEP_PACE >> CH1_SWEEP_PACE_OFFSET;

	ch1_volume = (mem.map[NR12_ADDR] & CH1_INITIAL_VOL) >> CH1_INITIAL_VOL_OFFSET;

	ch1_sweep_pace =
		(mem.map[NR10_ADDR] & CH1_PERIOD_SWEEP_PACE) >> CH1_PERIOD_SWEEP_PACE_OFFSET;

	ch1_sweep_step =
		(mem.map[NR10_ADDR] & CH1_PERIOD_SWEEP_STEP) >> CH1_PERIOD_SWEEP_STEP_OFFSET;

	ch1_sweep_negate =
		((mem.map[NR10_ADDR] & CH1_PERIOD_SWEEP_DIR) >> CH1_PERIOD_SWEEP_DIR_OFFSET) ? -1
											     : 1;

	//  this was an OR before. Change if needed
	if (ch1_sweep_pace && ch1_sweep_step)
		ch1_sweep_enable = 1;
	else
		ch1_sweep_enable = 0;

	ch1_sweep_shadow =
		((mem.map[NR14_ADDR] & CH1_PERIOD_HIGH) << 8 | mem.map[NR13_ADDR] & CH1_PERIOD_LOW);

	if (ch1_sweep_step) {
		if ((ch1_sweep_shadow + ((ch1_sweep_shadow >> ch1_sweep_step) * ch1_sweep_negate)) >
		    2047) {
			mem.map[NR52_ADDR] &= ~CH1_ON;
			ch1_sweep_enable = 0;
		}
	}
}

void gb_apu_trigger_ch2(void)
{
	if (ch2_dac_on) {
		mem.map[NR52_ADDR] |= CH2_ON;
	}

	if (ch2_length_counter == 0) {
		ch2_length_counter = 64;
	}

	uint16_t freq_x =
		((mem.map[NR24_ADDR] & CH2_PERIOD_HIGH) << 8 | mem.map[NR23_ADDR] & CH2_PERIOD_LOW);
	ch2_timer = (2048 - freq_x) * 4;

	ch2_envelope = mem.map[NR22_ADDR] & CH2_SWEEP_PACE >> CH2_SWEEP_PACE_OFFSET;

	ch2_volume = (mem.map[NR22_ADDR] & CH2_INITIAL_VOL) >> CH2_INITIAL_VOL_OFFSET;
}

void gb_apu_trigger_ch3(void)
{
	if (ch3_dac_on) {
		mem.map[NR52_ADDR] |= CH3_ON;
	}

	if (ch3_length_counter == 0) {
		ch3_length_counter = 256;
	}

	uint16_t freq_x =
		((mem.map[NR34_ADDR] & CH3_PERIOD_HIGH) << 8 | mem.map[NR33_ADDR] & CH3_PERIOD_LOW);
	ch3_timer = (2048 - freq_x) * 2;

	ch3_wave_pos = 0;
}

void gb_apu_trigger_ch4(void)
{
	if (ch4_dac_on) {
		mem.map[NR52_ADDR] |= CH4_ON;
	}

	if (ch4_length_counter == 0) {
		ch4_length_counter = 64;
	}

	ch4_timer = ch4_divisor[mem.map[NR43_ADDR] & CH4_CLOCK_DIV]
		    << ((mem.map[NR43_ADDR] & CH4_CLOCK_SHIFT) >> CH4_CLOCK_SHIFT_OFFSET);
	ch4_lfsr = 0x7fff;
	ch4_volume = (mem.map[NR42_ADDR] & CH4_INITIAL_VOL) >> CH4_INITIAL_VOL_OFFSET;
	ch4_envelope = (mem.map[NR42_ADDR] & CH4_SWEEP_PACE) >> CH4_SWEEP_PACE_OFFSET;
}
