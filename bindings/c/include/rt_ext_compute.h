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
typedef void (*PFN_rtCmdStorageBuffer)(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size);
typedef void (*PFN_rtCmdStorageTexture)(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view);
typedef void (*PFN_rtCmdComputeBarrier)(rt_command_buffer command_buffer);
typedef void (*PFN_rtCmdDispatch)(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z);

extern PFN_rtComputeProgramCreate rt_rtComputeProgramCreate;
extern PFN_rtComputeProgramDestroy rt_rtComputeProgramDestroy;
extern PFN_rtComputeProgramShader rt_rtComputeProgramShader;
extern PFN_rtComputeProgramLink rt_rtComputeProgramLink;
extern PFN_rtCmdUseComputeProgram rt_rtCmdUseComputeProgram;
extern PFN_rtCmdStorageBuffer rt_rtCmdStorageBuffer;
extern PFN_rtCmdStorageTexture rt_rtCmdStorageTexture;
extern PFN_rtCmdComputeBarrier rt_rtCmdComputeBarrier;
extern PFN_rtCmdDispatch rt_rtCmdDispatch;
bool rtLoad_RT_EXT_COMPUTE(void);

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
static inline void rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size) {
	rt_rtCmdStorageBuffer(command_buffer, binding, buffer, offset, size);
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
PFN_rtCmdStorageBuffer rt_rtCmdStorageBuffer = NULL;
PFN_rtCmdStorageTexture rt_rtCmdStorageTexture = NULL;
PFN_rtCmdComputeBarrier rt_rtCmdComputeBarrier = NULL;
PFN_rtCmdDispatch rt_rtCmdDispatch = NULL;

bool rtLoad_RT_EXT_COMPUTE(void) {
	rt_rtComputeProgramCreate = (PFN_rtComputeProgramCreate)rtGetProc("rtComputeProgramCreate");
	rt_rtComputeProgramDestroy = (PFN_rtComputeProgramDestroy)rtGetProc("rtComputeProgramDestroy");
	rt_rtComputeProgramShader = (PFN_rtComputeProgramShader)rtGetProc("rtComputeProgramShader");
	rt_rtComputeProgramLink = (PFN_rtComputeProgramLink)rtGetProc("rtComputeProgramLink");
	rt_rtCmdUseComputeProgram = (PFN_rtCmdUseComputeProgram)rtGetProc("rtCmdUseComputeProgram");
	rt_rtCmdStorageBuffer = (PFN_rtCmdStorageBuffer)rtGetProc("rtCmdStorageBuffer");
	rt_rtCmdStorageTexture = (PFN_rtCmdStorageTexture)rtGetProc("rtCmdStorageTexture");
	rt_rtCmdComputeBarrier = (PFN_rtCmdComputeBarrier)rtGetProc("rtCmdComputeBarrier");
	rt_rtCmdDispatch = (PFN_rtCmdDispatch)rtGetProc("rtCmdDispatch");

	return rt_rtComputeProgramCreate &&
		   rt_rtComputeProgramDestroy &&
		   rt_rtComputeProgramShader &&
		   rt_rtComputeProgramLink &&
		   rt_rtCmdUseComputeProgram &&
		   rt_rtCmdStorageBuffer &&
		   rt_rtCmdStorageTexture &&
		   rt_rtCmdComputeBarrier &&
		   rt_rtCmdDispatch;
}

#endif /* RUTILE_IMPL */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* RT_EXT_COMPUTE_H */
