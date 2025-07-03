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
		char _log_buf[512]; \
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

		sceKernelDcacheInvalidateRange(blocks[i].start_addr, blocks[i].size);

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

		if (num_error_addr != 0){
			sceKernelDelayThread(1000000 * 5);
			sceKernelExitGame();
		}
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
	num_blocks = 0;
}

int polyphonic_main();
int audio_thread(unsigned int args, void *argp){
	polyphonic_main();
}

int test_done = 0;
int write_block = 0;
int loop_test = 0;
int memtest_thread(unsigned int args, void *argp){
	test_done = 0;
	int passes = 0;

	do{
		if (passes % 5 == 0 && loop_test){
			status[0] = '\0';
		}
		if (num_blocks == 0){
			sceKernelDelayThread(1000000 * 2);
			allocate_blocks();
		}
		if (write_block){
			sceKernelDelayThread(1000000 * 2);
			write_blocks();
		}
		sceKernelDelayThread(1000000 * 2);
		read_blocks();
		sceKernelDelayThread(1000000 * 2);
		//dealloc_blocks();
		passes++;
		LOG_BOTH("%s: pass %d finished\n", __func__, passes);
	}while(loop_test);

	test_done = 1;

	return 0;
}

void protect_memory(){
	// test your memory protection here
	#if 0
	int uid_f0 = sceKernelAllocPartitionMemory(2, "SCE_PSPEMU_FLASHFS", PSP_SMEM_Addr, 0x20000, (void*)0x0B000000);
	int uid_scratch = sceKernelAllocPartitionMemory(2, "SCE_PSPEMU_SCRATCHPAD", PSP_SMEM_Addr, 0x30000, (void*)0x0BD00000);

	if (uid_f0 < 0){
		LOG_BOTH("%s: failed protecting flashfs, 0x%x\n", __func__, uid_f0);
	}
	if (uid_scratch < 0){
		LOG_BOTH("%s: failed protecting scratch pad, 0x%x\n", __func__, uid_scratch);
	}

	LOG_BOTH("%s: memory protection allocated\n", __func__);
	#endif
}

int msgdialog_main();

static void archive_flash_to_ms(const char *directory, int num){
	SceUID dhandle = sceIoDopen(directory);
	if (dhandle < 0){
		LOG_BOTH("%s: sceIoDopen on %s failed, 0x%x\n", __func__, directory, dhandle);
		return;
	}

	char mspath[255];
	char efpath[255];
	if (strlen(directory) - strlen("flash0:/") > 0){
		sprintf(mspath, "ms0:/landmine_f%d/%s", num, &directory[strlen("flash0:/")]);
		sprintf(efpath, "ef0:/landmine_f%d/%s", num, &directory[strlen("flash0:/")]);
	}else{
		sprintf(mspath, "ms0:/landmine_f%d/", num);
		sprintf(efpath, "ef0:/landmine_f%d/", num);
	}
	mspath[strlen(mspath) - 1] = '\0';
	efpath[strlen(efpath) - 1] = '\0';
	int mkdir_status_ms = sceIoMkdir(mspath, 0777);
	int mkdir_status_ef = sceIoMkdir(efpath, 0777);
	//if (mkdir_status_ms != 0) LOG_FILE("%s: failed creating %s, 0x%x\n", __func__, mspath, mkdir_status_ms);
	//if (mkdir_status_ef != 0) LOG_FILE("%s: failed creating %s, 0x%x\n", __func__, efpath, mkdir_status_ms);
	mspath[strlen(mspath)] = '/';
	efpath[strlen(efpath)] = '/';

	while(1){
		SceIoDirent entry;
		int dread_status = sceIoDread(dhandle, &entry);
		if (dread_status == 0){
			break;
		}
		if (dread_status < 0){
			LOG_FILE("%s: bad dread status 0x%x\n", __func__, dread_status);
			break;
		}
		if (strcmp(entry.d_name, "..") == 0 || strcmp(entry.d_name, ".") == 0){
			continue;
		}

		char fullpath[255];
		if (FIO_SO_ISDIR(entry.d_stat.st_attr)){
			sprintf(fullpath, "%s%s/", directory, entry.d_name);
			archive_flash_to_ms(fullpath, num);
		}else{
			sprintf(fullpath, "%s%s", directory, entry.d_name);
			char fullmspath[255];
			char fullefpath[255];
			sprintf(fullmspath, "%s%s", mspath, entry.d_name);
			sprintf(fullefpath, "%s%s", efpath, entry.d_name);

			SceUID f0 = sceIoOpen(fullpath, PSP_O_RDONLY, 0777);
			SceUID ms = sceIoOpen(fullmspath, PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
			SceUID ef = sceIoOpen(fullefpath, PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);

			if (f0 < 0 || (ms < 0 && ef < 0)){
				//LOG_FILE("%s: failed creating file entry for %s, f0 0x%x, %s 0x%x, %s 0x%x\n", __func__, fullpath, f0, fullmspath, ms, fullefpath, ef);
				if (f0 >= 0) sceIoClose(f0);
				if (ms >= 0) sceIoClose(ms);
				if (ef >= 0) sceIoClose(ef);
				continue;
			}

			char cpy_buf[1024 * 8];
			while(1){
				sceKernelDelayThread(10000);
				int len = sceIoRead(f0, cpy_buf, sizeof(cpy_buf));
				if (len == 0){
					sceIoClose(f0);
					if (ms >= 0) sceIoClose(ms);
					if (ef >= 0) sceIoClose(ef);
					break;
				}
				if (len < 0){
					LOG_FILE("%s: failed reading from f0, 0x%x\n", __func__)
					sceIoClose(f0);
					if (ms >= 0) sceIoClose(ms);
					if (ef >= 0) sceIoClose(ef);
					break;
				}
				if (ms >= 0) sceIoWrite(ms, cpy_buf, len);
				if (ef >= 0) sceIoWrite(ef, cpy_buf, len);
			}

			//LOG_FILE("%s: archived %s\n", __func__, fullpath);
		}
	}

	sceIoDclose(dhandle);
}

static void generate_dummy_file(){
	int fd = sceIoOpen("ms0:/landmine_dummy.bin", PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777);
	if (fd < 0){
		LOG_BOTH("%s: failed opening file for writing, 0x%x\n", __func__, fd);
		return;
	}
	char write_buf[1024] = {0};
	memset(write_buf, 0xaa, sizeof(write_buf));
	for(int i = 0;i < 10;i++){
		int write_status = sceIoWrite(fd, write_buf, sizeof(write_buf));
		if (write_status < 0){
			LOG_BOTH("%s: failed writing file, 0x%x\n", __func__, write_status);
			sceIoClose(fd);
			return;
		}
	}
	sceIoClose(fd);
	LOG_BOTH("%s: dummy file generated\n", __func__);
}

static void read_dummy_file(){
	int fd = sceIoOpen("ms0:/landmine_dummy.bin", PSP_O_RDONLY, 0777);
	if (fd < 0){
		LOG_FILE("%s: failed opening file for reading, 0x%x\n", __func__, fd);
		return;
	}
	uint32_t read_buf[1024 / sizeof(uint32_t)] = {0};
	for(int i = 0;i < 10;i++){
		int read_status = sceIoRead(fd, read_buf, sizeof(read_buf));
		if (read_status != sizeof(read_buf)){
			LOG_FILE("%s: failed reading file, 0x%x\n", __func__, read_status);
			break;
		}
		for(int j = 0;j < sizeof(read_buf) / sizeof(uint32_t);j++){
			if (read_buf[j] != 0xaaaaaaaa){
				LOG_FILE("%s: file corruption detected\n", __func__);
				break;
			}
		}
		sceKernelDelayThread(10000);
	}
	sceIoClose(fd);
}

int io_thread_2(unsigned int args, void *argp){
	generate_dummy_file();

	while(1){
		read_dummy_file();
	}
}

int io_thread(unsigned int args, void *argp){
	while(1){
		archive_flash_to_ms("flash0:/", 0);
	}
}

int main(int argc, char* argv[])
{
	log_fd = sceIoOpen("ms0:/landmine.log", PSP_O_CREAT | PSP_O_TRUNC | PSP_O_WRONLY, 0777); \

	protect_memory();

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

	#if 1
	// background IO
	int tid = sceKernelCreateThread("io", io_thread, 0x18, 0x12000, 0, NULL);
	if (tid < 0){
		LOG_BOTH("%s: failed creating io thread\n", __func__);
	}else{
		sceKernelStartThread(tid, 0, NULL);
		LOG_BOTH("%s: background IO started\n", __func__);
	}

	// background IO
	tid = sceKernelCreateThread("io", io_thread_2, 0x18, 0x12000, 0, NULL);
	if (tid < 0){
		LOG_BOTH("%s: failed creating io thread 2\n", __func__);
	}else{
		sceKernelStartThread(tid, 0, NULL);
		LOG_BOTH("%s: background IO 2 started\n", __func__);
	}

	#endif

	// play some audio
	tid = sceKernelCreateThread("audio", audio_thread, 0x18, 0x1000, PSP_THREAD_ATTR_VFPU, NULL);
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
		loop_test = 0;
		write_block = 1;
		sceKernelStartThread(tid, 0, NULL);
	}

	while(!test_done)
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

	LOG_BOTH("%s: testing if memory were overwritten\n", __func__);

	// memalloc test again while dialog is running
	sceKernelDeleteThread(tid);
	tid = sceKernelCreateThread("tester", memtest_thread, 0x18, 0x1000, 0, NULL);
	if (tid < 0){
		LOG_BOTH("%s: failed creating tester thread\n", __func__);
	}else{
		loop_test = 1;
		write_block = 0;
		sceKernelStartThread(tid, 0, NULL);
	}

	msgdialog_main();

	sceGuTerm();

	sceKernelExitGame();
	return 0;
}
