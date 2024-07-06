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

#define QUEUE_MSG_LEN 50

typedef void (*gb_debug_mutex_lock_t)(void *);
typedef void (*gb_debug_mutex_unlock_t)(void *);
typedef bool (*gb_dequeue_t)(void *q, char *message);

void gb_debug_init(gb_debug_mutex_lock_t lock, gb_debug_mutex_unlock_t unlock, gb_dequeue_t dequeue,
		   void *queue, void *mutex);
void gb_debug_check_queue();
bool gb_debug_step();

#endif /* INCLUDE_GB_DEBUG_H_ */
