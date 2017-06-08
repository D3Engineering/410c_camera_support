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
 * @file display.c
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/dpms.h>

#include <GLES3/gl3.h>
#include <GLES3/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <libdrm/drm_fourcc.h>

#include "display.h"
#include "gles_egl_util.h"
#include "log.h"

/**
 * Vertex shader, first stage produce texture and render positions.
 * Shaders included in code for simplicity but could be external files.
 */
static const char nv12_vertex_code[] =
	"#version 300 es\n"
	"//VERTEX_SHADER\n"
	"layout(location = 0) in vec4 a_position;\n"
	"layout(location = 1) in vec2 a_tex_coord;\n"
	"out vec2 v_tex_coord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = a_position;\n"
	"   v_tex_coord = a_tex_coord;\n"
	"}\n";
/**
 * fragment shader, second stage draw camera texture on each point in the window.
 * Shaders included in code for simplicity but could be external files.
 */
static const char nv12_fragment_code[] =
	"#version 300 es\n"
	"//FRAGMENT_SHADER\n"
	"precision mediump float;\n"
	"// position lookup used for both textures\n"
	"in vec2 v_tex_coord;\n"
	"// Output of the shader, RGB color per position in surface\n"
	"layout(location = 0) out vec4 out_color;\n"
	"// Additive offset for BT656 YUV to RGB transform.\n"
	"const vec3 offset = vec3(0, -0.5, -0.5);\n"
	"// Column order vector gain for BT656 YUV to RGB transform\n"
	"\n"
	"const mat3 coeff = mat3(\n"
	"    1.0,      1.0,     1.0,\n"
	"    0.0,     -0.344,  1.722,\n"
	"    1.402,   -0.714,   0);\n"
	"// Temporary variable for YUV value\n"
	"vec3 yuv;\n"
	"// Temporary variable for RGB value\n"
	"vec3 rgb;\n"
	"// Two used textures to hold the YUV420 Semi semi-planar data\n"
	"// The luma texture holds three copies of the Y value in fields r,g,b or x,y,z, and 1.0 for w\n"
	"uniform sampler2D s_luma_texture;\n"
	"// The chroma texture holds three copies of the CR value in fields or x,y,z, and CB in w\n"
	"uniform sampler2D s_chroma_texture;\n"
	"void main()\n"
	"{\n"
	"    // Lookup the texture at the  position in the render window.\n"
	"    // Take the two semi-planar textures assemble into single YUV pixel.\n"
	"    yuv.x = texture2D(s_luma_texture, v_tex_coord).x;\n"
	"    yuv.yz = texture2D(s_chroma_texture, v_tex_coord).xw;\n"
	"    // Then convert to YUV to RGB using offset and gain.\n"
	"    yuv += offset;\n"
	"    rgb = coeff * yuv;\n"
	"    out_color = vec4(rgb,1);\n"
	"}\n";



/**
 * Create a full screen native window and setup events to watch.
 * @param disp Display Data management structure with GPU handles.
 * @return error status of the setup. Value 0 is returned on success.
 */
int x11_create_window(struct display_context *disp)
{
	Display *x11_disp = NULL;
	Window win_root;
	XSetWindowAttributes  xattr = {0};
	Window win;

	// Open the default display
	x11_disp = XOpenDisplay(NULL);
	if ( x11_disp == NULL )
	{
		return -1;
	}

	// save access to the top level window
	win_root = DefaultRootWindow(x11_disp);

	// allow the window manager to control the window that will be created.
	xattr.override_redirect = false;
	// Announce events when the keyboard is pressed or the window state changes.
	xattr.event_mask = ExposureMask | KeyPressMask;
	// Set the background to white, making it clear when X has the window cleared.
	xattr.background_pixel = XWhitePixel(x11_disp, XDefaultScreen(x11_disp));

	// create the native window based on the attributes selected an resolution passed in.
	win = XCreateWindow(
		x11_disp, win_root,
		0, 0, disp->width, disp->height, 0,
		CopyFromParent, InputOutput,
		CopyFromParent, CWOverrideRedirect | CWEventMask | CWBackPixel,
		&xattr);

	// Set the window properties.
	XSetStandardProperties(
		x11_disp, win,
		"Camera","Camera",
		None, NULL,
		0, NULL);

	// Disable the display power management to turn keep the display on.
	DPMSDisable(x11_disp);

	// Set the window to the background pixel and display it to the front.
	XClearWindow(x11_disp, win);
	XMapRaised (x11_disp, win);

	// Request that the window becomes full screen and boarderless.
	// It will take some time for the display to acknowledge the event.
	// An Expose event will occur when this is completed.
	Atom wm_state = XInternAtom(x11_disp, "_NET_WM_STATE", false);
	Atom fullscreen = XInternAtom(x11_disp, "_NET_WM_STATE_FULLSCREEN", false);

	XEvent fs_event;
	memset(&fs_event, 0, sizeof(fs_event));
	fs_event.type = ClientMessage;

	fs_event.type = ClientMessage;
	fs_event.xclient.window = win;
	fs_event.xclient.message_type = wm_state;
	fs_event.xclient.format = 32;
	fs_event.xclient.data.l[0] = 1;
	fs_event.xclient.data.l[1] = fullscreen;
	fs_event.xclient.data.l[2] = 0;

	XSendEvent(x11_disp, win_root, false,
		SubstructureRedirectMask | SubstructureNotifyMask,
		&fs_event);

	// Save references to the native display and window type in the display context.
	disp->egl_native_display = x11_disp;
	disp->egl_native_window = win;

	return 0;
}

/**
 * Close the native window and display.
 * @param disp Display Data management structure with GPU handles.
 * @return error status of the setup. Value 0 is returned on success.
 */
int x11_close_display(struct display_context *disp)
{
	// Retrieve the native (x11) window and display references.
	Display *x11_disp = (Display*)disp->egl_native_display;
	Window win = (Window)disp->egl_native_window;


	if (x11_disp)
	{
		/* Turn display power management back on. */
		DPMSEnable(x11_disp);
		/* Close the application window and release display handle. */
		if (win) XDestroyWindow(x11_disp, win);
		XCloseDisplay(x11_disp);
	}

	return 0;
}

int egl_init(struct display_context *disp)
{
	EGLBoolean ret;
	EGLint major, minor;
	EGLDisplay egl_disp;
	EGLint num_configs;
	EGLContext context;

	/* Request an EGL v2.x context */
	EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	/*
	  * Request a window capable of OpenGL ES 3.
	  * Alpha blending is not used in the chosen shader and is not requested here.
	  * Stencils are not used and also omitted here.
	  */
	EGLint config_attribs[] = {
		EGL_RED_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_STENCIL_SIZE, 0,
		EGL_ALPHA_SIZE, 0,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_NONE };

		/* Two X11 categories on dragonboard
		other key differentiators: EGL_DEPTH_SIZE, EGL_STENCIL_SIZE, EGL_ALPHA_SIZE
		EGL_NATIVE_VISUAL_ID: 419 (0x1a3)
		EGL_NATIVE_VISUAL_TYPE: 5 (0x5)

		EGL_NATIVE_VISUAL_ID: 33 (0x21)
		EGL_NATIVE_VISUAL_TYPE: 4 (0x4)
		*/

	// Retrieve the EGL display reference that matches the native display chosen earlier.
	egl_disp = eglGetDisplay(disp->egl_native_display);
	if (egl_disp == EGL_NO_DISPLAY)
	{
		LOGS_ERR("Unable to get EGL Display for X11 window");
		return -1;
	}
	/* save the EGL display to the display context */
	disp->egl_display = egl_disp;

	// Intialize the EGL display and retrieve the running EGL version number.
	ret = eglInitialize(egl_disp, &major, &minor);
	if (ret == EGL_FALSE) {
		LOGS_ERR("Unable to initialize egl %s", string_egl_error(eglGetError()));
		return -1;
	}

	/*
	 * Different rendering APIs are available for EGL. This application uses OpenGL ES.
	 * OpenGL ES is the default. Unless an eglBindAPI has been called or there is an error in the drivers.
	 * Return an error if a different API is being used.
	 */
	EGLenum api = eglQueryAPI();
	switch(api)
	{
		case EGL_OPENGL_API: LOGS_INF("EGL API is EGL_OPENGL_API"); return -1;
		case EGL_OPENGL_ES_API: LOGS_INF("EGL API is EGL_OPENGL_ES_API"); break;
		case EGL_OPENVG_API: LOGS_ERR("EGL API is EGL_OPENVG_API"); return -1;
		case EGL_NONE: LOGS_ERR("EGL API is EGL_NONE"); return -1;
		default: LOGS_ERR("EGL API is unknown"); return -1;
	}

	/*
	 * Request the EGL config and GPU setup that will be used for rendering.
	 * This is based on the attributes selected above.
	 * The proper method is to retieve the number of configs that match the
	 * attributes and iterate through all available options and select the ideal version.
	 * The below operation just selects the first choice which may not be optimal.
	 */
	EGLConfig config;
	ret = eglChooseConfig(egl_disp, config_attribs, &config, 1, &num_configs);
	if (ret == EGL_FALSE || num_configs < 1)
	{
		LOGS_ERR("Unable to select config %s", string_egl_error(eglGetError()));
		return -1;
	}

	/*
	 * Create a render surface that matches the native window created previously.
	 * save the EGL surface handle to the display context.
	 */
	disp->egl_surface = eglCreateWindowSurface(egl_disp, config, disp->egl_native_window, NULL);
	if (disp->egl_surface == EGL_NO_SURFACE)
	{
		LOGS_ERR("Unable to create surface %s", string_egl_error(eglGetError()));
		return -1;
	}

	/* Create a EGL render context using API version 2.X. */
	context = eglCreateContext(egl_disp, config, EGL_NO_CONTEXT, context_attribs);
	if (context == EGL_NO_CONTEXT)
	{
		LOGS_ERR("Unable to create context %s", string_egl_error(eglGetError()));
		return -1;
	}

	/* Assign the surface that matches the full screen window as the current render target. */
	ret = eglMakeCurrent(egl_disp, disp->egl_surface, disp->egl_surface, context);
	if (ret == EGL_FALSE)
	{
		LOGS_ERR("Unable to bind surface %s", string_egl_error(eglGetError()));
		return -1;
	}

	/*
	 * Request the current window height and width.
	 * The native window full screen request runs in a different process and may not have completed.
	 * The EGL size may then match the original window size or the full screen size depending on timing.
	 * Ideally this should be queried again after Expose/Resize events are recived in the event loop.
	 */
	ret = eglQuerySurface(egl_disp, disp->egl_surface, EGL_HEIGHT, &disp->height);
	if (ret == EGL_FALSE)
	{
		LOGS_ERR("Unable to query surface %s", string_egl_error(eglGetError()));
		return -1;
	}
	ret = eglQuerySurface(egl_disp, disp->egl_surface, EGL_WIDTH, &disp->width);
	if (ret == EGL_FALSE)
	{
		LOGS_ERR("Unable to query surface %s", string_egl_error(eglGetError()));
		return -1;
	}
	LOGS_DBG("Surface Size %d x %d", disp->width, disp->height);

	/*
	 * Set how many frames are displayed before a buffer swap occurs when eglSwapBuffers is called.
	 * The value 1 waits until vsync is complete before the next buffer is displayed.
	 * A value of 0 will begin displaying independent of vsync as once render is complete.
	 */
	eglSwapInterval(egl_disp, 1);

	return 0;
}

/**
 * Display event loop, check for occuring events and send callbacks based on the events.
 * @param disp Display Data management structure with GPU handles.
 * @return Value 1 is returned if 'q' has been pressed indicating a request to quit the application.
 */
int x11_process_pending_events(struct display_context *disp)
{
	Display *x11_disp = (Display*)disp->egl_native_display;
	int quit = 0;
	char text[11];
	int keys = 0;
	KeySym key_press;
	XEvent event;

	/* Process events, one at a time, until there are none remaining. */
	while (XPending(x11_disp))
	{
		XNextEvent(x11_disp, &event);
		switch (event.type)
		{
			case Expose:
				/* Window resized/hidden/shown etc. */
			break;
			case KeyPress:
				/* Keyboard events occured, get the key sequence, limit to 10 keys total. */
				keys = XLookupString(&event.xkey, text, 10, &key_press, 0);
				if (disp->callbacks.key_event != NULL)
				{
					/* send the events to the application */
					disp->callbacks.key_event(text, keys, disp);
				}
				/* reserve the 'q' key to exit the display loop */
				if (keys == 1 && text[0] == 'q') quit = true;
				break;
			default:
				break;
		}
	}
	return quit;
}

/**
 * Render the next camera frame on the EGL surface using the NV12 shader program
 * Two buffers, seperate luma and chroma planes must be assigned in the disp->render_ctx
 * Setup a full screen window and utilize EGL and OpenGLES to setup the display_context.
 *
 * @param disp Display Data management structure with GPU handles.
 * @return error status of the setup. Value 0 is returned on success.
 */
int render_nv12m_subs_tex(struct display_context *disp)
{
	GLenum error = GL_NO_ERROR;
	int quit = 0;
	EGLBoolean ret = 0;

	/*
	 * Process any events such as window resize before rendering on the window
	 * One or more resize events may occur on startup as the window is switched to full screen mode.
	 */
	quit = x11_process_pending_events(disp);
	if (quit)
	{
		x11_close_display(disp);
		return 1;
	}

	/*
	 * There must be valid YUV420 semi planar data in the render context.
	 * The first buffer must contain luma data and the second must contain the chroma data.
	 * This should be NV12 format with the chroma in CbCr order.
	 */
	if (disp->render_ctx.num_buffers < 2 ||
		!disp->render_ctx.buffers[0] || !disp->render_ctx.buffers[1])
	{
		LOGS_ERR("Unable to continue no buffer address in display render_context\n");
		return -1;
	}

	/*
	 * Set the rendered surface to match the full window resolution.
	 * This routine will render to the entire window.
	 */
	glViewport(0, 0, disp->width, disp->height);
	/*
	 * Only the color buffer is used. (The depth, and stencil buffers are unused.)
	 * Set it to the background color before rendering.
	 */
	glClear(GL_COLOR_BUFFER_BIT);
	/** Select the NV12 Shader program compiled in the setup routine */
	glUseProgram(disp->program);
	error = glGetError();
	if (error != GL_NO_ERROR)
	{
		LOGS_ERR("Use program %s", string_gl_error(error));
		return -1;
	}

	/* Select the vertex array which includes the vertices and indices describing the window rectangle. */
	glBindVertexArray(disp->vertex_array);


	/*
	 * Copy the Luma data from plane 0 to the s_luma_texture texture in the GPU.
	 * Each texture has four components, x,y,z,w alias r,g,b,a alias s,r,t,u.
	 * Luma will be replicated in x, y, and z using type GL_LUMINCANCE. Component w is set to 1.0.
	 * The resolution of the texture matches the number of active pixels.
	 */
	glActiveTexture(GL_TEXTURE0);
	glTexSubImage2D(GL_TEXTURE_2D, 0,
		0, 0, disp->width, disp->height,
		GL_LUMINANCE, GL_UNSIGNED_BYTE, disp->render_ctx.buffers[0]);
	error = glGetError();
	glBindTexture(GL_TEXTURE_2D, disp->texture[0]);
	/* Indicate that GL_TEXTURE0 is s_luma_texture from previous lookup */
	glUniform1i(disp->location[0], 0);

	/*
	 * Copy the chroma data from plane 1 to the s_chroma_texture texture in the GPU.
	 * Each texture has four components, x,y,z,w alias r,g,b,a alias s,r,t,u.
	 * Cb is will be replicated in x, y, and z. Cr will be assigned to w. with type GL_LUMINANCE_ALPHA.
	 * There is one pair of chroma values for every four luma pixels.
	 * Half the vertical resoulution and half the horizontal resolution.
	 * Each GL_LUMINCANCE_ALPHA lookup will contain one pixel of chroma.
	 * The texture lookup will replicate the chroma values up to the total resolution.
	 */
	glActiveTexture(GL_TEXTURE1);
	glTexSubImage2D(GL_TEXTURE_2D, 0,
		0, 0, disp->width/2, disp->height/2,
		GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, disp->render_ctx.buffers[1]);
	error = glGetError();
	glBindTexture(GL_TEXTURE_2D, disp->texture[1]);
	/* Indicate that GL_TEXTURE1 is s_chroma_texture from previous lookup */
	glUniform1i(disp->location[1], 1);

	/*
	 * Draw the two triangles from 6 indices to form a rectangle from the data in the vertex array.
	 * The fourth parameter, indices value here is passed as null since the values are already
	 * available in the GPU memory through the vertex array
	 * GL_TRIANGLES - draw each set of three vertices as an individual trianvle.
	 */
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
	/** Select the default vertex array, allowing the applications array to be unbound */
	glBindVertexArray(0);

	/*
	 * display the new camera frame after render is complete at the next vertical sync
	 * This is drawn on the EGL surface which matches the full screen native window.
	 */
	ret = eglSwapBuffers(disp->egl_display, disp->egl_surface);
	if (ret == EGL_FALSE)
	{
		LOGS_ERR("Unable to update surface %s", string_egl_error(eglGetError()));
	}

	return 0;
}

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
int camera_nv12m_setup(struct display_context* disp, struct render_context *render_ctx)
{
	int ret;
	GLenum error = 0;
	/**
	 * The triangle vetices for the render target and the texture co-ordinates are interleaved.
	 * The triangle co-ordinates are between -1.0 and 1.0 with (0.0, 0.0, 0.0) as the origin.
	 * A flat rectangle the size of the whole window will be drawn so the third dimension is not required.
	 * Two triangles are drawn with the indices shown below on the left and co-ordinates on the right.
	 * @verbatim
	   0 + -- + 3       (-1.0,  1.0, 0.0) + -- + (1.0,  1.0, 0.0)
	     | \  |                           | \  |
	     |  \ |                           |  \ |
	   1 + -- + 2       (-1.0, -1.0, 0.0) + -- + (1.0, -1.0, 0.0)
	  @endverbatim
	 *
	 * The texture co-ordinates use a the lower left as origin and go between 0.0 and 1.0.
	 * The texture co-ordinate boundaries are interleaved with the triangle data for compactness in GPU memory.
	 *
	 */
	GLfloat vertices[] = {
		-1.0f, 1.0f, 0.0f,  /* Triangle 0/1, index 0, upper left */
		0.0f, 0.0f,         /* Texture lower left */
		-1.0f, -1.0f, 0.0f, /* Triangle 0, index 1, lower left */
		0.0f, 1.0f,         /* Texture upper left */
		1.0f, -1.0f, 0.0f,  /* Triangle 0/1, index 2, lower right */
		1.0f, 1.0f,         /* Texture upper right */
		1.0f, 1.0f, 0.0f,   /* Triangle 1, index 3, upper right */
		1.0f, 0.0f          /* Texture lower right */
	};
	GLushort indices[] = {0, 1, 2, 0, 2, 3};

	(void)render_ctx;

	/* force full screen window sizing */
	disp->width = 1920;
	disp->height = 1080;

	/* Create a window and get the native windows and display handles required for EGL init */
	ret = x11_create_window(disp);
	if (ret < 0){
		LOGS_ERR("Unable to create x11 window");
		goto cleanup;
	}

	/* Initialize the EGL buffer API with the native window and display */
	ret = egl_init(disp);
	if (ret < 0)
	{
		LOGS_ERR("Error during egl init");
		goto cleanup;
	}
	/* Process any pending events, this will draw the initial window on the screen */
	x11_process_pending_events(disp);

	/*
	 * Compile the shader program, consisting of both a vertex and fragment shader.
	 * The vertex generates the positions for the current pixel and texture position.
	 * The fragment shader lookups the Luma and chroma textures and converts them to a RGB color per pixel.
	 * RGB is required by the OpenGL renderer on Linux without extensions such as the VPDAU or VAAPI.
	 * The compiled program handle is returned and loaded by the render routine.
	 */
	disp->program = gles_load_program(nv12_vertex_code, nv12_fragment_code);
	if (!disp->program)
	{
		LOGS_ERR("Unable to load program");
		goto cleanup;
	}

	/*
	 * Get a handle to the s_luma_texture variable in the fragment shader.
	 * This handle is used to update the texture with each new camera frame.
	 */
	disp->location[0] = glGetUniformLocation(disp->program, "s_luma_texture");
	if (disp->location[0] == -1)
	{
		LOGS_ERR("Unable to get location program %s", string_gl_error(glGetError()));
		goto cleanup;
	}

	/*
	 * Get a handle to the s_chroma_texture variable in the fragment shader.
	 * This handle is used to update the texture with each new camera frame.
	 */
	disp->location[1] = glGetUniformLocation(disp->program, "s_chroma_texture");
	if (disp->location[1] == -1)
	{
		LOGS_ERR("Unable to get uniform location %s", string_gl_error(glGetError()));
		return -1;
	}

	/*
	 * Create a new vertex array. A group of vertices and indices stored in GPU memory.
	 * The frame being drawn is static fro the program duration, upload all the information
	 * to the GPU at the beginning then reference the location each frame without copying the data each frame.
	 */
	glGenVertexArrays(1, &disp->vertex_array);
	/* Select the vertex array that was just created and bind the new vertex buffers to the array */
	glBindVertexArray(disp->vertex_array);


	/*
	 * Generate a vertex buffers in GPU memory
	 * The first buffer is for the vertex data and the second buffer is for the index data.
	 */
	glGenBuffers(2, disp->vertex_buffers);

	/* Bind the first vertex buffer with the vertices to the vertex array */
	glBindBuffer(GL_ARRAY_BUFFER, disp->vertex_buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	/* Bind the second vertex buffer with the indices to the vertex array */
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, disp->vertex_buffers[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	/* Enable a_position (location 0) in the shader */
	glEnableVertexAttribArray(0);
	/* Enable a_tex_coord (location 1) in the shader */
	glEnableVertexAttribArray(1);

	/* Copy the vertices to the GPU to be used in a_position */
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), 0);
	error = glGetError();
	if (error != GL_NO_ERROR)
	{
		LOGS_ERR("Use glVertexAttribPointer 0 %s", string_gl_error(error));
	}
	/* Copy the texture co-ordinates to the GPU to be used in a_tex_coord */
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const void*)(3 * sizeof(GLfloat)));
	error = glGetError();
	if (error != GL_NO_ERROR)
	{
		LOGS_ERR("Use glVertexAttribPointer 1 %s", string_gl_error(error));
	}
	/* Select the default vertex array, allowing the application's array to be unbound */
	glBindVertexArray(0);

	/* Generate two textures, the first for luma data, the second for chroma data */
	glGenTextures(2, disp->texture);
	glBindTexture(GL_TEXTURE_2D, disp->texture[0]);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	/*
	 * Generate the space in the GPU for the luma texture, don't initialize the data.
	 * The data in the memory will be updated in the render function.
	 * Each texture has four components, x,y,z,w alias r,g,b,a alias s,r,t,u.
	 * Luma will be replicated in x, y, and z using type GL_LUMINCANCE. Component w is set to 1.0.
	 * The resolution of the texture matches the number of active pixels.
	 */
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
		disp->width, disp->height, 0,
		 GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
	error = glGetError();
	if (error != GL_NO_ERROR) {
		LOGS_ERR("Unable to generate texture %s", string_gl_error(error));
		goto cleanup;
	}
	/*
	 * Select the nearest texture value when the location doesn't match the exact texture position
	 * This shouldn't occur if the texture matches the surface size.
	 */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	/*
	 * Generate the space in the GPU for the chroma texture, don't initialize the data.
	 * The data in the memory will be updated in the render function.
	 * Each texture has four components, x,y,z,w alias r,g,b,a alias s,r,t,u.
	 * Cb is will be replicated in x, y, and z. Cr will be assigned to w. with type GL_LUMINANCE_ALPHA.
	 * There is one pair of chroma values for every four luma pixels.
	 * Half the vertical resoulution and half the horizontal resolution.
	 * Each GL_LUMINCANCE_ALPHA lookup will contain one pixel of chroma.
	 * The texture lookup will replicate the chroma values up to the total resolution.
	 */
	glBindTexture(GL_TEXTURE_2D, disp->texture[1]);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA,
		disp->width/2, disp->height/2, 0,
		 GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, NULL);
	error = glGetError();
	if (error != GL_NO_ERROR) {
		LOGS_ERR("Unable to generate texture %s", string_gl_error(error));
		return -1;
	}
	/*
	 * Select the nearest texture value when the location doesn't match the exact texture position.
	 * This will occur since the texture is a quarter the size of the surface size.
	 */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	/*
	 * Select an orange color that is distict from the native window.
	 * If the render fails an orange screen will appear.
	 * If render never reached then a white screen will appear.
	 */
	glClearColor ( 1.0f, 0.6f, 0.0f, 0.0f );

	/* Finally save the pointer to the render function that will be used to update the surface */
	disp->render_func = render_nv12m_subs_tex;

	return 0;
cleanup:
	x11_close_display(disp);
	return -1;
}
