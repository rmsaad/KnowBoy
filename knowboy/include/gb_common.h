/**
 * @file gb_common.h
 * @brief Common functionality to be used by each module.
 *
 * @author Rami Saad
 * @date 2023-12-19
 */

#ifndef INCLUDE_GB_COMMON_H_
#define INCLUDE_GB_COMMON_H_

/* Sets a bit in a memory location */
#define SET_BIT(n, bit) ((n) |= (0x1 << bit))

/* Resets a bit in a memory location */
#define RST_BIT(n, bit) ((n) &= ~(0x1 << bit))

/* Checks value of a bit of a memory location */
#define CHK_BIT(n, bit) ((n >> bit) & 0x1)

/* Concatenates two bytes of data */
#define CAT_BYTES(lsb, msb) ((uint16_t)(msb << 8) + lsb)

#endif /* INCLUDE_GB_COMMON_H_ */
