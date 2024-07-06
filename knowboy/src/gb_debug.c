/**
 * @file gb_mbc.c
 * @brief Gameboy debugger Functionality.
 *
 * This file implements debugger functionality for the gameboy.
 *
 * @author Rami Saad
 * @date 2024-06-30
 */

#include "gb_debug.h"
#include "gb_memory.h"
#include "logging.h"

#include <stdint.h>

#define MAX_BREAKPOINTS 5

typedef struct {
	uint16_t address[MAX_BREAKPOINTS];
	bool activated[MAX_BREAKPOINTS];
} breakpoints_t;

static bool debugger_stopped = false;
static breakpoints_t breakpoints;
static gb_debug_mutex_lock_t mutex_lock;
static gb_debug_mutex_unlock_t mutex_unlock;
static gb_dequeue_t gb_dequeue;
static void *gb_mutex;
static void *gb_queue;
static bool proceed;
static uint16_t prev_PC;
extern memory_t mem;

static bool strict_natoi(char *str, int len, uint16_t *result)
{
	int num = 0;
	int i = 0;
	bool is_valid = true;
	bool is_hex = false;

	if (str[0] == '-') {
		is_valid = false;
	}

	if (len > 2 && str[0] == '0' && str[1] == 'x') {
		is_hex = true;
		i = 2;
	}

	while (i < len) {
		if (isdigit(str[i])) {
			num = num * (is_hex ? 16 : 10) + (str[i] - '0');
		} else if (is_hex && str[i] >= 'a' && str[i] <= 'f') {
			num = num * 16 + (str[i] - 'a' + 10);
		} else if (is_hex && str[i] >= 'A' && str[i] <= 'F') {
			num = num * 16 + (str[i] - 'A' + 10);
		} else if (str[i] == '\0') {
			break;
		} else {
			is_valid = false;
			break;
		}
		i++;
	}

	if (is_valid) {
		*result = num;
	}

	return is_valid;
}

int get_next_free_breakpoint(breakpoints_t *bps)
{
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (!bps->activated[i]) {
			return i;
		}
	}
	return -1;
}

bool deactivate_breakpoint(breakpoints_t *bps, uint16_t address)
{
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (bps->address[i] == address && bps->activated[i]) {
			bps->activated[i] = false;
			return true;
		}
	}
	return false;
}

bool is_breakpoint_activated(breakpoints_t *bps, uint16_t address)
{
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (bps->address[i] == address && bps->activated[i]) {
			return true;
		}
	}
	return false;
}

void deactivate_all_breakpoints(breakpoints_t *bps)
{
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		bps->activated[i] = false;
	}
}

void gb_debug_init(gb_debug_mutex_lock_t lock, gb_debug_mutex_unlock_t unlock, gb_dequeue_t dequeue,
		   void *queue, void *mutex)
{
	debugger_stopped = false;
	proceed = false;
	mutex_lock = lock;
	mutex_unlock = unlock;
	gb_dequeue = dequeue;
	gb_queue = queue;
	gb_mutex = mutex;
	deactivate_all_breakpoints(&breakpoints);
}

static void gb_debug_parse_command()
{
	char message[QUEUE_MSG_LEN];
	if (gb_dequeue(gb_queue, message)) {
		if (strncmp(message, "stop", QUEUE_MSG_LEN) == 0) {
			debugger_stopped = true;
		} else if (strncmp(message, "continue", QUEUE_MSG_LEN) == 0) {
			debugger_stopped = false;
		} else if (strncmp(message, "step", QUEUE_MSG_LEN) == 0) {
			proceed = true;
			prev_PC = mem.reg.PC;
		} else if (strncmp(message, "registers", QUEUE_MSG_LEN) == 0) {
			LOG_DBG("opcode: %x, PC: %x, AF: %x, BC: %x, DE: %x, HL: %x, SP: %x",
				mem.map[mem.reg.PC], mem.reg.PC, mem.reg.AF, mem.reg.BC, mem.reg.DE,
				mem.reg.HL, mem.reg.SP);
		} else if (strncmp(message, "break", sizeof("break") - 1) == 0) {
			uint16_t address;
			bool valid = strict_natoi(&message[sizeof("break")],
						  QUEUE_MSG_LEN - (sizeof("break")), &address);
			if (valid && !is_breakpoint_activated(&breakpoints, address)) {
				int index = get_next_free_breakpoint(&breakpoints);
				if (index < 0) {
					LOG_ERR("No more breakpoints availible");
					return;
				}
				breakpoints.address[index] = address;
				breakpoints.activated[index] = true;
			}
		} else if (strncmp(message, "delete", sizeof("delete") - 1) == 0) {
			uint16_t address;
			bool valid = strict_natoi(&message[sizeof("delete")],
						  QUEUE_MSG_LEN - (sizeof("delete")), &address);
			if (valid && is_breakpoint_activated(&breakpoints, address)) {
				deactivate_breakpoint(&breakpoints, address);
			}
		}
	}
}

void gb_debug_check_queue()
{
	mutex_lock(gb_mutex);
	gb_debug_parse_command();
	mutex_unlock(gb_mutex);
}

bool gb_debug_step()
{
	if (proceed == true) {
		if (mem.reg.PC == prev_PC) {
			return false;
		} else {
			proceed = false;
		}
	}

	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (breakpoints.activated[i] == true) {
			if (mem.reg.PC == breakpoints.address[i]) {
				debugger_stopped = true;
			}
		}
	}

	if (debugger_stopped) {
		gb_debug_check_queue();
	}

	return debugger_stopped && !proceed;
}
