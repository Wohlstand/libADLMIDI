/*
 * Task manager with timer for DOS
 *
 * Copyright (c) 2025-2026 Vitaly Novichkov (Wohlstand)
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once
#ifndef DOS_TMAN_C_H
#define DOS_TMAN_C_H

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

void dos_taskman_suspend(void);
void dos_taskman_resume(void);

void dos_taskman_reserve_flush(FILE *stream);
int dos_taskman_reserve_fprintf(FILE *stream, const char *format, va_list agrgs);

int dos_taskman_is_inside_interrupt(void);

#ifdef __cplusplus
}
#endif

#endif
