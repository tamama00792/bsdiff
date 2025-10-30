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

#include "bsdiff.h"

#include <limits.h>
#include <string.h>

// 定义宏：返回两个数中较小的那个
#define MIN(x,y) (((x)<(y)) ? (x) : (y))

/**
 * 功能：快速后缀排序的分区函数，将后缀数组按指定前缀长度h进行分区
 * 参数：
 *   - I: 后缀数组（存储后缀的索引）
 *   - V: 辅助数组，用于存储每个后缀的排序值
 *   - start: 分区的起始位置
 *   - len: 分区的长度
 *   - h: 当前比较的前缀长度（用于对后缀进行排序）
 */
static void split(int64_t *I,int64_t *V,int64_t start,int64_t len,int64_t h)
{
	int64_t i,j,k,x,tmp,jj,kk;  // i: 循环计数器; j: 临时计数器; k: 临时计数器; x: 基准值; tmp: 临时交换变量; jj: 小于基准值的分界线; kk: 等于基准值的分界线

	// 如果分区长度小于16，使用插入排序（简单但高效）
	if(len<16) {
		// 遍历从start开始的所有元素
		for(k=start;k<start+len;k+=j) {
			j=1;                                // 初始化计数器j为1
			x=V[I[k]+h];                        // 选择当前元素的基准值
			// 遍历后续元素，找到最小值并统计相等元素的数量
			for(i=1;k+i<start+len;i++) {
				if(V[I[k+i]+h]<x) {             // 如果找到更小的值
					x=V[I[k+i]+h];               // 更新基准值
					j=0;                         // 重置计数器j
				};
				if(V[I[k+i]+h]==x) {             // 如果值相等
					tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;  // 交换到前面
					j++;                          // 计数器加1
				};
			};
			// 将排序结果写回V数组
			for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
			// 如果只有一个元素，标记为-1
			if(j==1) I[k]=-1;
		};
		return;
	};

	// 否则使用快速排序（递归分区）
	// 选择中位数作为基准值
	x=V[I[start+len/2]+h];
	jj=0;  // 小于基准值的元素计数
	kk=0;  // 等于基准值的元素计数
	// 统计小于和等于基准值的元素数量
	for(i=start;i<start+len;i++) {
		if(V[I[i]+h]<x) jj++;
		if(V[I[i]+h]==x) kk++;
	};
	jj+=start;  // 计算分界点（小于基准值的最后一个位置）
	kk+=jj;     // 计算分界点（等于基准值的最后一个位置）

	// 对元素进行分类：将小于、等于、大于基准值的元素分别放到三个区域
	i=start;j=0;k=0;
	while(i<jj) {
		if(V[I[i]+h]<x) {
			i++;  // 小于基准值，留在左区域
		} else if(V[I[i]+h]==x) {
			tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;  // 等于基准值，放到中间区域
			j++;
		} else {
			tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;  // 大于基准值，放到右区域
			k++;
		};
	};

	// 处理中间区域的剩余元素
	while(jj+j<kk) {
		if(V[I[jj+j]+h]==x) {
			j++;  // 等于基准值，留在中间区域
		} else {
			tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;  // 大于基准值，移到右区域
			k++;
		};
	};

	// 递归处理左区域（小于基准值的元素）
	if(jj>start) split(I,V,start,jj-start,h);

	// 更新中间区域的V值
	for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
	// 如果中间区域只有一个元素，标记为-1
	if(jj==kk-1) I[jj]=-1;

	// 递归处理右区域（大于基准值的元素）
	if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

/**
 * 功能：快速后缀排序算法，构建后缀数组I和辅助数组V
 * 参数：
 *   - I: 后缀数组（输出），存储排序后的后缀索引
 *   - V: 辅助数组（临时工作空间）
 *   - old: 原始数据缓冲区（旧文件的内容）
 *   - oldsize: 原始数据的大小（字节数）
 * 
 * 算法原理：
 * 使用快速排序算法构建后缀数组，用于加速后续的匹配过程
 */
static void qsufsort(int64_t *I,int64_t *V,const uint8_t *old,int64_t oldsize)
{
	int64_t buckets[256];  // 桶数组，用于统计每个字节值（0-255）的出现次数
	int64_t i,h,len;        // i: 循环计数器; h: 当前比较的前缀长度; len: 当前处理段的长度

	// 第一步：使用桶排序对第一个字节进行排序
	// 初始化桶数组
	for(i=0;i<256;i++) buckets[i]=0;
	// 统计每个字节值的出现次数
	for(i=0;i<oldsize;i++) buckets[old[i]]++;
	// 计算累积计数（每个字节值的结束位置）
	for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
	// 调整桶数组，使其表示每个字节值的起始位置
	for(i=255;i>0;i--) buckets[i]=buckets[i-1];
	buckets[0]=0;

	// 第二步：填充后缀数组I（基于第一个字节排序），I[i]存储第i个后缀在原数据中的索引
	for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
	// 在I[0]处存储oldsize作为标记
	I[0]=oldsize;
	// 填充辅助数组V（存储每个后缀所在区间的末尾（不含）的索引）
	for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
	V[oldsize]=0;
	// 标记不需要排序的单元素分组
	for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
	I[0]=-1;

	// 第三步：使用split函数递归地对较长前缀进行排序
	// 从h=1开始，每次翻倍，直到所有后缀都被正确排序
	for(h=1;I[0]!=-(oldsize+1);h+=h) {
		len=0;  // 初始化当前段的长度
		// 遍历所有后缀
		for(i=0;i<oldsize+1;) {
			if(I[i]<0) {
				// 如果遇到负值（单元素分组或已处理段），跳过
				len-=I[i];
				i-=I[i];
			} else {
				// 处理一个需要排序的段
				if(len) I[i-len]=-len;  // 标记前一段的长度
				len=V[I[i]]+1-i;         // 计算当前段的长度，I[i]是当前后缀在原数据的索引，V[I[i]]是当前后缀所在区间的末尾（不含）的索引，+1是因为区间是左闭右开的
				split(I,V,i,len,h);      // 对当前段进行排序
				i+=len;                  // 移动到下一个段
				len=0;                   // 重置长度
			};
		};
		// 如果最后还有未标记的段，标记它
		if(len) I[i-len]=-len;
	};

	// 第四步：反转数组，得到最终的后缀数组
	for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

/**
 * 功能：计算两个字节序列从头开始的匹配长度
 * 参数：
 *   - old: 旧数据缓冲区
 *   - oldsize: 旧数据的大小（字节数）
 *   - new: 新数据缓冲区
 *   - newsize: 新数据的大小（字节数）
 * 返回：从头开始的连续匹配字节数
 */
static int64_t matchlen(const uint8_t *old,int64_t oldsize,const uint8_t *new,int64_t newsize)
{
	int64_t i;  // 循环计数器

	// 从头开始逐个字节比较，直到不匹配或达到任一序列的末尾
	for(i=0;(i<oldsize)&&(i<newsize);i++)
		if(old[i]!=new[i]) break;  // 发现不匹配，退出循环

	return i;  // 返回匹配的字节数
}

/**
 * 功能：在后缀数组中二分搜索与new最匹配的后缀
 * 参数：
 *   - I: 后缀数组（已排序的后缀索引）
 *   - old: 旧数据缓冲区
 *   - oldsize: 旧数据的大小
 *   - new: 新数据缓冲区（要匹配的字符串）
 *   - newsize: 新数据的大小
 *   - st: 搜索范围的起始位置
 *   - en: 搜索范围的结束位置
 *   - pos: 输出参数，存储找到的最佳匹配位置
 * 返回：匹配的字节数
 */
static int64_t search(const int64_t *I,const uint8_t *old,int64_t oldsize,
		const uint8_t *new,int64_t newsize,int64_t st,int64_t en,int64_t *pos)
{
	int64_t x,y;  // x: 中间位置匹配长度; y: 结束位置匹配长度

	// 如果搜索范围只有1个或2个元素，直接比较
	if(en-st<2) {
		// 计算起始位置的匹配长度
		x=matchlen(old+I[st],oldsize-I[st],new,newsize);
		// 计算结束位置的匹配长度
		y=matchlen(old+I[en],oldsize-I[en],new,newsize);

		// 返回匹配长度更长的那个位置
		if(x>y) {
			*pos=I[st];
			return x;
		} else {
			*pos=I[en];
			return y;
		}
	};

	// 否则使用二分搜索
	x=st+(en-st)/2;  // 计算中间位置
	// 比较中间位置的后缀与new的匹配情况
	if(memcmp(old+I[x],new,MIN(oldsize-I[x],newsize))<0) {
		// 中间位置的后缀小于new，继续在右半部分搜索
		return search(I,old,oldsize,new,newsize,x,en,pos);
	} else {
		// 中间位置的后缀大于或等于new，继续在左半部分搜索
		return search(I,old,oldsize,new,newsize,st,x,pos);
	};
}

/**
 * 功能：将有符号64位整数转换为8字节的大端序（big-endian）字节数组
 * 参数：
 *   - x: 要转换的64位整数
 *   - buf: 输出缓冲区（8字节）
 * 
 * 注意：使用大端序存储，便于网络传输和跨平台兼容性
 */
static void offtout(int64_t x,uint8_t *buf)
{
	int64_t y;  // 用于存储x的绝对值

	// 如果是负数，先取绝对值（后续会处理符号位）
	if(x<0) y=-x; else y=x;

	// 将y从低位到高位逐字节分解（大端序：高字节在前）
	buf[0]=y%256;y-=buf[0];      // 最低字节
	y=y/256;buf[1]=y%256;y-=buf[1];
	y=y/256;buf[2]=y%256;y-=buf[2];
	y=y/256;buf[3]=y%256;y-=buf[3];
	y=y/256;buf[4]=y%256;y-=buf[4];
	y=y/256;buf[5]=y%256;y-=buf[5];
	y=y/256;buf[6]=y%256;y-=buf[6];
	y=y/256;buf[7]=y%256;         // 最高字节

	// 如果原数是负数，设置最高字节的最高位作为符号位
	if(x<0) buf[7]|=0x80;
}

/**
 * 功能：向数据流中写入数据（支持长数据的分块写入）
 * 参数：
 *   - stream: 指向数据流结构的指针
 *   - buffer: 要写入的数据缓冲区
 *   - length: 要写入的数据长度（字节数）
 * 返回：
 *   - 成功写入的字节数
 *   - -1: 写入失败
 * 
 * 注意：由于write函数可能只支持INT_MAX大小的写入，这里将大块数据分小块写入
 */
static int64_t writedata(struct bsdiff_stream* stream, const void* buffer, int64_t length)
{
	int64_t result = 0;  // 累计写入的字节数

	// 循环写入，直到所有数据都写入
	while (length > 0)
	{
		// 计算本次写入的大小（限制为INT_MAX）
		const int smallsize = (int)MIN(length, INT_MAX);
		// 调用写入函数
		const int writeresult = stream->write(stream, buffer, smallsize);
		// 检查写入是否成功
		if (writeresult == -1)
		{
			return -1;  // 写入失败
		}

		result += writeresult;           // 累计写入的字节数
		length -= smallsize;             // 减去已写入的长度
		buffer = (uint8_t*)buffer + smallsize;  // 更新缓冲区指针
	}

	return result;  // 返回总写入字节数
}

/**
 * 功能：bsdiff内部使用的请求结构体
 * 用于传递差分计算所需的所有参数
 */
struct bsdiff_request
{
	const uint8_t* old;            // 旧文件数据指针
	int64_t oldsize;                // 旧文件大小
	const uint8_t* new;            // 新文件数据指针
	int64_t newsize;                // 新文件大小
	struct bsdiff_stream* stream;  // 输出流指针
	int64_t *I;                     // 后缀数组指针
	uint8_t *buffer;                // 临时缓冲区
};

/**
 * 功能：BSDiff算法的核心实现函数，计算两个文件的差分
 * 参数：
 *   - req: 包含旧文件、新文件和输出流的请求结构体
 * 返回：
 *   - 0: 成功
 *   - -1: 失败（内存分配失败或写入失败）
 * 
 * 算法原理：
 * 1. 首先对旧文件构建后缀数组
 * 2. 遍历新文件，使用二分搜索在后缀数组中查找最佳匹配
 * 3. 对于每个匹配区域，记录：匹配长度、差异数据、额外数据
 * 4. 将这三部分数据写入补丁文件
 */
static int bsdiff_internal(const struct bsdiff_request req)
{
	int64_t *I,*V;                    // I: 后缀数组; V: 临时辅助数组
	int64_t scan,pos,len;              // scan: 新文件扫描位置; pos: 旧文件匹配位置; len: 当前匹配长度
	int64_t lastscan,lastpos,lastoffset;  // 上次处理的扫描位置、匹配位置、偏移
	int64_t oldscore,scsc;            // oldscore: 旧文件匹配分数; scsc: 扫描计数器
	int64_t s,Sf,lenf,Sb,lenb;        // s: 临时计数器; Sf: 前向最大得分; lenf: 前向最大长度; Sb: 后向最大得分; lenb: 后向最大长度
	int64_t overlap,Ss,lens;          // overlap: 重叠长度; Ss: 重叠得分; lens: 重叠长度
	int64_t i;                         // 循环计数器
	uint8_t *buffer;                   // 临时缓冲区指针
	uint8_t buf[8 * 3];                // 控制数据缓冲区（3个64位整数，共24字节）

	// 为辅助数组V分配内存
	if((V=req.stream->malloc((req.oldsize+1)*sizeof(int64_t)))==NULL) return -1;
	I = req.I;  // 使用传入的后缀数组

	// 第一步：对旧文件构建后缀数组（这是算法的核心步骤）
	qsufsort(I,V,req.old,req.oldsize);
	// 释放辅助数组V（不再需要）
	req.stream->free(V);

	buffer = req.buffer;  // 使用传入的缓冲区

	/* 第二步：计算差分，同时写入控制数据 */
	// 初始化扫描位置、匹配长度、匹配位置
	scan=0;len=0;pos=0;
	// lastscan是当前“候选”匹配区域在new中的开始位置；
	// lastpos 是当前“候选”匹配区域在old中的开始位置；
	// lastoffset是当前“候选”匹配区域中old中位置相对于new中对应位置的差值，
	//   因此<new_pos>+lastoffset=<old_pos>；
	lastscan=0;lastpos=0;lastoffset=0;
	// 主循环：遍历整个新文件
	while(scan<req.newsize) {
		// 当前“候选”匹配区域为：new[lastscan, scan) <-> old[lastpos, scan+lastoffset)
		oldscore=0;  // 初始化旧文件匹配分数

		// 寻找下一个匹配点（使用贪心算法扩展匹配范围）
		for(scsc=scan+=len;scan<req.newsize;scan++) {
			// 在后缀数组中搜索与当前位置最佳匹配的位置，pos代表位置，len代表长度
			len=search(I,req.old,req.oldsize,req.new+scan,req.newsize-scan,
					0,req.oldsize,&pos);

			// 上一步的搜索，得到了“候选”匹配区域new的向后延伸，在old中完全匹配区域的开始位置和长度，
			//   但这个开始位置和“候选”匹配区域中old的结束位置有可能并不相连。

			// 统计new中[scan, scan+len)与old中“候选”匹配区域的对应延伸区段中相等的字节数，
			// 累加到oldscore中。注意循环变量是scsc，因此不会重复累加。
			for(;scsc<scan+len;scsc++)
			if((scsc+lastoffset<req.oldsize) &&
				(req.old[scsc+lastoffset] == req.new[scsc]))
				oldscore++;

			// (len==oldscore) && (len!=0) 说明当前“候选”区域的延伸区域仍然匹配的很好，
			//   那么可以扩展“候选”匹配区域，并继续向后搜索；
			// (len>oldscore+8) 说明延伸区域中至少有8个以上的字节是不匹配的，
			//   那么当前“候选”区域应该到此截止而不扩展；
			if(((len==oldscore) && (len!=0)) || 
				(len>oldscore+8)) break;

			// 走到这里说明当前不相等的字节数没有大于8，需要继续循环。
			// 由于下次循环是从scan+1的位置上尝试，因此若scan对应的字节是相等的，
			// 它已经被计算在oldscore之内的值需要被减掉。
			if((scan+lastoffset<req.oldsize) &&
				(req.old[scan+lastoffset] == req.new[scan]))
				oldscore--;
		};

		// len!=oldscore说明当前“候选”匹配区域不需要再继续向后搜索了；
		// scan==newsize说明已经已到达文件尾；
		// 符合这两个条件之一时，我们开始处理当前这个近似匹配区域；
		if((len!=oldscore) || (scan==req.newsize)) {
			// 前向扩展：在前一个匹配点之后寻找更长的连续匹配
			// 确定lenf的长度。区域[lastscan, lastscan+lenf)被称为forward extension
			s=0;Sf=0;lenf=0;
			for(i=0;(lastscan+i<scan)&&(lastpos+i<req.oldsize);) {
				if(req.old[lastpos+i]==req.new[lastscan+i]) s++;
				i++;
				// 记录最佳前向匹配位置（得分函数：匹配数*2-总长度）
				if(s*2-i>Sf*2-lenf) { Sf=s; lenf=i; };
			};

			// 后向扩展：在当前匹配点之前寻找更长的连续匹配
			// 确定lenb的长度。区域[scan-lenb, scan)被称为backward extension
			lenb=0;
			if(scan<req.newsize) {
				s=0;Sb=0;
				for(i=1;(scan>=lastscan+i)&&(pos>=i);i++) {
					if(req.old[pos-i]==req.new[scan-i]) s++;
					// 记录最佳后向匹配位置
					if(s*2-i>Sb*2-lenb) { Sb=s; lenb=i; };
				};
			};

			// 处理重叠情况：如果前向扩展和后向扩展有重叠，选择最佳重叠点
			// forword extension和backward extension区域出现重叠时需要调整lenf和lenb
			if(lastscan+lenf>scan-lenb) {
				overlap=(lastscan+lenf)-(scan-lenb);  // 计算重叠长度
				s=0;Ss=0;lens=0;
				// 遍历重叠区域，找到最佳的重叠点
				for(i=0;i<overlap;i++) {
					// 前向扩展的匹配得分
					if(req.new[lastscan+lenf-overlap+i]==
					   req.old[lastpos+lenf-overlap+i]) s++;
					// 后向扩展的匹配得分（减去）
					if(req.new[scan-lenb+i]==
					   req.old[pos-lenb+i]) s--;
					// 记录最高得分
					if(s>Ss) { Ss=s; lens=i+1; };
				};

				// 调整前向和后向的长度
				lenf+=lens-overlap;
				lenb-=lens;
			};

			// 将控制数据编码为大端序格式
			offtout(lenf,buf);                                    // ctrl[0]: diff长度
			offtout((scan-lenb)-(lastscan+lenf),buf+8);           // ctrl[1]: extra长度
			offtout((pos-lenb)-(lastpos+lenf),buf+16);           // ctrl[2]: 旧文件偏移

			/* 写入控制数据 */
			if (writedata(req.stream, buf, sizeof(buf)))
				return -1;

			/* 写入diff数据（差值：新文件-旧文件）*/
			// forward extension即为diff区段，减去old中对应位置的值，结果写到buffer中
			for(i=0;i<lenf;i++)
				buffer[i]=req.new[lastscan+i]-req.old[lastpos+i];
			// 写入三元组的x
			if (writedata(req.stream, buffer, lenf))
				return -1;

			/* 写入额外数据（新文件中无法用旧文件表示的部分）*/
			// forward extension和backward extension若不相连，两者中间的区域
			// 即为extra区段，其内容直接复制到buffer中
			for(i=0;i<(scan-lenb)-(lastscan+lenf);i++)
				buffer[i]=req.new[lastscan+lenf+i];
			// 写入三元组的y
			if (writedata(req.stream, buffer, (scan-lenb)-(lastscan+lenf)))
				return -1;

			// 更新位置追踪变量
			// backward extension会被作为下一轮“候选”匹配区域的开始部分，
			// 也就是变成下一次的forward extension的一部分
			lastscan=scan-lenb;  // 更新上次扫描位置
			lastpos=pos-lenb;    // 更新上次匹配位置
			lastoffset=pos-scan;  // 更新偏移量
		};
	};

	return 0;  // 成功完成差分计算
}

/**
 * 功能：BSDiff公开API，计算两个文件的差分并生成补丁文件
 * 参数：
 *   - old: 旧文件数据指针
 *   - oldsize: 旧文件大小（字节数）
 *   - new: 新文件数据指针
 *   - newsize: 新文件大小（字节数）
 *   - stream: 输出流指针（用于写入补丁数据）
 * 返回：
 *   - 0: 成功
 *   - -1: 失败（内存分配失败）
 */
int bsdiff(const uint8_t* old, int64_t oldsize, const uint8_t* new, int64_t newsize, struct bsdiff_stream* stream)
{
	int result;                  // 返回值
	struct bsdiff_request req;   // 内部请求结构体

	// 为后缀数组I分配内存
	if((req.I=stream->malloc((oldsize+1)*sizeof(int64_t)))==NULL)
		return -1;

	// 为临时缓冲区分配内存
	if((req.buffer=stream->malloc(newsize+1))==NULL)
	{
		stream->free(req.I);  // 内存分配失败，释放之前分配的内存
		return -1;
	}

	// 填充请求结构体
	req.old = old;
	req.oldsize = oldsize;
	req.new = new;
	req.newsize = newsize;
	req.stream = stream;

	// 调用内部函数执行实际的差分计算
	result = bsdiff_internal(req);

	// 释放分配的内存
	stream->free(req.buffer);
	stream->free(req.I);

	return result;
}

#if defined(BSDIFF_EXECUTABLE)

#include <sys/types.h>

#include <bzlib.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * 功能：向BZip2压缩流中写入数据
 * 参数：
 *   - stream: 指向数据流结构的指针
 *   - buffer: 要写入的数据缓冲区
 *   - size: 要写入的数据大小（字节数）
 * 返回：
 *   - 0: 成功
 *   - -1: 写入失败
 */
static int bz2_write(struct bsdiff_stream* stream, const void* buffer, int size)
{
	int bz2err;      // BZip2错误码
	BZFILE* bz2;     // BZip2文件句柄指针

	// 从stream的opaque字段获取BZip2文件句柄
	bz2 = (BZFILE*)stream->opaque;
	// 向BZip2文件中写入数据
	BZ2_bzWrite(&bz2err, bz2, (void*)buffer, size);
	// 检查错误状态
	if (bz2err != BZ_STREAM_END && bz2err != BZ_OK)
		return -1;

	return 0;  // 成功
}

/**
 * 功能：程序主入口，生成补丁文件
 * 参数：
 *   - argc: 命令行参数数量
 *   - argv: 命令行参数数组（argv[0]=程序名, argv[1]=旧文件, argv[2]=新文件, argv[3]=补丁文件）
 * 返回：
 *   - 0: 成功
 *   其他值: 失败（由err函数直接退出）
 */
int main(int argc,char *argv[])
{
	int fd;                        // 文件描述符
	int bz2err;                    // BZip2错误码
	uint8_t *old,*new;             // 旧文件和新文件的内存缓冲区
	off_t oldsize,newsize;         // 旧文件和新文件的大小
	uint8_t buf[8];                // 临时缓冲区（用于存储新文件大小）
	FILE * pf;                     // 补丁文件指针
	struct bsdiff_stream stream;   // 数据流结构
	BZFILE* bz2;                   // BZip2文件句柄

	// 初始化BZip2句柄
	memset(&bz2, 0, sizeof(bz2));
	// 设置数据流的内存分配函数
	stream.malloc = malloc;
	stream.free = free;
	stream.write = bz2_write;

	// 检查命令行参数数量
	if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile\n",argv[0]);

	/* 读取旧文件到内存 */
	// 分配oldsize+1字节而不是oldsize字节，确保即使oldsize=0也能正确工作
	if(((fd=open(argv[1],O_RDONLY,0))<0) ||                    // 以只读模式打开旧文件
		((oldsize=lseek(fd,0,SEEK_END))==-1) ||                // 获取文件大小
		((old=malloc(oldsize+1))==NULL) ||                     // 分配内存
		(lseek(fd,0,SEEK_SET)!=0) ||                           // 定位到文件开头
		(read(fd,old,oldsize)!=oldsize) ||                     // 读取整个文件
		(close(fd)==-1)) err(1,"%s",argv[1]);                 // 关闭文件


	/* 读取新文件到内存 */
	// 分配newsize+1字节而不是newsize字节，确保即使newsize=0也能正确工作
	if(((fd=open(argv[2],O_RDONLY,0))<0) ||                    // 以只读模式打开新文件
		((newsize=lseek(fd,0,SEEK_END))==-1) ||                // 获取文件大小
		((new=malloc(newsize+1))==NULL) ||                     // 分配内存
		(lseek(fd,0,SEEK_SET)!=0) ||                           // 定位到文件开头
		(read(fd,new,newsize)!=newsize) ||                     // 读取整个文件
		(close(fd)==-1)) err(1,"%s",argv[2]);                 // 关闭文件

	/* 创建补丁文件 */
	if ((pf = fopen(argv[3], "w")) == NULL)
		err(1, "%s", argv[3]);

	/* 写入补丁文件头（魔数+新文件大小）*/
	// 将新文件大小编码为8字节大端序格式
	offtout(newsize, buf);
	// 写入魔数"ENDSLEY/BSDIFF43"（16字节）
	if (fwrite("ENDSLEY/BSDIFF43", 16, 1, pf) != 1 ||
		fwrite(buf, sizeof(buf), 1, pf) != 1)                    // 写入新文件大小（8字节）
		err(1, "Failed to write header");


	/* 打开BZip2压缩流 */
	// 使用压缩级别9（最高压缩率）打开BZip2写入器
	if (NULL == (bz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)))
		errx(1, "BZ2_bzWriteOpen, bz2err=%d", bz2err);

	// 设置opaque指针指向BZip2句柄
	stream.opaque = bz2;
	// 调用bsdiff函数生成补丁数据
	if (bsdiff(old, oldsize, new, newsize, &stream))
		err(1, "bsdiff");

	/* 关闭BZip2压缩流 */
	BZ2_bzWriteClose(&bz2err, bz2, 0, NULL, NULL);
	if (bz2err != BZ_OK)
		err(1, "BZ2_bzWriteClose, bz2err=%d", bz2err);

	/* 关闭补丁文件 */
	if (fclose(pf))
		err(1, "fclose");

	/* 释放分配的内存 */
	free(old);
	free(new);

	return 0;
}

#endif
