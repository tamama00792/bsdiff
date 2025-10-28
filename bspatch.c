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

#include <limits.h>
#include "bspatch.h"

/**
 * 功能：将大端序的8字节数据转换为有符号64位整数（补丁文件使用此格式）
 * 参数：
 *   - buf: 指向8字节缓冲区的指针，使用大端序（网络字节序）存储
 * 返回：转换后的有符号64位整数
 * 
 * 原理：大端序是最高字节在前，小端序是最低字节在前
 *       这里将8个大端序字节组合成一个64位整数，并处理符号位
 */
static int64_t offtin(uint8_t *buf)
{
	int64_t y;  // 用于存储最终结果的64位有符号整数
    
	// 从高位到低位逐字节组合
	y=buf[7]&0x7F;           // 取第8个字节（最高字节）的低7位，清空符号位
    y=y*256;y+=buf[6];       // 左移1字节（乘以256），加上第7个字节
	y=y*256;y+=buf[5];       // 继续左移并加上第6个字节
	y=y*256;y+=buf[4];       // 继续左移并加上第5个字节
	y=y*256;y+=buf[3];       // 继续左移并加上第4个字节
	y=y*256;y+=buf[2];       // 继续左移并加上第3个字节
	y=y*256;y+=buf[1];       // 继续左移并加上第2个字节
	y=y*256;y+=buf[0];       // 继续左移并加上第1个字节（最低字节）

	// 判断符号位（第8个字节的最高位）
	if(buf[7]&0x80) y=-y;    // 如果符号位为1，表示负数，需要取反

	return y;
}

/**
 * 功能：将旧文件根据差分数据生成新文件（BSDiff算法的核心补丁函数）
 * 参数：
 *   - old: 指向旧文件内容的指针
 *   - oldsize: 旧文件的大小（字节数）
 *   - new: 指向新文件内容缓冲区的指针（输出）
 *   - newsize: 新文件的大小（字节数）
 *   - stream: 指向补丁数据流的指针（提供读取补丁数据的方法）
 * 返回：
 *   - 0: 成功
 *   - -1: 失败（读取错误或数据损坏）
 * 
 * 算法原理：
 * BSDiff使用三个控制数据来重建新文件：
 *   ctrl[0] - diff长度：需要复制到新文件的数据长度
 *   ctrl[1] - extra长度：需要额外添加的数据长度
 *   ctrl[2] - 旧文件偏移：旧文件中需要跳过的字节数
 */
int bspatch(const uint8_t* old, int64_t oldsize, uint8_t* new, int64_t newsize, struct bspatch_stream* stream)
{
	uint8_t buf[8];          // 临时缓冲区，用于读取8字节的控制数据
	int64_t oldpos,newpos;   // 旧文件和新文件的当前位置指针
	int64_t ctrl[3];         // 控制数据数组：ctrl[0]=diff长度, ctrl[1]=extra长度, ctrl[2]=旧文件偏移
	int64_t i;               // 循环计数器

	// 初始化：从文件开头开始
	oldpos=0;  // 旧文件的当前位置，从0开始
	newpos=0;  // 新文件的当前位置，从0开始
	
	// 主循环：直到新文件的所有字节都被写入
	while(newpos<newsize) {
		
		/* 读取控制数据 */
		// 每次操作需要3个控制值（diff长度、extra长度、旧文件偏移）
		for(i=0;i<=2;i++) {
			// 从补丁数据流中读取8字节的控制数据
			if (stream->read(stream, buf, 8))
				return -1;  // 读取失败，返回错误
			// 将8字节的大端序数据转换为64位整数
			ctrl[i]=offtin(buf);
		};

		/* 安全检查：验证控制数据的有效性 */
		// 检查：diff长度和extra长度必须非负且不超过INT_MAX
		// 检查：当前新文件位置加上diff长度不能超出新文件大小
		if (ctrl[0]<0 || ctrl[0]>INT_MAX ||
			ctrl[1]<0 || ctrl[1]>INT_MAX ||
			newpos+ctrl[0]>newsize)
			return -1;  // 控制数据无效，返回错误

		/* 读取diff字符串（差分数据）*/
		// 从补丁数据流中读取diff数据到新文件缓冲区
		if (stream->read(stream, new + newpos, ctrl[0]))
			return -1;  // 读取失败，返回错误

		/* 将旧文件数据添加到diff字符串中 */
		// 将diff数据与旧文件中对应位置的数据相加，得到新文件的内容
		for(i=0;i<ctrl[0];i++)
			// 确保旧文件位置有效（在范围内）
			if((oldpos+i>=0) && (oldpos+i<oldsize))
				// 将旧文件数据加到diff数据上
				new[newpos+i]+=old[oldpos+i];

		/* 调整新文件和旧文件的指针位置 */
		// 新文件位置向前移动diff长度
		newpos+=ctrl[0];
		// 旧文件位置向前移动diff长度
		oldpos+=ctrl[0];

		/* 再次安全检查：验证extra数据的有效性 */
		// 检查：当前新文件位置加上extra长度不能超出新文件大小
		if(newpos+ctrl[1]>newsize)
			return -1;  // 控制数据无效，返回错误

		/* 读取extra字符串（额外数据）*/
		// 从补丁数据流中读取extra数据到新文件缓冲区
		if (stream->read(stream, new + newpos, ctrl[1]))
			return -1;  // 读取失败，返回错误

		/* 调整新文件和旧文件的指针位置 */
		// 新文件位置向前移动extra长度（写入extra数据）
		newpos+=ctrl[1];
		// 旧文件位置向前移动ctrl[2]长度（跳过旧文件中的某些数据）
		oldpos+=ctrl[2];
	};

	return 0;  // 成功完成补丁应用
}

#if defined(BSPATCH_EXECUTABLE)

#include <bzlib.h>      // BZip2压缩库头文件
#include <stdlib.h>     // 标准库：内存管理、程序控制等
#include <stdint.h>     // 标准库：固定宽度整数类型
#include <stdio.h>      // 标准库：输入输出
#include <string.h>     // 标准库：字符串处理
#include <err.h>        // 标准库：错误报告
#include <sys/types.h>  // 系统库：数据类型定义
#include <sys/stat.h>   // 系统库：文件状态
#include <unistd.h>     // 系统库：POSIX操作系统API
#include <fcntl.h>      // 系统库：文件控制

/**
 * 功能：从BZip2压缩的补丁文件中读取数据
 * 参数：
 *   - stream: 指向补丁数据流结构的指针
 *   - buffer: 用于存储读取数据的缓冲区
 *   - length: 要读取的字节数
 * 返回：
 *   - 0: 成功读取指定长度的数据
 *   - -1: 读取失败（实际读取的字节数与请求的不一致）
 * 
 * 注意：这是一个回调函数，用于bspatch函数从BZip2压缩文件中读取数据
 */
static int bz2_read(const struct bspatch_stream* stream, void* buffer, int length)
{
	int n;              // 实际读取的字节数
	int bz2err;         // BZip2错误码
	BZFILE* bz2;        // BZip2文件句柄指针

	// 从stream的opaque字段获取BZip2文件句柄
	bz2 = (BZFILE*)stream->opaque;
	
	// 从BZip2文件中读取指定长度的数据
	n = BZ2_bzRead(&bz2err, bz2, buffer, length);
	
	// 检查是否成功读取了指定长度的数据
	if (n != length)
		return -1;  // 读取失败，返回错误

	return 0;  // 读取成功
}

/**
 * 功能：程序主入口，执行文件补丁操作
 * 参数：
 *   - argc: 命令行参数数量
 *   - argv: 命令行参数数组
 *          argv[0]: 程序名称
 *          argv[1]: 旧文件路径
 *          argv[2]: 新文件路径
 *          argv[3]: 补丁文件路径
 * 返回：
 *   - 0: 成功
 *   其他值: 失败（由err函数直接退出）
 * 
 * 程序流程：
 * 1. 检查命令行参数是否正确
 * 2. 读取补丁文件头（24字节）
 * 3. 验证补丁文件魔数（ENDSLEY/BSDIFF43）
 * 4. 读取新文件大小
 * 5. 读取旧文件内容到内存
 * 6. 分配新文件缓冲区
 * 7. 打开BZip2压缩的补丁数据
 * 8. 调用bspatch函数应用补丁
 * 9. 将新文件内容写入磁盘
 * 10. 清理资源并退出
 */
int main(int argc,char * argv[])
{
	FILE * f;                          // 补丁文件的文件指针
	int fd;                            // 文件描述符（用于旧文件和新文件）
	int bz2err;                        // BZip2错误码
	uint8_t header[24];                // 补丁文件头（24字节）
	uint8_t *old, *new;                // 旧文件和新文件的内存缓冲区
	int64_t oldsize, newsize;          // 旧文件和新文件的大小
	BZFILE* bz2;                       // BZip2文件句柄
	struct bspatch_stream stream;      // 补丁数据流结构
	struct stat sb;                    // 文件状态结构（用于保存文件权限）

	// 检查命令行参数数量（需要4个：程序名、旧文件、新文件、补丁文件）
	if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile\n",argv[0]);

	/* 打开补丁文件 */
	// 以只读模式打开补丁文件
	if ((f = fopen(argv[3], "r")) == NULL)
		err(1, "fopen(%s)", argv[3]);

	/* 读取补丁文件头 */
	// 补丁文件头共24字节：前16字节是魔数，后8字节是新文件大小
	if (fread(header, 1, 24, f) != 24) {
		// 检查是否到达文件末尾（说明文件太小）
		if (feof(f))
			errx(1, "Corrupt patch\n");
		// 读取失败
		err(1, "fread(%s)", argv[3]);
	}

	/* 验证补丁文件魔数 */
	// 检查前16字节是否为 "ENDSLEY/BSDIFF43"
	if (memcmp(header, "ENDSLEY/BSDIFF43", 16) != 0)
		errx(1, "Corrupt patch\n");

	/* 从文件头读取新文件大小 */
	// header+16 指向文件头中的新文件大小字段（后8字节）
	newsize=offtin(header+16);
	// 验证新文件大小是否有效（必须为非负数）
	if(newsize<0)
		errx(1,"Corrupt patch\n");

	/* 关闭补丁文件，重新打开旧文件并读取到内存 */
	// 这一系列操作：打开旧文件 -> 获取大小 -> 分配内存 -> 定位到开头 -> 读取内容 -> 获取状态 -> 关闭文件
	if(((fd=open(argv[1],O_RDONLY,0))<0) ||                    // 以只读模式打开旧文件
		((oldsize=lseek(fd,0,SEEK_END))==-1) ||                // 获取文件大小（定位到文件末尾）
		((old=malloc(oldsize+1))==NULL) ||                     // 分配内存（多加1字节以防溢出）
		(lseek(fd,0,SEEK_SET)!=0) ||                           // 定位到文件开头
		(read(fd,old,oldsize)!=oldsize) ||                     // 读取整个旧文件到内存
		(fstat(fd, &sb)) ||                                     // 获取文件状态（包括权限信息）
		(close(fd)==-1)) err(1,"%s",argv[1]);                 // 关闭旧文件
		
	// 为新文件分配内存
	if((new=malloc(newsize+1))==NULL) err(1,NULL);

	/* 重新打开BZip2压缩的补丁数据 */
	// 注意：f指针已经在开头，现在从第24字节位置读取压缩数据
	if (NULL == (bz2 = BZ2_bzReadOpen(&bz2err, f, 0, 0, NULL, 0)))
		errx(1, "BZ2_bzReadOpen, bz2err=%d", bz2err);

	/* 设置补丁数据流结构 */
	// 设置读取函数为bz2_read
	stream.read = bz2_read;
	// 设置opaque指针为BZip2文件句柄
	stream.opaque = bz2;
	
	/* 应用补丁，生成新文件 */
	if (bspatch(old, oldsize, new, newsize, &stream))
		errx(1, "bspatch");

	/* 清理BZip2资源 */
	// 关闭BZip2读取器
	BZ2_bzReadClose(&bz2err, bz2);
	// 关闭补丁文件
	fclose(f);

	/* 将新文件写入磁盘 */
	// 打开新文件（创建、清空、只写），使用旧文件的权限
	if(((fd=open(argv[2],O_CREAT|O_TRUNC|O_WRONLY,sb.st_mode))<0) ||  // 创建新文件
		(write(fd,new,newsize)!=newsize) ||                            // 写入新文件内容
		(close(fd)==-1))                                               // 关闭新文件
		err(1,"%s",argv[2]);

	/* 释放内存资源 */
	// 释放新文件缓冲区
	free(new);
	// 释放旧文件缓冲区
	free(old);

	return 0;  // 成功完成所有操作
}

#endif
