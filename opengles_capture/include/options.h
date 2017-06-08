/*
 * Copyright (c) 2017 D3 Engineering
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/**
 * User specified option management.
 * @file options.h
 *
 */
#ifndef OPTION_H__
#define OPTION_H__

#include <sys/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Default values for command line options
 */
#define DEFAULT_COUNT 5
#define DEFAULT_DEVICE "/dev/video3"
#define DEFAULT_BUFFER_COUNT 4
#define DEFAULT_SUBDEVICE "/dev/v4l-subdev10"

#define CAPTURE_DEV		'd'
#define CAPTURE_SUBDEV	's'
#define CAPTURE_COUNT	'n'
#define PROGRAM_USE 	'u'

struct options;
/**
 * Function pointer for any test program entry points
 */
typedef int (*usage_function)(void *capture, void* display, struct options*);

/**
 * Description of a test option to run from the command line.
 */
struct usage {
	/** Name of the command line test, must be unique. */
	char* name;
	/** Description of the test displayed in the command line help. */
	char* description;
	/** The function to call when this test is selected. */
	usage_function function;
	/** Linked list entry of all test choices. */
	TAILQ_ENTRY(usage) usage_entry;
};

/**
 * Data structure for all user command line options.
 */
struct options {
	/** number of buffers to capture. */
	int capture_count;
	/** Number of buffers to allocate from the v4l2 device. */
	int buffer_count;
	/** Export DMA file descriptor for each v4l2 plane. */
	int dma_export;
	/** V4L2 capture device path. */
	char* dev_name;
	/** V4L2 camera subdevice path. */
	char* subdev_name;
	/** Selected application use or test to run. */
	struct usage* program_use;
	/** Default test to run if none is selected by command line arguments. */
	struct usage* default_usage;
	/** List of possible applications tests or uses that can be run by the user. */
	TAILQ_HEAD(, usage) usage_head;
};

extern struct options g_program_options;

/**
 * GCC constructor extension for setting up the program uses.
 * PRIORITY_SETUP is used to initialize the global usage list.
 */
#define PRIORITY_SETUP 101
/**
 * PRIORITY_DEFAULTS is used to select the function called if the user does not change it via the command line.
 */
#define PRIORITY_DEFAULTS 102
/**
 * PRIORITY_NEW_USAGE should be chosen for any new functions to add to the applition.
 */
#define PRIORITY_NEW_USAGE 103
/**
 * Add a new function option to be run at startup from the command line.
 * Each function can be a different test program or application.
 * If none is selected the default entry will be used.
 * A new default should use PRIORITY_DEFAULTS.
 * A new function should use PRIORITY_NEW_USAGE.
 * PRIORITY_SETUP is reserved by the main app for structure initialization.
 *
 * @note Example of a usage setup
 * static struct usage a_new_test = {
 *    .name="TEST_A",
 *    .description = "Description of TEST_A",
 *    .function = test_a_func};
 * __attribute__((constructor (PRIORITY_NEW_USAGE))) void add_test_a(void) {
 *     insert_usage(&a_new_test, false); }; // add a new test that is not a default
 */
static inline void insert_usage(struct usage* new_usage, int default_usage)
{
	TAILQ_INSERT_TAIL(&g_program_options.usage_head, new_usage ,usage_entry);
	if (default_usage) g_program_options.default_usage = new_usage;
};

#ifdef __cplusplus
}
#endif

#endif