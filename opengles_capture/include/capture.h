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
 * V4L2 Capture structure definitions.
 * @file capture.h
 */
#ifndef CAPTURE_H__
#define CAPTURE_H__

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

/**
 * Hold refernces to the memory mapped buffers from V4L2.
 * Each instance of this structure represents one V4L2 buffer and all its planes.
 * Also holds the DMA buffer file descriptors for DMA exports.
 */
struct video_buf_map {
	/** Length of memory mapped calls per video plane. */
	uint32_t length[VIDEO_MAX_PLANES];
	/** User space memory address of mapped video planes. */
	void *addr[VIDEO_MAX_PLANES];
	/** DMA file descriptor of exported video planes. */
	int dma_buf_fd[VIDEO_MAX_PLANES];
	/** Plane descriptor for each v4l2buf. */
	struct v4l2_plane v4l2planes[VIDEO_MAX_PLANES];
	/** Copy of the v4l2 video buffer structure. */
	struct v4l2_buffer v4l2buf;
};

/**
 * State table for camera focus controls.
 */
enum
{
	/** Focus is reset to home position and no focus control is running. */
	IDLE_FOCUS,
	/** Continuous auto focus is running. */
	AUTO_FOCUS_ENABLED,
	/** A single shot focus was run and the focus position is held there. */
	SINGLE_FOCUS_START,
	/** Focus is held at the last position when this state is commanded. */
	FOCUS_PAUSE,
};

/**
 * State information for video capture and associated callbacks.
 */
struct application_state
{
	/** The current focus control state. */
	int focus_state;
	/** Current camera test pattern or live view mode */
	int test_state;
};

/**
 * Data management structure for video capture routines.
 * Contians V4L2 buffer mappings, modes, and camera application state.
 */
struct capture_context {
	/** V4L2 buffer and memory map information for each allocated frame */
	struct video_buf_map buffers[VIDEO_MAX_FRAME];
	/** State information for video capture and associated callbacks. */
	struct application_state app;
	/** Number of buffers allocated in buffers member. */
	int num_buf;
	/** Number of planes required for capture video standard. */
	int num_planes;
	/** V4L2 capture API used: Single, multiple plane. */
	enum v4l2_buf_type type;
	/** V4L2 buffer allocation method: memory map, DMA, user pointer. */
	enum v4l2_memory memory;
	/** Video device file descriptor, capture is performed with this device. */
	int v4l2_fd;
	/** Video subdevice file descriptor used for  focus control, test patterns and other device options. */
	int v4l2_subdev_fd;
};


/**
 * Setup V4L2 capture device, OpenGL display, and intitate video streaming.
 * The base function to perform the capture and display test program.
 *
 * @param cap_ctx Capture data management structure with V4L2 buffer mapping.
 * @param disp_ctx Display data management sturcture with OpenGL refernces.
 * @param opt User selected progam configuration options.
 * @return error status of the setup. Value 0 is returned on success.
 */
int capture_and_display(void* cap_ctx, void* disp_ctx, struct options* opt);

/**
 * Setup V4L2 capture device for streaming and allocate buffers.
 *
 * @param cap Capture data management structure with V4L2 buffer mapping.
 * @param opt User seelcted progam configuration options.
 * @return error status of the setup. Value 0 is returned on success.
 */
int capture_setup(struct capture_context *cap, struct options *opt);

/**
 * Stop V4L2 streaming, unmap video buffers, and release kernel buffers.
 *
 * @param cap Capture data management structure with V4L2 buffer mapping.
 * @param opt User seelcted progam configuration options.
 * @return error status of the shutdown. Value 0 is returned on success.
 */
int capture_shutdown(struct capture_context *cap);

#endif
