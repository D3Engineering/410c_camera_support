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
 * V4L2 Capture and OpenGL Display application.
 * @file capture.c
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
#include <signal.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

#include "options.h"
#include "capture.h"
#include "display.h"
#include "log.h"

/**
 * Exit signal control to free kernel buffers.
 * volatile to ensure that the variable is not optimized out.
 * Read and write occur in separate execution threads.
 */
static volatile int signal_quit = 0;


/**
 * Print v4l2 buffer information.
 * @param buf Pointer to V4L2 buffer allocation.
 * @param type V4L2 memory type used for the buffer.
 */
void print_v4l2_buffer(struct v4l2_buffer *buf, uint32_t type)
{
	LOGS_DBG("Buffer #%d", buf->index);

	LOGS_DBG("\ttype:   %d", buf->type);
	LOGS_DBG("\tmemory: %d", buf->memory);
	if (buf->m.planes != 0)
	{
		for (unsigned int p = 0; p < buf->length; p++)
		{
			LOGS_DBG("\t - Plane#: %d",  p);
			LOGS_DBG("\t - used:   %d",  buf->m.planes[p].bytesused);
			LOGS_DBG("\t - length: %d",  buf->m.planes[p].length);
			if (type == V4L2_MEMORY_MMAP) {
				LOGS_DBG("\t - offset: %d",  buf->m.planes[p].m.mem_offset);
			} else if (type == V4L2_MEMORY_USERPTR) {
				LOGS_DBG("\t - usrptr: %lu", buf->m.planes[p].m.userptr);
			} else if (type == V4L2_MEMORY_DMABUF) {
				LOGS_DBG("\t - fd:     %d",  buf->m.planes[p].m.fd);
			}
		}
	}
}

/**
 * Exit signal control to free kernel buffers.
 * The kernel driver doesn't free outstanding buffers when the device is closed.
 * The signal will catch a ctrl-C exit and request that the appliction loop terminates.
 */
static void signal_exit(int signal)
{
	(void)signal;
	signal_quit = true;
}

/**
 * Release memory map of buffer planes and close DMA export buffers.
 * @param cap Capture data management structure with V4L2 buffer mapping.
 * @return result of the kernel request to release V4L2 buffers.
 */
int unmap_buffers(struct capture_context *cap)
{
	struct v4l2_requestbuffers req;
	int ret = 0;

	/* Release all planes in all buffers. */
	for (int i = 0; i < cap->num_buf; i++)
	{
		for (int p = 0; p < cap->num_planes; p++)
		{
			if (cap->buffers && cap->buffers[i].dma_buf_fd[p] >= 0)
			{
				/* Close assigned DMA file descriptors and set them to an invalid number. */
				close(cap->buffers[i].dma_buf_fd[p]);
				cap->buffers[i].dma_buf_fd[p] = -1;
			}
			if (cap->buffers &&
				cap->buffers[i].addr[p] != 0 &&
				cap->buffers[i].addr[p] != MAP_FAILED)
			{
				/* Unmap any buffers with a valid address */
				ret = munmap(cap->buffers[i].addr[p], cap->buffers[i].length[p]);
			}
		}
	}

	/*
	 * Ask for 0 buffers from the video driver.
	 * Causes the kernel to free any allocated V4L2 buffers. Buffers must be unmapped first.
	 * The kernel driver doesn't always release them when the file descriptor is closed.
	 * This is necessary to prevent memory leaks.
	 */
	memset(&req, 0, sizeof(req));
	req.count = 0;
	req.type = cap->type;
	req.memory = cap->memory;
	// free buffers
	ret = ioctl(cap->v4l2_fd, VIDIOC_REQBUFS, &req);
	return ret;
}

/**
 * Map each video plane to user space buffers. Optionally export DMA file descriptors.
 * @param cap Capture data management structue with V4L2 buffer mapping.
 * @param dma_export Flag to request that DMA exports for each plane are performed.
 * @return error status of the map. Value 0 is returned on success.
 */
int map_buffers(struct capture_context *cap, bool dma_export)
{
	struct v4l2_buffer *buf;
	struct v4l2_exportbuffer dma_buf;
	int ret = 0;

	for (int i = 0; i < cap->num_buf; i++)
	{
		/** For each allocated buffer, request the v4l2 information */
		buf = &cap->buffers[i].v4l2buf;
		buf->m.planes = cap->buffers[i].v4l2planes;
		buf->length = cap->num_buf;
		buf->type = cap->type;
		buf->memory = cap->memory;
		buf->index = i;

		ret = ioctl(cap->v4l2_fd, VIDIOC_QUERYBUF, buf);
		if (ret < 0)
		{
			LOGS_ERR("Unable to query buffer %s", strerror(ret));
			return ret;
		}

		print_v4l2_buffer(buf, cap->type);
		for (int p = 0; p < cap->num_planes; p++)
		{
			/* Separately memory map each plane to its own user space address */
			cap->buffers[i].length[p] = buf->m.planes[p].length;
			cap->buffers[i].addr[p] = mmap(NULL, buf->m.planes[p].length,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				cap->v4l2_fd, buf->m.planes[p].m.mem_offset);
			if (cap->buffers[i].addr[p] == MAP_FAILED)
			{
				LOGS_ERR("Unable to mmap buffer. Error #%d: %s", errno, strerror(errno));
				return -errno;
			}

			if (dma_export)
			{
				/* Export a DMA file descriptor for each plane when requested */
				dma_buf.type = cap->type;
				dma_buf.index = i;
				dma_buf.plane = p;
				ret = ioctl(cap->v4l2_fd, VIDIOC_EXPBUF, &dma_buf);
				if (ret < 0)
				{
					LOGS_ERR("Unable to export dma buffer. Error #%d: %s", errno, strerror(errno));
					return -errno;
				}

				cap->buffers[i].dma_buf_fd[p] = dma_buf.fd;
			}
		}
	}
	return 0;
}


/**
 * Queue all allocated V4L2 buffers in the driver to prepare for capture.
 * @param fd file descriptor for the V4L2 device.
 * @param count number of buffers in the array to queue.
 * @param buffers array of v4l2 buffer to queue.
 * @return error status of the function. Value 0 is returned on success.
 */
int queue_buffers(int fd, int count, struct video_buf_map buffers[])
{
	int ret = 0;
	for (int i = 0; i < count; i++)
	{
		ret = ioctl(fd, VIDIOC_QBUF, &buffers[i].v4l2buf);
		if (ret < 0)
		{
			LOGS_ERR("Unable to queue buffer %d", errno);
			break;
		}
	}
	return ret;
}

/**
 * Start the v4l2 video stream.
 * @param fd file descriptor for the V4L2 device.
 * @return error status of the function. Value 0 is returned on success.
 */
int start_stream(int fd)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	return ioctl(fd, VIDIOC_STREAMON, &type);
}

/**
 * Stop the v4l2 video stream.
 * @param fd file descriptor for the V4L2 device.
 * @return error status of the function. Value 0 is returned on success.
 */
int stop_stream(int fd)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	return ioctl(fd, VIDIOC_STREAMOFF, &type);
}

/**
 * Video capture and display loop.
 * Dequeue v4l2 buffers, send the mapped buffers to render then re-queue the buffer.
 * @param cap Capture data management structure with V4L2 buffer mapping.
 * @param disp Display Data management structure with GPU handles.
 * @return error status of the function. Value 0 is returned on success.
 */
int capture_display_yuv(struct capture_context *cap, struct display_context *disp)
{
	int ret = 0;
	struct v4l2_buffer buf;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	memset(&buf, 0, sizeof(buf));
	/* initilize the buffer type reference to hold the dequeued buffer info. */
	buf.type = cap->type;
	buf.memory = cap->memory;
	buf.length = cap->num_planes;
	buf.m.planes = planes;

	/* Select an empty buffer for priming the video display */
	disp->render_ctx.num_buffers = cap->num_planes;
	for (int i = 0; i < cap->num_planes; i++)
	{
		disp->render_ctx.buffers[i] = cap->buffers[0].addr[i];
	}

	/* Setup the OpenGL display, disp->render will be assigned for future display calls */
	ret = camera_nv12m_setup(disp, &disp->render_ctx);
	if (ret)
	{
		LOGS_ERR("Error setting up display aborting capture");
		return -1;
	}

	/* Continue until an error occurs or external signal requests an exit */
	while(!ret && !signal_quit)
	{
		ret = ioctl(cap->v4l2_fd, VIDIOC_DQBUF, &buf);
		if (ret < 0) {
			LOGS_ERR("DQBUF: %d - %s", errno, strerror(errno));
			return errno;
		}
		/* use the buffer index returned from dequeue to select the memory map planes for rendering */
		disp->render_ctx.num_buffers = cap->num_planes;
		for (int i = 0; i < cap->num_planes; i++)
		{
			disp->render_ctx.buffers[i] = cap->buffers[buf.index].addr[i];
		}
		/*
		 * Render the planes to the display.
		 * Return value inidcates error or request to exit the capture-display loop.
		 */
		ret = disp->render_func(disp);
		if (ret < 0) {
			LOGS_ERR("Error during display aborting capture");
			return errno;
		} if (ret > 0) {
			LOGS_INF("Exiting display loop normally");
			break;
		}

		/* Requeue the last buffer, the memory should be duplicated in the GPU and no longer needed. */
		ret = ioctl(cap->v4l2_fd, VIDIOC_QBUF, &buf);
	}
	return 0;
}

/**
 * Open the video capture subdevice, used for test pattern and focus control.
 *
 * @param device file system path to a v4l2 subdevice.
 * @return file descriptor of the v4l2 subdevice or error when negative.
 */
int get_subdevice(const char* device)
{
	int fd;
	fd = open(device, O_RDWR);
	if (fd < 0) {
		LOGS_ERR("Unable to open device %s: %s", device, strerror(errno));
		return -1;
	}
	return fd;
}

/**
 * Open the video capture device, used for streaming.
 * MPLANE API is used throughout the application, ensure that video streaming and MPLANE API are supported.
 *
 * @param device file system path to a v4l2 capture device.
 * @return file descriptor of the v4l2 capture device or error when negative.
 */
int get_device(const char* device)
{
	int fd;
	struct v4l2_capability cap;
	fd = open(device, O_RDWR);
	if (fd < 0) {
		LOGS_ERR("Unable to open device %s: %s", device, strerror(errno));
		return -1;
	}

	ioctl(fd, VIDIOC_QUERYCAP, &cap);

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
		LOGS_ERR("device does not have multiple plane capture capabilities");
		fd = -1;
	}
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		LOGS_ERR("device does not have streaming capabilities");
		fd = -1;
	}
	return fd;
}

/**
 * Stop capture streaming and release buffers to avoid memory leaks.
 *
 * Unmap the v4l2 buffers then release them in the kernel.
 * The kernel driver doesn't free outstanding buffers when the device is closed.
 *
 * @param cap Capture data management structure with V4L2 buffer mapping.
 * @return rror status of the shutdown. Value 0 is returned on success.
 */
int capture_shutdown(struct capture_context *cap)
{
	stop_stream(cap->v4l2_fd);
	return unmap_buffers(cap);
}

/**
 * Setup V4L2 capture device for streaming and allocate buffers.
 *
 * @param cap Capture data management structure with V4L2 buffer mapping.
 * @param opt User seelcted progam configuration options.
 * @return error status of the setup. Value 0 is returned on success.
 */
int capture_setup(struct capture_context *cap, struct options *opt)
{
	int ret = 0;
	struct v4l2_requestbuffers req;
	struct v4l2_format fmt = {0};

	/*
	 * MPLANE API is used by the application.
	 * NV12 is used by the render routine which has two planes.
	 * First plane is luma, second plane is chroma at 1/4 resolution.
	 */
	cap->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	cap->memory = V4L2_MEMORY_MMAP;
	cap->num_planes = 2;

	/*
	 * Framezises should be queried and a valid format chosen.
	 * This application is forcing 1080p for the sensor and display.
	 */
	fmt.type = cap->type;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
	fmt.fmt.pix_mp.num_planes = cap->num_planes;
	fmt.fmt.pix_mp.width = 1920;
	fmt.fmt.pix_mp.width = 1080;
	ret = ioctl(cap->v4l2_fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0)
	{
		LOGS_ERR("Unable to set format %d", ret);
		exit(-errno);
	}

	/* Request the number of buffers indicated by the user options */
	memset(&req, 0, sizeof(req));
	req.count = opt->buffer_count;
	req.type = cap->type;
	req.memory = cap->memory;
	ret = ioctl(cap->v4l2_fd, VIDIOC_REQBUFS, &req);
	if (ret < 0)
	{
		LOGS_ERR("Unable to request user buffers %d", ret);
		exit(-errno);
	}

	/* Save the number of buffers actually allocated and set the DMA desciptors to invalid descriptors. */
	cap->num_buf = req.count;
	for (int i = 0; i < cap->num_buf; i++)
		for (int p = 0; p < cap->num_planes; p++)
			cap->buffers[i].dma_buf_fd[p] = -1;

	/* Memory map the buffers into user space */
	ret = map_buffers(cap, opt->dma_export);
	if (ret) goto cleanup;

	/* initialize capture by queueing aloctaed buffers before streaming is enabled */
	ret = queue_buffers(cap->v4l2_fd, cap->num_buf, cap->buffers);
	if (ret) goto cleanup;

	/* Start the video stream, this will setup initial settings on the subdevice. */
	ret = start_stream(cap->v4l2_fd);

	return ret;
cleanup:
	capture_shutdown(cap);
	return ret;
}

/**
 * Request the camera subdevice display a test pattern.
 * Cycle through three available test patterns on the sensor.
 *
 * @note STREAM_ON must have occured before this fuction is called.
 * @param cap Capture data management structure with V4L2 buffer mapping.
 * @param state 0 indicates live view, 1 indicates cycle through test patterns.
 * @return error status of the test pattern request. Value 0 is returned on success.
 */
int test_pattern(struct capture_context *cap, int state)
{
	struct v4l2_control ctrl;
	int ret;

	/*
	 * 0 is live view 1-3 are test patterns.
	 * Cycle through test patterns when test pattern mode is chosen.
	*/
	if (state)
	{
		cap->app.test_state = (cap->app.test_state + 1 ) % 4;
		if (cap->app.test_state == 0)
			cap->app.test_state = 1;
		LOGS_INF("Test pattern %d", cap->app.test_state);
	}
	else
	{
		cap->app.test_state = 0;
		LOGS_INF("Live view");
	}
	ctrl.value = cap->app.test_state;
	ctrl.id = V4L2_CID_TEST_PATTERN;
	ret = ioctl(cap->v4l2_subdev_fd, VIDIOC_S_CTRL, &ctrl);
	return ret;
}

/**
 * Request the camera subdevice change focus mode.
 * Walk through the focus mode state machine and select the
 *
 * @note STREAM_ON must have occured before this fuction is called.
 * @param cap Capture data management structure with V4L2 buffer mapping.
 * @param state The type of focus to step into.
 * @return error status of the focus request. Value 0 is returned on success.
 */
int focus_state(struct capture_context *cap, int new_state)
{
	struct v4l2_control ctrl;
	int ret;
	switch (cap->app.focus_state)
	{
		/* Focus is reset to home position and no focus control is running. */
		case IDLE_FOCUS:
			if (new_state == AUTO_FOCUS_ENABLED)
			{
				/* Enable continuos auto focus */
				cap->app.focus_state = AUTO_FOCUS_ENABLED;
				ctrl.value = 1;
				ctrl.id = V4L2_CID_FOCUS_AUTO;
				LOGS_INF("Focus auto");
			}
			else if (new_state == SINGLE_FOCUS_START)
			{
				/* perform single focus and keep that focus position. */
				cap->app.focus_state = SINGLE_FOCUS_START;
				ctrl.value = 1;
				ctrl.id = V4L2_CID_AUTO_FOCUS_START;
				LOGS_INF("Focus single");
			}
			break;
		/* Continuous auto focus is running. */
		case AUTO_FOCUS_ENABLED:
			if (new_state == AUTO_FOCUS_ENABLED)
			{
				/* disable auto focus and return to idle. */
				cap->app.focus_state = IDLE_FOCUS;
				ctrl.value = 0;
				ctrl.id = V4L2_CID_FOCUS_AUTO;
				LOGS_INF("Focus disable");
			}
			else if (new_state == FOCUS_PAUSE)
			{
				/* hold focus in its current position. */
				cap->app.focus_state = FOCUS_PAUSE;
				ctrl.value = V4L2_LOCK_FOCUS;
				ctrl.id = V4L2_CID_3A_LOCK;
				LOGS_INF("Focus pause");
			}
			else if (new_state == SINGLE_FOCUS_START)
			{
				/* perform single focus and keep that focus position. */
				cap->app.focus_state = SINGLE_FOCUS_START;
				ctrl.value = 1;
				ctrl.id = V4L2_CID_AUTO_FOCUS_START;
				LOGS_INF("Focus single");
			}
			break;
		/* A single shot focus was run and the focus position is held there. */
		case SINGLE_FOCUS_START:
			if (new_state == SINGLE_FOCUS_START)
			{
				/* perform single focus and keep that focus position. */
				cap->app.focus_state = SINGLE_FOCUS_START;
				ctrl.value = 1;
				ctrl.id = V4L2_CID_AUTO_FOCUS_START;
				LOGS_INF("Focus single");
			}
			else if (new_state == FOCUS_PAUSE)
			{
				/* hold focus in its current position. */
				cap->app.focus_state = FOCUS_PAUSE;
				ctrl.value = V4L2_LOCK_FOCUS;
				ctrl.id = V4L2_CID_3A_LOCK;
				LOGS_INF("Focus pause");
			}
			else if (new_state == AUTO_FOCUS_ENABLED)
			{
				/* Enable continuos auto focus. */
				cap->app.focus_state = AUTO_FOCUS_ENABLED;
				ctrl.value = 1;
				ctrl.id = V4L2_CID_FOCUS_AUTO;
				LOGS_INF("Focus auto");
			}
			break;
		/* Focus is held at the last position when this state is commanded. */
		case FOCUS_PAUSE:
			if (new_state == AUTO_FOCUS_ENABLED)
			{
				/* Enable continuos auto focus. */
				cap->app.focus_state = AUTO_FOCUS_ENABLED;
				ctrl.value = 1;
				ctrl.id = V4L2_CID_FOCUS_AUTO;
				LOGS_INF("Focus auto");
			}
			else if (new_state == SINGLE_FOCUS_START)
			{
				/* perform single focus and keep that focus position. */
				cap->app.focus_state = SINGLE_FOCUS_START;
				ctrl.value = 1;
				ctrl.id = V4L2_CID_AUTO_FOCUS_START;
				LOGS_INF("Focus single");
			}
			break;
		default:
			cap->app.focus_state = IDLE_FOCUS;
			break;
	}
	ret = ioctl(cap->v4l2_subdev_fd, VIDIOC_S_CTRL, &ctrl);
	LOGS_DBG("Focus command result is %d", ret);
	return ret;
}

void print_key_functions()
{
	LOGS_INF("Keyboard Shortcuts");
	LOGS_INF("q - quit application.");
	LOGS_INF("a - Toggle between continuous auto focus and no focus control.");
	LOGS_INF("f - Run a single auto focus and hold.");
	LOGS_INF("p - Hold focus at the point when the button is pressed.");
	LOGS_INF("t - Cycle through three sensor test patterns.");
	LOGS_INF("l - Select sensor live view.");
	LOGS_INF("h - Print this menu.");
}

/**
 * Handle keyboard presses.
 * Run camera focus modes and test pattern selection based on keyboard input.
 *
 * @note STREAM_ON must have occured before this fuction is called.
 * @param keys array of currently pressed keys in ascii.
 * @param num_keys number of valid elements in the keys array.
 * @param display_context data management structure for display routines.
 */
void do_key_event(char keys[], int num_keys, struct display_context* disp)
{
	struct capture_context* cap = disp->callbacks.private_context;
	if (num_keys == 1)
	{
		switch (keys[0])
		{
			case 'h':
				print_key_functions();
				break;
			case 'a':
				focus_state(cap, AUTO_FOCUS_ENABLED);
				break;
			case 'f':
				focus_state(cap, SINGLE_FOCUS_START);
				break;
			case 'p':
				focus_state(cap, FOCUS_PAUSE);
				break;
			case 't':
				test_pattern(cap, 1);
				break;
			case 'l':
				test_pattern(cap, 0);
				break;
			default:
				break;
		}
	}
}

/**
 * Setup V4L2 capture device, OpenGL display, and intitate video streaming.
 * The base function to perform the capture and display test program.
 *
 * @param cap_ctx Capture data management structure with V4L2 buffer mapping.
 * @param disp_ctx Display data management sturcture with OpenGL refernces.
 * @param opt User selected progam configuration options.
 * @return error status of the setup. Value 0 is returned on success.
 */
int capture_and_display(void* cap_ctx, void* disp_ctx, struct options* opt)
{
		struct capture_context* cap = cap_ctx;
		struct display_context* disp = disp_ctx;
		struct sigaction sa;
		int ret;

		/* initialize application state after STREAM_ON. */
		cap->app.focus_state = AUTO_FOCUS_ENABLED;
		cap->app.test_state = 0;

		/* open the video device for capture. */
		cap->v4l2_fd = get_device(opt->dev_name);
		if (cap->v4l2_fd < 0) {
			exit(1);
		}
		/* open the camera sensor subdevice for controls. */
		cap->v4l2_subdev_fd = get_subdevice(opt->subdev_name);
		if (cap->v4l2_subdev_fd < 0) {
			exit(1);
		}

		/* Setup the v4l2 device and start streaming. */
		ret = capture_setup(cap, opt);
		if (ret)
		{
			LOGS_ERR("Unable to start capture stream");
			capture_shutdown(cap);
			return ret;
		}

		/**
		* Setup a signal handler to catch ctrl-c.
		* unmap and free the kernel buffers to prevent a memory leak.
		*/
		sa.sa_handler = signal_exit;
		sigemptyset (&sa.sa_mask);
		sa.sa_flags = 0;
		sigaction(SIGINT, &sa, NULL);

		/* setup the display event callback functions and context */
		disp->callbacks.key_event = do_key_event;
		disp->callbacks.private_context = cap;

		/* Enter the capture display loop */
		ret = capture_display_yuv(cap, disp);
		/* Cleanly release the buffers map and free them in the kernel on either error or exit request. */
		capture_shutdown(cap);

		return ret;
}
