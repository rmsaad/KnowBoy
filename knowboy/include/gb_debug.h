/**
 * @file gb_debug.h
 * @brief API for the gameboy debugger functionaliy.
 *
 * @author Rami Saad
 * @date 2024-06-30
 */

#ifndef INCLUDE_GB_DEBUG_H_
#define INCLUDE_GB_DEBUG_H_

#include <stdbool.h>
#include <stdint.h>

#define QUEUE_MSG_LEN 50ul

typedef bool (*gb_debug_check_msg_queue_t)(void *, char *);
typedef void (*gb_debug_flush_t)(void);

void gb_debug_init(gb_debug_check_msg_queue_t check_msg_queue, gb_debug_flush_t flush,
		   void *queue_ctx);
void gb_debug_check_msg_queue(void);
bool gb_debug_step(void);

#endif /* INCLUDE_GB_DEBUG_H_ */
