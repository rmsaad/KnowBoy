/**
 * @file gb_papu.c
 * @brief Gameboy Audio Functionality.
 *
 * This file emulates all functionality of the gameboy audio.
 *
 * @author Rami Saad
 * @date 2021-07-11
 */

#include "gb_memory.h"
#include "gb_papu.h"
#include "logging.h"

#include <string.h>

// Audio buffers
uint16_t audio_buf[32768] = {0};
uint16_t audio_buf_pos = 0;

const uint8_t duties[4][8] = {
	{0, 0, 0, 0, 0, 0, 0, 1}, // 00
	{1, 0, 0, 0, 0, 0, 0, 1}, // 01
	{1, 0, 0, 0, 0, 1, 1, 1}, // 10
	{0, 1, 1, 1, 1, 1, 1, 0}  // 11
};

// DAQ Status
uint8_t ch1_daq_on = 0;
uint8_t ch2_daq_on = 0;
uint8_t ch4_daq_on = 0;

// Length Counters
uint8_t ch1_length_counter = 0;
uint8_t ch2_length_counter = 0;
uint8_t ch3_length_counter = 0;
uint8_t ch4_length_counter = 0;

// frequency timers
int32_t ch1_timer = 0;
int32_t ch2_timer = 0;
int32_t ch3_timer = 0;
int32_t ch4_timer = 0;

// Volume Envelopes
uint8_t ch1_envelope = 0;
uint8_t ch2_envelope = 0;
uint8_t ch3_envelope = 0;
uint8_t ch4_envelope = 0;

// Inital Volume of Envelope
uint16_t ch1_volume = 0;
uint16_t ch2_volume = 0;
uint16_t ch3_volume = 0;
uint16_t ch4_volume = 0;

// 4.194 MHZ --> 44100 HZ Conversion (4.194 / 0.0441 = 95)
uint8_t audio_freq_convert_factor = 95;

// Frame Sequence Step 0 - 7 (Incremented every 8192 T States)
uint8_t frame_sequence_step = 0;

// Frame Sequence TState (Incremented every T State up to 8192)
uint16_t frame_sequence_cycle = 0;

// CH1 & CH2 duty position (0 - 7)
uint8_t ch1_duty_pos = 0;
uint8_t ch2_duty_pos = 0;
uint8_t ch3_wave_pos = 0;

// CH1 Sweep Period, Negate, Shift
uint8_t ch1_sweep_enable = 0;
uint8_t ch1_sweep_pace = 0;
uint8_t ch1_sweep_dir = 0;
uint32_t ch1_sweep_shadow = 0;
int16_t ch1_sweep_step = 0;
int16_t ch1_sweep_negate = 0;

const uint8_t ch4_divisor[8] = {8, 16, 32, 48, 64, 80, 96, 112};
uint16_t ch4_lfsr = 0;

extern memory_t mem;

void gb_papu_step_ch1(void)
{

	// length
	if (frame_sequence_step % 2 == 0 && (mem.ram[NR14_ADDR] & CH1_LEN_EN) &&
	    ch1_length_counter) {
		ch1_length_counter--;
		if (ch1_length_counter <= 0) {
			mem.ram[NR52_ADDR] &= ~CH1_ON;
		}
	}

	//  handle sweep (frequency sweep)
	ch1_sweep_step =
		(mem.ram[NR10_ADDR] & CH1_PERIOD_SWEEP_STEP) >> CH1_PERIOD_SWEEP_STEP_OFFSET;
	ch1_sweep_pace =
		(mem.ram[NR10_ADDR] & CH1_PERIOD_SWEEP_PACE) >> CH1_PERIOD_SWEEP_PACE_OFFSET;
	ch1_sweep_dir = (mem.ram[NR10_ADDR] & CH1_PERIOD_SWEEP_DIR) >> CH1_PERIOD_SWEEP_DIR_OFFSET;

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

					mem.ram[NR13_ADDR] = ch1_sweep_shadow & 0xff;
					gb_memory_write(NR14_ADDR, (ch1_sweep_shadow >> 8) & 0x07);

					if ((ch1_sweep_shadow +
					     ((ch1_sweep_shadow >> ch1_sweep_step) *
					      ch1_sweep_negate)) > 2047) {
						ch1_sweep_enable = 0;
						mem.ram[NR52_ADDR] &= ~CH1_ON;
					}
				}

				if ((newfreq > 2047) ||
				    ((ch1_sweep_shadow + ((ch1_sweep_shadow >> ch1_sweep_step) *
							  ch1_sweep_negate)) > 2047)) {
					ch1_sweep_enable = 0;
					mem.ram[NR52_ADDR] &= ~CH1_ON;
				}
			}
		}
	}

	//  handle envelope (volume envelope)
	if (frame_sequence_step == 7 && ch1_daq_on &&
	    ((mem.ram[NR12_ADDR] & CH1_SWEEP_PACE) >> CH1_SWEEP_PACE_OFFSET)) {
		--ch1_envelope;

		if (ch1_envelope <= 0) {
			ch1_envelope =
				(mem.ram[NR12_ADDR] & CH1_SWEEP_PACE) >> CH1_SWEEP_PACE_OFFSET;

			int8_t vol =
				ch1_volume + ((mem.ram[NR12_ADDR] & CH1_ENVELOPE_DIR) ? 1 : -1);

			if (vol >= 0x0 && vol <= 0xF) {
				ch1_volume = vol;
			}
		}
	}
}

void gb_papu_step_ch2(void)
{
	// length
	if (frame_sequence_step % 2 == 0 && (mem.ram[NR24_ADDR] & CH2_LEN_EN) &&
	    ch2_length_counter) {
		ch2_length_counter--;
		if (ch2_length_counter <= 0) {
			mem.ram[NR52_ADDR] &= ~CH2_ON;
		}
	}

	// envelope
	if (frame_sequence_step == 7 && ch2_daq_on &&
	    ((mem.ram[NR22_ADDR] & CH2_SWEEP_PACE) >> CH2_SWEEP_PACE_OFFSET)) {
		ch2_envelope--;

		if (ch2_envelope <= 0) {

			ch2_envelope =
				(mem.ram[NR22_ADDR] & CH2_SWEEP_PACE) >> CH2_SWEEP_PACE_OFFSET;

			// get louder or quieter
			int8_t vol =
				ch2_volume + ((mem.ram[NR22_ADDR] & CH2_ENVELOPE_DIR) ? 1 : -1);

			if (vol >= 0x0 && vol <= 0xF) {
				ch2_volume = vol;
			}
		}
	}
}

void gb_papu_step_ch3(void)
{

	//  handle length
	if (frame_sequence_step % 2 == 0 && (mem.ram[NR34_ADDR] & CH3_LEN_EN) &&
	    ch3_length_counter) {
		ch3_length_counter--;
		if (ch3_length_counter == 0) {
			mem.ram[NR52_ADDR] &= ~CH3_ON;
		}
	}
}

void gb_papu_step_ch4(void)
{

	//  handle length
	if (frame_sequence_step % 2 == 0 && (mem.ram[NR44_ADDR] & CH4_LEN_EN) &&
	    ch4_length_counter) {
		ch4_length_counter--;
		if (ch4_length_counter == 0) {
			mem.ram[NR52_ADDR] &= ~CH4_ON;
		}
	}

	//  handle envelope (volume envelope)
	if (frame_sequence_step == 7) {
		--ch4_envelope;

		if (ch4_envelope <= 0) {

			ch4_envelope = mem.ram[NR42_ADDR] & CH4_SWEEP_PACE >> CH4_SWEEP_PACE_OFFSET;
			if ((mem.ram[NR42_ADDR] & CH4_SWEEP_PACE) != 0) {

				int8_t vol = ch4_volume +
					     ((mem.ram[NR42_ADDR] & CH4_ENVELOPE_DIR) ? 1 : -1);

				if (vol >= 0x0 && vol <= 0xF)
					ch4_volume = vol;
			}
		}
	}
}

audio_buf_t gb_papu_step(void)
{
	uint8_t current_cylces = 4;

	while (current_cylces--) {

		// check timer
		if (ch1_timer-- <= 0x00) {
			uint16_t freq_x = ((mem.ram[NR14_ADDR] & CH1_PERIOD_HIGH) << 8 |
					   mem.ram[NR13_ADDR] & CH1_PERIOD_LOW);
			ch1_timer = (2048 - freq_x) * 4;
			ch1_duty_pos++;
			ch1_duty_pos %= 8;
		}

		if (ch2_timer-- <= 0x00) {
			uint16_t freq_x = ((mem.ram[NR24_ADDR] & CH2_PERIOD_HIGH) << 8 |
					   mem.ram[NR23_ADDR] & CH2_PERIOD_LOW);
			ch2_timer = (2048 - freq_x) * 4;
			ch2_duty_pos++;
			ch2_duty_pos %= 8;
		}

		if (ch3_timer-- <= 0x00) {
			uint16_t freq_x = ((mem.ram[NR34_ADDR] & CH3_PERIOD_HIGH) << 8 |
					   mem.ram[NR33_ADDR] & CH3_PERIOD_LOW);
			ch3_timer = (2048 - freq_x) * 2;
			ch3_wave_pos++;
			ch3_wave_pos %= 32;
		}

		if (ch4_timer-- <= 0x00) {
			ch4_timer = ch4_divisor[mem.ram[NR43_ADDR] & CH4_CLOCK_DIV]
				    << ((mem.ram[NR43_ADDR] & CH4_CLOCK_SHIFT) >>
					CH4_CLOCK_SHIFT_OFFSET);

			//  handle lfsr
			uint8_t xor_res = (ch4_lfsr & 0x1) ^ ((ch4_lfsr & 0x2) >> 1);
			ch4_lfsr >>= 1;
			ch4_lfsr |= (xor_res << 14);
			if ((mem.ram[NR43_ADDR] & CH4_LFSR_WIDTH) >> CH4_LFSR_WIDTH_OFFSET) {
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

			gb_papu_step_ch1();
			gb_papu_step_ch2();
			gb_papu_step_ch3();
			gb_papu_step_ch4();
		}

		// 95
		if (!--audio_freq_convert_factor) {
			audio_freq_convert_factor = 95;

			audio_buf[audio_buf_pos] = 0;
			audio_buf[audio_buf_pos + 1] = 0;

			if (mem.ram[NR52_ADDR] & AUDIO_ON) {

				// ch1
				if (mem.ram[NR52_ADDR] & CH1_ON) {

					int duty = (mem.ram[NR11_ADDR] & CH1_WAVE_DUTY) >>
						   CH1_WAVE_DUTY_OFFSET;

					if (mem.ram[NR51_ADDR] & CH1_LEFT) {
						audio_buf[audio_buf_pos] =
							((duties[duty][ch1_duty_pos] == 1)
								 ? ch1_volume
								 : 0);
					}

					if ((mem.ram[NR51_ADDR] & CH1_RIGHT)) {
						audio_buf[audio_buf_pos + 1] =
							((duties[duty][ch1_duty_pos] == 1)
								 ? ch1_volume
								 : 0);
					}
				}

				// ch2
				if (mem.ram[NR52_ADDR] & CH2_ON) {

					int duty = (mem.ram[NR21_ADDR] & CH2_WAVE_DUTY) >>
						   CH2_WAVE_DUTY_OFFSET;

					if (mem.ram[NR51_ADDR] & CH2_LEFT) {
						audio_buf[audio_buf_pos] +=
							((duties[duty][ch2_duty_pos] == 1)
								 ? ch2_volume
								 : 0);
					}
					if (mem.ram[NR51_ADDR] & CH2_RIGHT) {
						audio_buf[audio_buf_pos + 1] +=
							((duties[duty][ch2_duty_pos] == 1)
								 ? ch2_volume
								 : 0);
					}
				}

				// ch3
				if ((mem.ram[NR52_ADDR] & CH3_ON) &&
				    (mem.ram[NR30_ADDR] & CH3_DAQ_ON)) {

					uint8_t wave = mem.ram[WPRAM_BASE + (ch3_wave_pos / 2)];

					if (ch3_wave_pos % 2) {
						wave = wave & 0xf;
					} else {
						wave = wave >> 4;
					}

					uint8_t vol = ((mem.ram[NR32_ADDR] & CH3_OUTPUT_LEVEL) >>
						       CH3_OUTPUT_LEVEL_OFFSET);
					if (vol)
						wave = wave >> (vol - 1);
					else
						wave = wave >> 4;

					if (mem.ram[NR51_ADDR] & CH3_LEFT) {
						audio_buf[audio_buf_pos] += wave;
					}

					if (mem.ram[NR51_ADDR] & CH3_RIGHT) {
						audio_buf[audio_buf_pos + 1] += wave;
					}
				}

				// ch4
				if (((mem.ram[NR52_ADDR] & CH4_ON)) &&
				    (mem.ram[NR42_ADDR] & (CH4_INITIAL_VOL + CH4_ENVELOPE_DIR))) {

					if (mem.ram[NR51_ADDR] & CH4_LEFT) {
						audio_buf[audio_buf_pos] +=
							((ch4_lfsr & 0x1) ? ch4_volume : 0);
					}
					if (mem.ram[NR51_ADDR] & CH4_RIGHT) {
						audio_buf[audio_buf_pos + 1] +=
							((ch4_lfsr & 0x1) ? ch4_volume : 0);
					}
				}

				audio_buf[audio_buf_pos] <<=
					((mem.ram[NR50_ADDR] & VOL_LEFT) >> VOL_LEFT_OFFSET);

				audio_buf[audio_buf_pos + 1] <<=
					((mem.ram[NR50_ADDR] & VOL_RIGHT) >> VOL_RIGHT_OFFSET);
			}
			audio_buf_pos += 2;
		}
	}

	audio_buf_t buf;
	buf.buffer = audio_buf;
	buf.len = &audio_buf_pos;
	return buf;
}

// TRIGGER EVENTS
void gb_papu_trigger_ch1(void)
{
	// Enable Channel
	mem.ram[NR52_ADDR] |= CH1_ON;

	// Set Length Channel to 64 - t1
	if (ch1_length_counter == 0)
		ch1_length_counter = 64 - (mem.ram[NR11_ADDR] & CH1_INITIAL_LEN_TIMER);

	uint16_t freq_x =
		((mem.ram[NR14_ADDR] & CH1_PERIOD_HIGH) << 8 | mem.ram[NR13_ADDR] & CH1_PERIOD_LOW);
	ch1_timer = (2048 - freq_x) * 4;

	// reload volume envelope
	ch1_envelope = mem.ram[NR12_ADDR] & CH1_SWEEP_PACE >> CH1_SWEEP_PACE_OFFSET;
	ch1_daq_on = 1;

	ch1_volume = (mem.ram[NR12_ADDR] & CH1_INITIAL_VOL) >> CH1_INITIAL_VOL_OFFSET;

	ch1_sweep_pace =
		(mem.ram[NR10_ADDR] & CH1_PERIOD_SWEEP_PACE) >> CH1_PERIOD_SWEEP_PACE_OFFSET;

	ch1_sweep_step =
		(mem.ram[NR10_ADDR] & CH1_PERIOD_SWEEP_STEP) >> CH1_PERIOD_SWEEP_STEP_OFFSET;

	ch1_sweep_negate =
		((mem.ram[NR10_ADDR] & CH1_PERIOD_SWEEP_DIR) >> CH1_PERIOD_SWEEP_DIR_OFFSET) ? -1
											     : 1;

	//  this was an OR before. Change if needed
	if (ch1_sweep_pace && ch1_sweep_step)
		ch1_sweep_enable = 1;
	else
		ch1_sweep_enable = 0;

	ch1_sweep_shadow =
		((mem.ram[NR14_ADDR] & CH1_PERIOD_HIGH) << 8 | mem.ram[NR13_ADDR] & CH1_PERIOD_LOW);

	if (ch1_sweep_step) {
		if ((ch1_sweep_shadow + ((ch1_sweep_shadow >> ch1_sweep_step) * ch1_sweep_negate)) >
		    2047) {
			mem.ram[NR52_ADDR] &= ~CH1_ON;
			ch1_sweep_enable = 0;
		}
	}

	if (mem.ram[NR12_ADDR] & (CH1_ENVELOPE_DIR + CH1_INITIAL_VOL) == 0x00) {
		mem.ram[NR52_ADDR] &= ~CH1_ON;
	}
}

void gb_papu_trigger_ch2(void)
{
	mem.ram[NR52_ADDR] |= CH2_ON;

	if (ch2_length_counter == 0)
		ch2_length_counter = 64 - (mem.ram[NR21_ADDR] & CH2_INITIAL_LEN_TIMER);

	uint16_t freq_x =
		((mem.ram[NR24_ADDR] & CH2_PERIOD_HIGH) << 8 | mem.ram[NR23_ADDR] & CH2_PERIOD_LOW);
	ch2_timer = (2048 - freq_x) * 4;

	ch2_envelope = mem.ram[NR22_ADDR] & CH2_SWEEP_PACE >> CH2_SWEEP_PACE_OFFSET;
	ch2_daq_on = 1;

	ch2_volume = (mem.ram[NR22_ADDR] & CH2_INITIAL_VOL) >> CH2_INITIAL_VOL_OFFSET;

	if (mem.ram[NR22_ADDR] & (CH2_ENVELOPE_DIR + CH2_INITIAL_VOL) == 0x00) {
		mem.ram[NR52_ADDR] &= ~CH2_ON;
	}
}

void gb_papu_trigger_ch3(void)
{
	mem.ram[NR52_ADDR] |= CH3_ON;

	if (ch3_length_counter == 0)
		ch3_length_counter = 256 - (mem.ram[NR31_ADDR] & CH3_INITIAL_LEN_TIMER);

	uint16_t freq_x =
		((mem.ram[NR34_ADDR] & CH3_PERIOD_HIGH) << 8 | mem.ram[NR33_ADDR] & CH3_PERIOD_LOW);
	ch3_timer = (2048 - freq_x) * 2;

	ch3_wave_pos = 0;

	if ((mem.ram[NR30_ADDR] & CH3_DAQ_ON) == 0x00) {
		mem.ram[NR52_ADDR] &= ~CH3_ON;
	}
}

void gb_papu_trigger_ch4(void)
{
	mem.ram[NR52_ADDR] |= CH4_ON;

	if (ch4_length_counter == 0)
		ch4_length_counter = 64 - (mem.ram[NR41_ADDR] & CH4_INITIAL_LEN_TIMER);

	ch4_timer = ch4_divisor[mem.ram[NR43_ADDR] & CH4_CLOCK_DIV]
		    << ((mem.ram[NR43_ADDR] & CH4_CLOCK_SHIFT) >> CH4_CLOCK_SHIFT_OFFSET);
	ch4_lfsr = 0x7fff;
	ch4_volume = (mem.ram[NR42_ADDR] & CH4_INITIAL_VOL) >> CH4_INITIAL_VOL_OFFSET;
	ch4_envelope = (mem.ram[NR42_ADDR] & CH4_SWEEP_PACE) >> CH4_SWEEP_PACE_OFFSET;

	if (mem.ram[NR42_ADDR] & (CH4_ENVELOPE_DIR + CH4_INITIAL_VOL) == 0x00) {
		mem.ram[NR52_ADDR] &= ~CH4_ON;
	}
}
