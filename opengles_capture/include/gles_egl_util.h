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
 * EGL and OpenGLES utilites for errors and shader compiling.
 * @file gles_egl_util.h
 */
#ifndef GLES_EGL_UTIL_H__
#define GLES_EGL_UTIL_H__

#include <GLES3/gl3.h>
#include <GLES3/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

const char* string_egl_error(EGLint error);
const char* string_gl_error(GLenum error);
GLuint gles_load_shader(GLenum shader_type, const char *code);
GLuint gles_load_program(const char *vertex_code, const char *fragment_code);

void *gles_load_extension(const char* extension, const char* procedure_name);
void *egl_load_extension(EGLDisplay display, const char* extension, const char* procedure_name);

#endif