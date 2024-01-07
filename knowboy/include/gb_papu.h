/**
 * @file gb_papu.h
 * @brief API for the gameboy audio.
 *
 * @author Rami Saad
 * @date 2021-07-11
 */

#ifndef INCLUDE_GB_PAPU_H_
#define INCLUDE_GB_PAPU_H_

typedef struct {
	uint16_t *buffer;
	uint16_t *len;
} audio_buf_t;

audio_buf_t gb_papu_step(void);
void gb_papu_trigger_ch1(uint8_t data);
void gb_papu_trigger_ch2(uint8_t data);
void gb_papu_trigger_ch3(uint8_t data);
void gb_papu_trigger_ch4(uint8_t data);

#endif /* INCLUDE_GB_PAPU_H_ */
