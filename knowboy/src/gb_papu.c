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

#include <string.h>

// Audio buffers
uint16_t audio_buf[32768] = {0};
uint16_t audio_buf_pos = 0;

uint8_t duties[4][8] = {
	{0, 0, 0, 0, 0, 0, 0, 1}, //  00 (0x0)
	{1, 0, 0, 0, 0, 0, 0, 1}, //  01 (0x1)
	{1, 0, 0, 0, 0, 1, 1, 1}, //  10 (0x2)
	{0, 1, 1, 1, 1, 1, 1, 0}  //  11 (0x3)
};

// Channel Enables
uint8_t ch1_enable = 0;
uint8_t ch2_enable = 0;
uint8_t ch3_enable = 0;
uint8_t ch4_enable = 0;

// Volume Envelope Enables
uint8_t ch1_envelope_enable = 0; // This might refer to sweeps only
uint8_t ch2_envelope_enable = 0;
uint8_t ch3_envelope_enable = 0;
uint8_t ch4_envelope_enable = 0;

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
uint16_t ch1_initial_volume = 0;
uint16_t ch2_initial_volume = 0; //~~
uint16_t ch3_initial_volume = 0;
uint16_t ch4_initial_volume = 0;

// 4.194 MHZ --> 44100 HZ Conversion (4.194 / 0.0441 = 95)
uint8_t ch1_audio_freq_convert_factor = 95;
uint8_t ch2_audio_freq_convert_factor = 95;
uint8_t ch3_audio_freq_convert_factor = 95;
uint8_t ch4_audio_freq_convert_factor = 95;

// Frame Sequence Step 0 - 7 (Incremented every 8192 T States)
uint8_t ch1_frame_sequence_step = 0;
uint8_t ch2_frame_sequence_step = 0;
uint8_t ch3_frame_sequence_step = 0;
uint8_t ch4_frame_sequence_step = 0;

// Frame Sequence TState (Incremented every T State up to 8192)
uint16_t ch1_frame_sequence_cycle = 0;
uint16_t ch2_frame_sequence_cycle = 0;
uint16_t ch3_frame_sequence_cycle = 0;
uint16_t ch4_frame_sequence_cycle = 0;

// CH1 & CH2 duty position (0 - 7)
uint8_t ch1_duty_pos = 0;
uint8_t ch2_duty_pos = 0;
uint8_t ch3_wave_pos = 0;

// CH1 Sweep Period, Negate, Shift
uint8_t ch1_sweep_enable = 0;
uint8_t ch1_sweep_period = 0;
uint32_t ch1_sweep_shadow = 0;
int16_t ch1_sweep_shift = 0;
int16_t ch1_sweep_negate = 0;

uint8_t ch4_divisor[8] = {8, 16, 32, 48, 64, 80, 96, 112};
uint16_t ch4_lfsr = 0;

extern memory_t mem;

void gb_papu_step_ch1(void)
{

	// length
	if (ch1_frame_sequence_step % 2 == 0 && ((mem.ram[NR14_ADDR] >> 6) & 0x01) &&
	    ch1_length_counter) {
		ch1_length_counter--;
		if (ch1_length_counter <= 0) {
			ch1_enable = 0;
		}
	}

	//  handle sweep (frequency sweep)
	if ((ch1_frame_sequence_step == 2 || ch1_frame_sequence_step == 6) &&
	    ((mem.ram[NR10_ADDR] >> 4) & 0x07) && (mem.ram[NR10_ADDR] & 0x07)) {

		//  tick sweep down
		--ch1_sweep_period;
		if (ch1_sweep_period <= 0) {

			//  reload sweep
			ch1_sweep_period = (mem.ram[NR10_ADDR] >> 4) & 0x07;
			if (ch1_sweep_period == 0)
				ch1_sweep_period = 8;

			//  only sweep, if there is a sweep frequency set
			if (((mem.ram[NR10_ADDR] >> 4) & 7) && ch1_sweep_enable &&
			    (mem.ram[NR10_ADDR] & 7)) {

				ch1_sweep_shift = mem.ram[NR10_ADDR] & 0x07;
				ch1_sweep_negate = ((mem.ram[NR10_ADDR] >> 3) & 0x01) ? -1 : 1;

				uint32_t newfreq =
					ch1_sweep_shadow +
					((ch1_sweep_shadow >> ch1_sweep_shift) * ch1_sweep_negate);
				if (newfreq < 2048 && ch1_sweep_shift) {
					ch1_sweep_shadow = newfreq;

					gb_memory_write(NR13_ADDR, ch1_sweep_shadow & 0xff);
					gb_memory_write(NR14_ADDR, (ch1_sweep_shadow >> 8) & 0x07);

					if ((ch1_sweep_shadow +
					     ((ch1_sweep_shadow >> ch1_sweep_shift) *
					      ch1_sweep_negate)) > 2047) {
						ch1_sweep_enable = 0;
						ch1_enable = 0;
					}
				}
				if (newfreq > 2047) {
					ch1_sweep_enable = 0;
					ch1_enable = 0;
				}
				if ((ch1_sweep_shadow + ((ch1_sweep_shadow >> ch1_sweep_shift) *
							 ch1_sweep_negate)) > 2047) {
					ch1_sweep_enable = 0;
					ch1_enable = 0;
				}
			}
		}
	}

	//  handle envelope (volume envelope)
	if (ch1_frame_sequence_step == 7 && (mem.ram[NR12_ADDR] & 0x07) && ch1_envelope_enable) {
		//  tick envelope down
		--ch1_envelope;
		//  when the envelope is done ticking to zero, it's time to trigger it's purpose
		if (ch1_envelope <= 0) {

			//  reload envelope tick count
			ch1_envelope = mem.ram[NR12_ADDR] & 0x07;
			//  calc new frequence (+1 / -1)
			int8_t vol =
				ch1_initial_volume + (((mem.ram[NR12_ADDR] >> 3) & 0x01) ? 1 : -1);

			//  the new volume needs to be inside 0-15 range
			if (vol >= 0 && vol <= 15) {
				ch1_initial_volume = vol;
			} else
				ch1_envelope_enable = 0;
		}
	}
}

void gb_papu_step_ch2(void)
{

	// length
	if (ch2_frame_sequence_step % 2 == 0 && ((mem.ram[NR24_ADDR] >> 6) & 0x01) &&
	    ch2_length_counter) {
		ch2_length_counter--;
		if (ch2_length_counter <= 0) {
			ch2_enable = 0;
		}
	}

	// envelope
	if (ch2_frame_sequence_step == 7 && ch2_envelope_enable && mem.ram[NR22_ADDR] & 0x07) {
		ch2_envelope--;

		if (ch2_envelope <= 0) {
			ch2_envelope = mem.ram[NR22_ADDR] & 0x07;

			// dont understand here!!
			int8_t vol =
				ch2_initial_volume + (((mem.ram[NR22_ADDR] >> 3) & 0x01) ? 1 : -1);

			if (vol >= 0 && vol <= 15) {
				ch2_initial_volume = vol;
			} else {
				ch2_envelope_enable = 0;
			}
		}
	}
}

void gb_papu_step_ch3(void)
{

	//  handle length
	if (ch3_frame_sequence_step % 2 == 0 && ((mem.ram[NR34_ADDR] >> 6) & 0x01) &&
	    ch3_length_counter) {
		ch3_length_counter--;
		if (ch3_length_counter == 0) {
			ch3_enable = 0;
		}
	}
}

void gb_papu_step_ch4(void)
{

	//  handle length
	if (ch4_frame_sequence_step % 2 == 0 && ((mem.ram[NR44_ADDR] >> 6) & 0x01) &&
	    ch4_length_counter) {
		ch4_length_counter--;
		if (ch4_length_counter == 0) {
			ch4_enable = 0;
		}
	}

	//  handle envelope (volume envelope)
	if (ch4_frame_sequence_step == 7) {
		//  tick envelope down
		--ch4_envelope;
		//  when the envelope is done ticking to zero, it's time to trigger it's purpose
		if (ch4_envelope <= 0) {
			//  reload envelope tick count
			ch4_envelope = mem.ram[NR42_ADDR] & 0x07;
			/*if (SC4envelope == 0)
			    SC4envelope = 8;*/
			if ((mem.ram[NR42_ADDR] & 0x07) != 0) {
				//  calc new frequence (+1 / -1)
				int8_t vol = ch4_initial_volume +
					     (((mem.ram[NR42_ADDR] >> 3) & 0x01) ? 1 : -1);
				//  the new volume needs to be inside 0-15 range
				if (vol >= 0 && vol <= 15)
					ch4_initial_volume = vol;
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
			uint16_t freq_x = ((mem.ram[NR14_ADDR] & 0x07) << 8 | mem.ram[NR13_ADDR]);
			// reload freq timer when it hits zero
			ch1_timer = (2048 - freq_x) * 4; //
			ch1_duty_pos++;
			ch1_duty_pos %= 8;
		}

		if (ch2_timer-- <= 0x00) {
			uint16_t freq_x = ((mem.ram[NR24_ADDR] & 0x07) << 8 | mem.ram[NR23_ADDR]);
			// reload freq timer when it hits zero
			ch2_timer = (2048 - freq_x) * 4;
			ch2_duty_pos++;
			ch2_duty_pos %= 8;
		}

		if (ch3_timer-- <= 0x00) {
			uint16_t freq_x =
				((mem.ram[NR34_ADDR] & 0x07) << 8 |
				 mem.ram[NR33_ADDR]); // reload freq timer when it hits zero
			ch3_timer = (2048 - freq_x) * 2;
			ch3_wave_pos++;
			ch3_wave_pos %= 32;
		}

		if (ch4_timer-- <= 0x00) {
			ch4_timer = ch4_divisor[mem.ram[NR43_ADDR] & 0x07]
				    << (mem.ram[NR43_ADDR] >> 4);

			//  handle lfsr
			uint8_t xor_res = (ch4_lfsr & 0x1) ^ ((ch4_lfsr & 0x2) >> 1);
			ch4_lfsr >>= 1;
			ch4_lfsr |= (xor_res << 14);
			if ((mem.ram[NR43_ADDR] >> 3) & 0x01) {
				ch4_lfsr |= (xor_res << 6);
				ch4_lfsr &= 0x7f;
			}
		}

		// FS Step
		ch1_frame_sequence_cycle++;
		if (ch1_frame_sequence_cycle == 8192) {
			ch1_frame_sequence_cycle = 0; // Set Frame Sequence Tstates back to zero

			ch1_frame_sequence_step++;
			ch1_frame_sequence_step %= 8;

			gb_papu_step_ch1();
			gb_papu_step_ch2();
			gb_papu_step_ch3();
			gb_papu_step_ch4();
		}

		// 95
		if (!--ch1_audio_freq_convert_factor) {
			ch1_audio_freq_convert_factor = 95;

			// ch1
			//  enabled channel
			if (ch1_enable && (mem.ram[NR52_ADDR] & 0x01) &&
			    (mem.ram[NR51_ADDR] & 0x11)) {
				int duty = mem.ram[NR11_ADDR] >> 6;
				audio_buf[audio_buf_pos] = ((duties[duty][ch1_duty_pos] == 1)
								    ? (ch1_initial_volume << 8)
								    : 0); // Left  Channel
				audio_buf[audio_buf_pos + 1] = ((duties[duty][ch1_duty_pos] == 1)
									? (ch1_initial_volume << 8)
									: 0); // Right Channel

			}
			//  disabled channel
			else {
				audio_buf[audio_buf_pos] = 0;
				audio_buf[audio_buf_pos + 1] = 0;
			}

			// ch2
			if (ch2_enable && (mem.ram[NR51_ADDR] & 0x22) &&
			    (mem.ram[NR52_ADDR] & 0x02)) {

				int duty = mem.ram[NR21_ADDR] >> 6;
				audio_buf[audio_buf_pos] += ((duties[duty][ch2_duty_pos] == 1)
								     ? (ch2_initial_volume << 8)
								     : 0); // Left  Channel
				audio_buf[audio_buf_pos + 1] += ((duties[duty][ch2_duty_pos] == 1)
									 ? (ch2_initial_volume << 8)
									 : 0); // Right Channel
			}

			// ch3
			if (ch3_enable && mem.ram[NR30_ADDR] >> 0x07 &&
			    (mem.ram[NR51_ADDR] & 0x44)) {

				uint8_t wave = mem.ram[WPRAM_BASE + (ch3_wave_pos / 2)];
				if (ch3_wave_pos % 2) {
					wave = wave & 0xf;
				} else {
					wave = wave >> 4;
				}

				uint8_t vol = (mem.ram[NR32_ADDR] >> 5) & 0x03;
				if (vol)
					wave = wave >> (vol - 1);
				else
					wave = wave >> 4;

				//  if dac is enabled, output actual wave
				audio_buf[audio_buf_pos] += wave << 8;
				audio_buf[audio_buf_pos + 1] += wave << 8;
			}

			// ch4
			//  enabled channel
			if (ch4_enable && ((mem.ram[NR52_ADDR] >> 3) & 0x01) &&
			    (mem.ram[NR42_ADDR] & 0xf8) && (mem.ram[NR51_ADDR] & 0x88)) {
				//  push data to buffer
				audio_buf[audio_buf_pos] +=
					((ch4_lfsr & 0x1) ? (ch4_initial_volume << 8)
							  : 0); // Left Channel
				audio_buf[audio_buf_pos + 1] +=
					((ch4_lfsr & 0x1) ? (ch4_initial_volume << 8)
							  : 0); // Right Channel
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
void gb_papu_trigger_ch1(uint8_t data)
{
	ch1_enable = 1; // Enable Channel

	if (ch1_length_counter == 0) // Set Length Channel to 64 - t1(data)
		ch1_length_counter = 64 - data;

	uint16_t freq_x =
		((mem.ram[NR14_ADDR] & 0x07) << 8 | mem.ram[NR13_ADDR]); // reload freq timer
	ch1_timer = (2048 - freq_x) * 4;

	ch1_initial_volume = mem.ram[NR12_ADDR] >> 4;

	if (mem.ram[NR12_ADDR] >> 3 == 0x00) {
		ch1_enable = 0;
	}

	ch1_sweep_period = (mem.ram[NR10_ADDR] >> 4) & 0x07;
	ch1_sweep_shift = mem.ram[NR10_ADDR] & 0x07;
	ch1_sweep_negate = ((mem.ram[NR10_ADDR] >> 3) & 0x01) ? -1 : 1;

	if (ch1_sweep_period && ch1_sweep_shift) //  this was an OR before. Change if needed
		ch1_sweep_enable = 1;
	else
		ch1_sweep_enable = 0;

	ch1_sweep_shadow = (((mem.ram[NR14_ADDR] & 0x07) << 8) | mem.ram[NR13_ADDR]);

	if (ch1_sweep_shift) {
		if ((ch1_sweep_shadow +
		     ((ch1_sweep_shadow >> ch1_sweep_shift) * ch1_sweep_negate)) > 2047) {
			ch1_sweep_enable = 0;
			ch1_enable = 0;
		}
	}
}

void gb_papu_trigger_ch2(uint8_t data)
{
	ch2_enable = 1; // Enable Channel
	gb_memory_write(NR52_ADDR, 0x82);
	if (ch2_length_counter == 0) // Set length Channel to 64 - t1(data)
		ch2_length_counter = 64 - data;

	uint16_t freq_x =
		((mem.ram[NR24_ADDR] & 0x07) << 8 | mem.ram[NR23_ADDR]); // reload freq timer
	ch2_timer = (2048 - freq_x) * 4;

	ch2_envelope = mem.ram[NR22_ADDR] & 0x07; // reload volume envelope
	ch2_envelope_enable = 1;

	ch2_initial_volume = mem.ram[NR22_ADDR] >> 4;

	if (mem.ram[NR22_ADDR] >> 3 == 0x00) {
		ch2_enable = 0;
	}
}

void gb_papu_trigger_ch3(uint8_t data)
{
	ch3_enable = 1;

	if (ch3_length_counter == 0)
		ch3_length_counter = 256 - data;

	uint16_t freq_x =
		((mem.ram[NR34_ADDR] & 0x07) << 8 | mem.ram[NR33_ADDR]); // reload freq timer
	ch3_timer = (2048 - freq_x) * 2;

	ch3_wave_pos = 0;

	if (mem.ram[NR30_ADDR] >> 6 == 0x00) {
		ch3_enable = 0;
	}
}

void gb_papu_trigger_ch4(uint8_t data)
{
	ch4_enable = 1;

	if (ch4_length_counter == 0)
		ch4_length_counter = 64 - data;

	ch4_timer = ch4_divisor[mem.ram[NR43_ADDR] & 0x7] << (mem.ram[NR43_ADDR] >> 4);
	ch4_lfsr = 0x7fff;
	ch4_initial_volume = mem.ram[NR42_ADDR] >> 4;
	ch4_envelope = mem.ram[NR42_ADDR] & 0x07;
	ch4_envelope_enable = 1;

	if (mem.ram[NR42_ADDR] >> 3 == 0x00) {
		ch4_enable = 0;
	}
}
