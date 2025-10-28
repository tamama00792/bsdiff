/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2012 Matthew Endsley
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BSDIFF_H
# define BSDIFF_H

# include <stddef.h>
# include <stdint.h>

/**
 * 功能：数据流结构体，定义内存分配和写入操作
 * 用于bsdiff函数将补丁数据写入到自定义的目标（如文件、内存、网络等）
 */
struct bsdiff_stream
{
	void* opaque;  // 不透明指针，存储用户自定义数据（如文件句柄）

	void* (*malloc)(size_t size);  // 内存分配函数指针
	void (*free)(void* ptr);        // 内存释放函数指针
	int (*write)(struct bsdiff_stream* stream, const void* buffer, int size);  // 数据写入函数指针
};

/**
 * 功能：计算两个文件的差分并生成补丁文件
 * 参数：
 *   - old: 旧文件数据指针
 *   - oldsize: 旧文件大小（字节数）
 *   - new: 新文件数据指针
 *   - newsize: 新文件大小（字节数）
 *   - stream: 输出流指针（用于写入补丁数据）
 * 返回：
 *   - 0: 成功
 *   - -1: 失败
 */
int bsdiff(const uint8_t* old, int64_t oldsize, const uint8_t* new, int64_t newsize, struct bsdiff_stream* stream);

#endif
