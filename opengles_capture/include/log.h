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
 * Log capture macros.
 * @file log.h
 */
#ifndef LOG_H__
#define LOG_H__

#include <stdio.h>
#include <time.h>
#include <sys/time.h>


#ifdef __cplusplus
extern "C" {
#endif


#define STRINGIFY(s) #s

#define	LOG_ERR		0
#define	LOG_WARNING	1
#define	LOG_INFO	2
#define	LOG_DEBUG	3

#define LOG_ALL LOG_DEBUG

#define LOGS_DBG(fmt, ...) LOGS(LOG_DEBUG, "DEBUG: ", fmt, ##__VA_ARGS__)
#define LOGS_INF(fmt, ...) LOGS(LOG_INFO, "INFO:  ", fmt, ##__VA_ARGS__)
#define LOGS_WRN(fmt, ...) LOGS(LOG_WARNING, "WARN:  ", fmt, ##__VA_ARGS__)
#define LOGS_ERR(fmt, ...) LOGS(LOG_ERR, "ERROR: ", fmt, ##__VA_ARGS__)

extern int VERBOSE;

#define LOGS(lvl, slvl, fmt, ...) { \
	if (lvl <= VERBOSE) { \
		struct timeval _tv; gettimeofday(&_tv, NULL); \
		printf("[%-10lu.%06lu] " slvl, _tv.tv_sec, _tv.tv_usec); \
		printf(fmt, ##__VA_ARGS__); \
		printf("  [" __FILE__ " %s:%d]\n", __func__, __LINE__); \
	}}

#ifdef __cplusplus
}
#endif

#endif