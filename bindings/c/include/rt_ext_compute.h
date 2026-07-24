#ifndef RT_EXT_COMPUTE_H
#define RT_EXT_COMPUTE_H

/*
 * RT_EXT_COMPUTE extension package.
 */

#include "rutile.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct rt_compute_program_t* rt_compute_program;

typedef rt_compute_program (*PFN_rtComputeProgramCreate)(void);
typedef void (*PFN_rtComputeProgramDestroy)(rt_compute_program program);
typedef void (*PFN_rtComputeProgramShader)(rt_compute_program program, u64 size, const void* data);
typedef void (*PFN_rtComputeProgramLink)(rt_compute_program program);
typedef void (*PFN_rtCmdUseComputeProgram)(rt_command_buffer command_buffer, rt_compute_program program);
typedef void (*PFN_rtCmdStorageTexture)(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view);
typedef void (*PFN_rtCmdComputeBarrier)(rt_command_buffer command_buffer);
typedef void (*PFN_rtCmdDispatch)(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z);

extern PFN_rtComputeProgramCreate rt_rtComputeProgramCreate;
extern PFN_rtComputeProgramDestroy rt_rtComputeProgramDestroy;
extern PFN_rtComputeProgramShader rt_rtComputeProgramShader;
extern PFN_rtComputeProgramLink rt_rtComputeProgramLink;
extern PFN_rtCmdUseComputeProgram rt_rtCmdUseComputeProgram;
extern PFN_rtCmdStorageTexture rt_rtCmdStorageTexture;
extern PFN_rtCmdComputeBarrier rt_rtCmdComputeBarrier;
extern PFN_rtCmdDispatch rt_rtCmdDispatch;
enum rt_error rtLoad_RT_EXT_COMPUTE(void);

#ifndef RT_NO_API_WRAPPERS
static inline rt_compute_program rtComputeProgramCreate(void) {
	return rt_rtComputeProgramCreate();
}
static inline void rtComputeProgramDestroy(rt_compute_program program) {
	rt_rtComputeProgramDestroy(program);
}
static inline void rtComputeProgramShader(rt_compute_program program, u64 size, const void* data) {
	rt_rtComputeProgramShader(program, size, data);
}
static inline void rtComputeProgramLink(rt_compute_program program) {
	rt_rtComputeProgramLink(program);
}
static inline void rtCmdUseComputeProgram(rt_command_buffer command_buffer, rt_compute_program program) {
	rt_rtCmdUseComputeProgram(command_buffer, program);
}
static inline void rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view) {
	rt_rtCmdStorageTexture(command_buffer, binding, texture_view);
}
static inline void rtCmdComputeBarrier(rt_command_buffer command_buffer) {
	rt_rtCmdComputeBarrier(command_buffer);
}
static inline void rtCmdDispatch(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z) {
	rt_rtCmdDispatch(command_buffer, group_count_x, group_count_y, group_count_z);
}
#endif

#ifdef RUTILE_IMPL

PFN_rtComputeProgramCreate rt_rtComputeProgramCreate = NULL;
PFN_rtComputeProgramDestroy rt_rtComputeProgramDestroy = NULL;
PFN_rtComputeProgramShader rt_rtComputeProgramShader = NULL;
PFN_rtComputeProgramLink rt_rtComputeProgramLink = NULL;
PFN_rtCmdUseComputeProgram rt_rtCmdUseComputeProgram = NULL;
PFN_rtCmdStorageTexture rt_rtCmdStorageTexture = NULL;
PFN_rtCmdComputeBarrier rt_rtCmdComputeBarrier = NULL;
PFN_rtCmdDispatch rt_rtCmdDispatch = NULL;

typedef bool (*PFN_rtInit_RT_EXT_COMPUTE)(void);

static void rt__clear_RT_EXT_COMPUTE(void) {
	rt_rtComputeProgramCreate = NULL;
	rt_rtComputeProgramDestroy = NULL;
	rt_rtComputeProgramShader = NULL;
	rt_rtComputeProgramLink = NULL;
	rt_rtCmdUseComputeProgram = NULL;
	rt_rtCmdStorageTexture = NULL;
	rt_rtCmdComputeBarrier = NULL;
	rt_rtCmdDispatch = NULL;
}

enum rt_error rtLoad_RT_EXT_COMPUTE(void) {
	PFN_rtInit_RT_EXT_COMPUTE init = (PFN_rtInit_RT_EXT_COMPUTE)rtGetProc("rtInit_RT_EXT_COMPUTE");
	rt__clear_RT_EXT_COMPUTE();
	if (init && !init()) {
		enum rt_error err = rtError();
		return err != RT_SUCCESS ? err : RT_UNSUPPORTED_FEATURE;
	}
	rt_rtComputeProgramCreate = (PFN_rtComputeProgramCreate)rtGetProc("rtComputeProgramCreate");
	rt_rtComputeProgramDestroy = (PFN_rtComputeProgramDestroy)rtGetProc("rtComputeProgramDestroy");
	rt_rtComputeProgramShader = (PFN_rtComputeProgramShader)rtGetProc("rtComputeProgramShader");
	rt_rtComputeProgramLink = (PFN_rtComputeProgramLink)rtGetProc("rtComputeProgramLink");
	rt_rtCmdUseComputeProgram = (PFN_rtCmdUseComputeProgram)rtGetProc("rtCmdUseComputeProgram");
	rt_rtCmdStorageTexture = (PFN_rtCmdStorageTexture)rtGetProc("rtCmdStorageTexture");
	rt_rtCmdComputeBarrier = (PFN_rtCmdComputeBarrier)rtGetProc("rtCmdComputeBarrier");
	rt_rtCmdDispatch = (PFN_rtCmdDispatch)rtGetProc("rtCmdDispatch");

	if (rt_rtComputeProgramCreate &&
		rt_rtComputeProgramDestroy &&
		rt_rtComputeProgramShader &&
		rt_rtComputeProgramLink &&
		rt_rtCmdUseComputeProgram &&
		rt_rtCmdStorageTexture &&
		rt_rtCmdComputeBarrier &&
		rt_rtCmdDispatch) {
		return RT_SUCCESS;
	}
	rt__clear_RT_EXT_COMPUTE();
	return RT_EXTENSION_NOT_PRESENT;
}

#endif /* RUTILE_IMPL */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RT_EXT_COMPUTE_H */
