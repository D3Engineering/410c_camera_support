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
 * Main application for V4L2 capture and OpenGL display tests and utilities.
 * @file main.c
 *
 * @copyright
 * Copyright (c) 2017 D3 Engineering
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/queue.h>

#include "options.h"
#include "capture.h"
#include "display.h"
#include "log.h"

int VERBOSE = LOG_INFO;

struct options g_program_options = {0};

/* Setup the catpure display function as a progam usage. */
static struct usage capture_display = {
		.name="CAPTURE_DISPLAY",
		.description = "Capture V4L2 buffers and OpenGLES YUV shader display",
		.function = capture_and_display,
};

/**
 * Initialize the queue of program tests before tests are added.
 * @note The contstructor attribute is a GCC extension.
 */
__attribute__((constructor (PRIORITY_SETUP))) void init_global(void)
{
	TAILQ_INIT(&g_program_options.usage_head);
}

/**
 * Initialize the default application function capture_and_display.
 * @note The contstructor attribute is a GCC extension.
 */
__attribute__((constructor (PRIORITY_DEFAULTS))) void add_default_usage(void)
{
	insert_usage(&capture_display, true);
}

/**
 * Print the available program command line options.
 * @param argv the list of program arguments.
 */
void usage(char * const argv[])
{
	printf("%s - V4L2 capture, OpenGL Display and test\n", argv[0]);
	printf("-d <device>, --device v4l2 device for streaming\n");
	printf("-s <sub-device>, --subdevice v4l2 subdevice device for options\n");
	printf("-p #,  --test-pattern # test pattern to capture instead of live video\n");
	printf("-u TYPE,  --usage TYPE program use to return\n");
	printf("\tSupport values:\n");
	struct usage *usage;
	TAILQ_FOREACH(usage, &g_program_options.usage_head, usage_entry)
		printf("\t%s - %s\n", usage->name, usage->description);
	printf("-v, --verbose\n");
}

/**
 * Assign the command line options that should be used by default for options not set by the user.
 * @param opt data structure that contains all user options.
 */
void set_default_options(struct options *opt)
{
	opt->capture_count = DEFAULT_COUNT;
	opt->dev_name = (char*)DEFAULT_DEVICE;
	opt->subdev_name = (char*)DEFAULT_SUBDEVICE;
	opt->buffer_count = DEFAULT_BUFFER_COUNT;
	opt->program_use = opt->default_usage;
	opt->dma_export = false;
}


/**
 * Parse the command line arguments and assign the results to the option structure.
 * @param opt data structure that contains all user options.
 * @param argc number of commmand line arguments.
 * @param argv array of command line argument strings.
 * @return 0 on success or negative on error parsing arguments.
 */
int get_options(struct options *opt, int argc, char * const argv[])
{
	int o;
	int found = false;
	struct usage *program_use;
	static struct option long_options[] = {
		{"device", 			required_argument, 	0, CAPTURE_DEV  },
		{"subdevice", 		required_argument, 	0, CAPTURE_SUBDEV  },
		{"count", 			required_argument,	0, CAPTURE_COUNT },
		{"usage",			required_argument,	0, PROGRAM_USE },
		{"help",			no_argument, 		0, 'h'},
		{"verbose",			optional_argument,	0, 'v'},
		{0},
	};

	set_default_options(opt);

	while(1)
	{
		o = getopt_long(argc, argv, "d:s:p:n:u:hv", long_options, NULL);
		if (o == -1) break;

		switch (o)
		{
			case CAPTURE_DEV:
				opt->dev_name = optarg;
				break;

			case CAPTURE_SUBDEV:
				opt->subdev_name = optarg;
				break;

			case CAPTURE_COUNT:
				opt->capture_count = atoi(optarg);
				if (opt->capture_count <= 0)
				{
					LOGS_ERR("Unable to set capture count to %d using default %d",
						opt->capture_count, DEFAULT_COUNT);
					opt->capture_count = DEFAULT_COUNT;
				}
				break;

			case PROGRAM_USE:
				/*
				 * Search the queue for the user requested progam use.
				 * If a matching name is found save the pointer to that routine.
				 */
				TAILQ_FOREACH(program_use, &g_program_options.usage_head, usage_entry)
				{
					if (strcmp(optarg, program_use->name) == 0)
					{
						found = true;
						opt->program_use = program_use;
						break;
					}
				}
				if (!found)
				{
					printf("unknown program use %s\n", optarg);
					usage(argv);
					return -1;
				}
				break;

			case 'v':
				if (optarg) VERBOSE = atoi(optarg);
				else   		VERBOSE = LOG_ALL;
				break;

			case '?':
			case 'h':
				usage(argv);
				exit(0);
				break;

			default:
				printf("unknown argument %c", o);
				usage(argv);
				return -1;
		}
	}
	return 0;
}

/**
 * Parse command line options and run the test that the user selects.
 */
int main(int argc, char * const argv[])
{
	struct capture_context cap_context;
	struct capture_context *cap = &cap_context;
	struct display_context disp_context;
	struct display_context *disp = &disp_context;
	int ret = 0;


	memset(disp, 0, sizeof(*disp));
	memset(cap, 0, sizeof(*cap));

	ret = get_options(&g_program_options, argc, argv);
	if (ret) return ret;

	ret = g_program_options.program_use->function(cap, disp, &g_program_options);

	return ret;
}

