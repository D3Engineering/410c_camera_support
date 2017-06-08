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
 * OpenGLES Display Support.
 * @file display.h
 *
 */
#ifndef DISPLAY_H__
#define DISPLAY_H__

#include "options.h"

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct display_context;

/** Set an arbitrary maximum number of buffers to the render call. */
#define MAX_RENDER_BUFFERS 32
/**
 * Set an arbitrary maximum number GPU handles in device structures.
 * @note This value is not represetative of limits in OpenGL or the hardware.
 */
#define MAX_DISPLAY_OBJECTS 16

/**
 * Display event loop callbacks.
 * Functions called by the display event loop or render functions.
 */
struct event_callbacks
{
	/**
	 * key_event called for key press events capture by the display.
	 * @param keys array of currently pressed keys in ascii
	 * @param num_keys number of valid elements in the keys array
	 * @param display_context data management structure for display routines
	 */
	void (*key_event)(char keys[], int num_keys, struct display_context*);
	/** context returned to callback functions via the display context. */
	void *private_context;
};

/**
 * Inputs to the render display routine.
 */
struct render_context
{
	/** size of the buffers array. */
	int num_buffers;
	/** array of pointers to memory mapped video planes to display. */
	void *buffers[MAX_RENDER_BUFFERS];
};

/**
 * Function pointer to draw the buffer to the screen.
 * Each program use may require a different render function.
 * @param display_context data management structure for display routines.
 */
typedef int (*RENDER) (struct display_context*);

/**
 * Data management structure for display routines.
 * Contains internal render and window management values.
 */
struct display_context
{
	/** handle to the native (x11) display. */
	EGLNativeDisplayType egl_native_display;
	/** handle to the native (x11) window. */
	EGLNativeWindowType  egl_native_window;
	/** handle to the surface that will be drawn on for EGL API. */
	EGLSurface egl_surface;
	/** handle to the display that will be drawn on for EGL API. */
	EGLDisplay egl_display;

	/** Height of the surface to be drawn on. */
	EGLint height;
	/** Width of the surface to be drawn on. */
	EGLint width;

	/** Handle to the vertex array, or collection of indices and vertices. */
	GLuint vertex_array;
	/** List of handles to vertex buffers, vertices and indices stored in GPU memory. */
	GLuint vertex_buffers[MAX_DISPLAY_OBJECTS];
	/** List of texture handles stored in GPU memory, video frames will be copied here. */
	GLuint texture[MAX_DISPLAY_OBJECTS];
	/** List of uniform reference locations for passing information to the shader program */
	GLint location[MAX_DISPLAY_OBJECTS];
	/** The handle to the compiled shader program */
	GLuint program;

	/** Functions pointers called by the display event loop or render functions. */
	struct event_callbacks callbacks;

	/** Function pointer to the routine that updates the EGL surface. */
	RENDER render_func;
	/** Structure data used by the render_func to complete the surface draw update. */
	struct render_context render_ctx;
};

/**
 * Display and GPU setup for a YUV420 texture display.
 * Setup a full screen window and utilize EGL and OpenGLES to setup the display_context.
 * Compiles the shader program so that the application may render frames on the display surface.
 *
 * @param disp Display Data management structure with GPU handles.
 * @param render_ctx contains display buffers that may be used for initial setup.
 * @note disp->render is assigned for the caller for the display render routine.
 * @return error status of the setup. Value 0 is returned on success.
 */
int camera_nv12m_setup(struct display_context* disp, struct render_context *render_ctx);


/**
 * Initialize EGL drawing surface attached to the native window.
 *
 * @param disp Display Data management structure with GPU handles.
 * @return error status of the setup. Value 0 is returned on success.
 */
int egl_init(struct display_context *disp);

/**
 * Display event loop, check for occuring events and send callbacks based on the events.
 * @param disp Display Data management structure with GPU handles.
 * @return Value 1 is returned if 'q' has been pressed indicating a request to quit the application.
 */
int x11_process_pending_events(struct display_context *disp);

/**
 * Create a full screen native window and setup events to watch.
 * @param disp Display Data management structure with GPU handles.
 * @return error status of the setup. Value 0 is returned on success.
 */
int x11_create_window(struct display_context *disp);

/**
 * Close the native window and display.
 * @param disp Display Data management structure with GPU handles.
 * @return error status of the setup. Value 0 is returned on success.
 */
int x11_close_display(struct display_context *disp);

#endif