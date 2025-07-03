/*
 * PSP Software Development Kit - https://github.com/pspdev
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in PSPSDK root for details.
 *
 * Copyright (c) 2005 Jesper Svennevid
 */

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <pspgu.h>
#include <pspgum.h>

#include "common/callbacks.h"

PSP_MODULE_INFO("Cube Sample", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static unsigned int __attribute__((aligned(16))) list[262144];
extern unsigned char logo_start[];

PSP_HEAP_SIZE_KB(1000);

struct Vertex
{
	float u, v;
	unsigned int color;
	float x,y,z;
};

static struct Vertex __attribute__((aligned(16))) vertices[12*3] =
{
	{0, 0, 0xff7f0000,-1,-1, 1}, // 0
	{1, 0, 0xff7f0000,-1, 1, 1}, // 4
	{1, 1, 0xff7f0000, 1, 1, 1}, // 5

	{0, 0, 0xff7f0000,-1,-1, 1}, // 0
	{1, 1, 0xff7f0000, 1, 1, 1}, // 5
	{0, 1, 0xff7f0000, 1,-1, 1}, // 1

	{0, 0, 0xff7f0000,-1,-1,-1}, // 3
	{1, 0, 0xff7f0000, 1,-1,-1}, // 2
	{1, 1, 0xff7f0000, 1, 1,-1}, // 6

	{0, 0, 0xff7f0000,-1,-1,-1}, // 3
	{1, 1, 0xff7f0000, 1, 1,-1}, // 6
	{0, 1, 0xff7f0000,-1, 1,-1}, // 7

	{0, 0, 0xff007f00, 1,-1,-1}, // 0
	{1, 0, 0xff007f00, 1,-1, 1}, // 3
	{1, 1, 0xff007f00, 1, 1, 1}, // 7

	{0, 0, 0xff007f00, 1,-1,-1}, // 0
	{1, 1, 0xff007f00, 1, 1, 1}, // 7
	{0, 1, 0xff007f00, 1, 1,-1}, // 4

	{0, 0, 0xff007f00,-1,-1,-1}, // 0
	{1, 0, 0xff007f00,-1, 1,-1}, // 3
	{1, 1, 0xff007f00,-1, 1, 1}, // 7

	{0, 0, 0xff007f00,-1,-1,-1}, // 0
	{1, 1, 0xff007f00,-1, 1, 1}, // 7
	{0, 1, 0xff007f00,-1,-1, 1}, // 4

	{0, 0, 0xff00007f,-1, 1,-1}, // 0
	{1, 0, 0xff00007f, 1, 1,-1}, // 1
	{1, 1, 0xff00007f, 1, 1, 1}, // 2

	{0, 0, 0xff00007f,-1, 1,-1}, // 0
	{1, 1, 0xff00007f, 1, 1, 1}, // 2
	{0, 1, 0xff00007f,-1, 1, 1}, // 3

	{0, 0, 0xff00007f,-1,-1,-1}, // 4
	{1, 0, 0xff00007f,-1,-1, 1}, // 7
	{1, 1, 0xff00007f, 1,-1, 1}, // 6

	{0, 0, 0xff00007f,-1,-1,-1}, // 4
	{1, 1, 0xff00007f, 1,-1, 1}, // 6
	{0, 1, 0xff00007f, 1,-1,-1}, // 5
};

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)


char status[1024 * 10] = {0};

int log_fd = -1;

#define LOG_FILE(...){ \
	if (log_fd < 0){ \
		log_fd = sceIoOpen("ms0:/landmine.log", PSP_O_CREAT | PSP_O_APPEND | PSP_O_WRONLY, 0777); \
	} \
	if (log_fd >=0){ \
		char _log_buf[128]; \
		int _log_len = sprintf(_log_buf, __VA_ARGS__); \
		sceIoWrite(log_fd, _log_buf, _log_len); \
		sceIoClose(log_fd); \
		log_fd = -1; \
	} \
}

#define LOG_SCREEN(...) { \
	sprintf(&status[strlen(status)], __VA_ARGS__); \
}

#define LOG_BOTH(...) { \
	LOG_FILE(__VA_ARGS__); \
	LOG_SCREEN(__VA_ARGS__); \
}

void print_status(){
	pspDebugScreenSetXY(0, 0);
	pspDebugScreenPrintf("%s", status);
}

struct block{
	char *start_addr;
	int size;
	SceUID id;
};

struct block blocks[32];

int num_blocks = 0;

static void read_blocks(){
	for(int i = 0;i < num_blocks;i++){
		LOG_BOTH("%s: reading block %d with size %d\n", __func__, blocks[i].id, blocks[i].size);
		int rewind = strlen(status);
		int num_error_addr = 0;;
		int last_j = 0;

		for(int j = 0;j < blocks[i].size;j++){
			if (j - last_j > 256 * 1024){
				LOG_SCREEN("%d/%d", j, blocks[i].size);
				sceKernelDelayThread(10000);
				last_j = j;
				status[rewind] = '\0';
			}

			uint64_t *val = (uint64_t *)&blocks[i].start_addr[j];
			if ((uint64_t)val % 8 == 0 && j + 8 <= blocks[i].size){
				if (*val != 0xaaaaaaaaaaaaaaaa){
					LOG_FILE("E 0x%x %d\n", &blocks[i].start_addr[j], blocks[i].id);
					num_error_addr++;
				}
				j+=7;
				continue;
			}

			if(blocks[i].start_addr[j] != 0xaa){
				LOG_FILE("E 0x%x %d\n", &blocks[i].start_addr[j], blocks[i].id);
				num_error_addr++;
			}
		}
		LOG_BOTH("%s: done reading allocated bytes, errors %d\n", __func__, num_error_addr);
	}
}

static void write_blocks(){
	LOG_BOTH("%s: writing allocated blocks\n", __func__);
	for(int i = 0;i < num_blocks;i++){
		memset(blocks[i].start_addr, 0xaa, blocks[i].size);
	}
	LOG_BOTH("%s: blocks written\n", __func__);
}

static void allocate_blocks(){
	num_blocks = 0;

	while(1){
		int max_free = sceKernelMaxFreeMemSize();
		if (max_free == 0){
			LOG_BOTH("%s: no more free memory\n", __func__);
			break;
		}
		int block_id = sceKernelAllocPartitionMemory(2, "block", PSP_SMEM_Low, max_free, 0);
		if (block_id < 0){
			LOG_BOTH("%s: allocation failed, 0x%x, max_free %d\n", __func__, block_id, max_free);
			break;
		}
		blocks[num_blocks].start_addr = sceKernelGetBlockHeadAddr(block_id);
		blocks[num_blocks].size = max_free;
		blocks[num_blocks].id = block_id;
		LOG_BOTH("%s: allocated %d at 0x%x with id %d\n", __func__, max_free, blocks[num_blocks].start_addr, block_id);
		num_blocks++;
	}
}

static void dealloc_blocks(){
	for (int i = 0;i > num_blocks;i++){
		sceKernelFreePartitionMemory(blocks[i].id);
	}
}

int polyphonic_main();
int audio_thread(unsigned int args, void *argp){
	polyphonic_main();
}

int test_done = 0;

int memtest_thread(unsigned int args, void *argp){
	sceKernelDelayThread(1000000 * 2);

	allocate_blocks();
	sceKernelDelayThread(1000000 * 2);
	write_blocks();
	sceKernelDelayThread(1000000 * 2);
	read_blocks();
	sceKernelDelayThread(1000000 * 2);
	LOG_BOTH("%s: test finished\n", __func__);
	test_done = 1;
	dealloc_blocks();

	return 0;
}

void protect_memory(){
	// test your memory protection here
	int uid_f0 = sceKernelAllocPartitionMemory(2, "SCE_PSPEMU_FLASHFS", PSP_SMEM_Addr, 0x20000, (void*)0x0B000000);
	int uid_scratch = sceKernelAllocPartitionMemory(2, "SCE_PSPEMU_SCRATCHPAD", PSP_SMEM_Addr, 0x30000, (void*)0x0BD00000);

	if (uid_f0 < 0){
		LOG_BOTH("%s: failed protecting flashfs, 0x%x\n", __func__, uid_f0);
	}
	if (uid_scratch < 0){
		LOG_BOTH("%s: failed protecting scratch pad, 0x%x\n", __func__, uid_scratch);
	}

	LOG_BOTH("%s: memory protection allocated\n", __func__);
}

int msgdialog_main();

int main(int argc, char* argv[])
{
	setupCallbacks();

	// setup GU

	pspDebugScreenInit();

	void* fbp0 = guGetStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_8888);
	void* fbp1 = guGetStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_8888);
	void* zbp = guGetStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_4444);

	sceGuInit();

	sceGuStart(GU_DIRECT,list);
	sceGuDrawBuffer(GU_PSM_8888,fbp0,BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,fbp1,BUF_WIDTH);
	sceGuDepthBuffer(zbp,BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH/2),2048 - (SCR_HEIGHT/2));
	sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
	sceGuDepthRange(65535,0);
	sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	int val = 0;	

	log_fd = sceIoOpen("ms0:/landmine.log", PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777); \

	protect_memory();

	// play some audio
	int tid = sceKernelCreateThread("audio", audio_thread, 0x18, 0x1000, PSP_THREAD_ATTR_VFPU, NULL);
	if (tid < 0){
		LOG_BOTH("%s: failed creating audio thread\n", __func__);
	}else{
		sceKernelStartThread(tid, 0, NULL);	
	}

	// memalloc test
	tid = sceKernelCreateThread("tester", memtest_thread, 0x18, 0x1000, 0, NULL);
	if (tid < 0){
		LOG_BOTH("%s: failed creating tester thread\n", __func__);
	}else{
		sceKernelStartThread(tid, 0, NULL);	
	}

	while(running() && !test_done)
	{
		sceGuStart(GU_DIRECT,list);

		// clear screen

		sceGuClearColor(0xff554433);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

		// setup matrices for cube

		sceGumMatrixMode(GU_PROJECTION);
		sceGumLoadIdentity();
		sceGumPerspective(75.0f,16.0f/9.0f,0.5f,1000.0f);

		sceGumMatrixMode(GU_VIEW);
		sceGumLoadIdentity();

		sceGumMatrixMode(GU_MODEL);
		sceGumLoadIdentity();
		{
			ScePspFVector3 pos = { 0, 0, -2.5f };
			ScePspFVector3 rot = { val * 0.79f * (GU_PI/180.0f), val * 0.98f * (GU_PI/180.0f), val * 1.32f * (GU_PI/180.0f) };
			sceGumTranslate(&pos);
			sceGumRotateXYZ(&rot);
		}

		// setup texture

		sceGuTexMode(GU_PSM_4444,0,0,0);
		sceGuTexImage(0,64,64,64,logo_start);
		sceGuTexFunc(GU_TFX_ADD,GU_TCC_RGB);
		sceGuTexEnvColor(0xffff00);
		sceGuTexFilter(GU_LINEAR,GU_LINEAR);
		sceGuTexScale(1.0f,1.0f);
		sceGuTexOffset(0.0f,0.0f);
		sceGuAmbientColor(0xffffffff);

		// draw cube

		sceGumDrawArray(GU_TRIANGLES,GU_TEXTURE_32BITF|GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,12*3,0,vertices);

		sceGuFinish();
		sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

		sceDisplayWaitVblankStart();

		print_status();

		sceGuSwapBuffers();

		val++;
	}

	LOG_BOTH("%s: testing one more time with dialog box active\n", __func__);

	// memalloc test again while dialog is running
	sceKernelDeleteThread(tid);
	tid = sceKernelCreateThread("tester", memtest_thread, 0x18, 0x1000, 0, NULL);
	if (tid < 0){
		LOG_BOTH("%s: failed creating tester thread\n", __func__);
	}else{
		sceKernelStartThread(tid, 0, NULL);
	}

	msgdialog_main();

	sceGuTerm();

	sceKernelExitGame();
	return 0;
}
