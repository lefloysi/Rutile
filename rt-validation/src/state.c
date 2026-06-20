#include "handles.h"
#include "logger.h"
#include "procs.h"
#include "queue.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

PFN_rtInit rtval_next_rtInit = NULL;
PFN_rtExit rtval_next_rtExit = NULL;
PFN_rtError rtval_next_rtError = NULL;
PFN_rtErrorMessage rtval_next_rtErrorMessage = NULL;
PFN_rtClearError rtval_next_rtClearError = NULL;
PFN_rtGetName rtval_next_rtGetName = NULL;
PFN_rtQueryFormatCapabilities rtval_next_rtQueryFormatCapabilities = NULL;
PFN_rtBufferCreate rtval_next_rtBufferCreate = NULL;
PFN_rtBufferDestroy rtval_next_rtBufferDestroy = NULL;
PFN_rtBufferData rtval_next_rtBufferData = NULL;
PFN_rtBufferSubdata rtval_next_rtBufferSubdata = NULL;
PFN_rtBufferRead rtval_next_rtBufferRead = NULL;
PFN_rtBufferMap rtval_next_rtBufferMap = NULL;
PFN_rtBufferUnmap rtval_next_rtBufferUnmap = NULL;
PFN_rtTextureCreate rtval_next_rtTextureCreate = NULL;
PFN_rtTextureDestroy rtval_next_rtTextureDestroy = NULL;
PFN_rtTextureViewCreate rtval_next_rtTextureViewCreate = NULL;
PFN_rtTextureViewDestroy rtval_next_rtTextureViewDestroy = NULL;
PFN_rtTextureViewFilter rtval_next_rtTextureViewFilter = NULL;
PFN_rtTextureViewAddress rtval_next_rtTextureViewAddress = NULL;
PFN_rtTextureViewAnisotropy rtval_next_rtTextureViewAnisotropy = NULL;
PFN_rtTextureViewLod rtval_next_rtTextureViewLod = NULL;
PFN_rtTextureCopy rtval_next_rtTextureCopy = NULL;
PFN_rtTextureData rtval_next_rtTextureData = NULL;
PFN_rtTextureSubcopy rtval_next_rtTextureSubcopy = NULL;
PFN_rtTextureSubdata rtval_next_rtTextureSubdata = NULL;
PFN_rtTextureViewCopyToBuffer rtval_next_rtTextureViewCopyToBuffer = NULL;
PFN_rtTextureViewExtent rtval_next_rtTextureViewExtent = NULL;
PFN_rtFramebufferCreate rtval_next_rtFramebufferCreate = NULL;
PFN_rtFramebufferDestroy rtval_next_rtFramebufferDestroy = NULL;
PFN_rtFramebufferColorView rtval_next_rtFramebufferColorView = NULL;
PFN_rtFramebufferSetColorView rtval_next_rtFramebufferSetColorView = NULL;
PFN_rtFramebufferDepthView rtval_next_rtFramebufferDepthView = NULL;
PFN_rtGraphicsProgramCreate rtval_next_rtGraphicsProgramCreate = NULL;
PFN_rtGraphicsProgramDestroy rtval_next_rtGraphicsProgramDestroy = NULL;
PFN_rtGraphicsProgramVertexLayout rtval_next_rtGraphicsProgramVertexLayout = NULL;
PFN_rtGraphicsProgramVertexShader rtval_next_rtGraphicsProgramVertexShader = NULL;
PFN_rtGraphicsProgramFragmentShader rtval_next_rtGraphicsProgramFragmentShader = NULL;
PFN_rtGraphicsProgramRasterState rtval_next_rtGraphicsProgramRasterState = NULL;
PFN_rtGraphicsProgramBlendState rtval_next_rtGraphicsProgramBlendState = NULL;
PFN_rtGraphicsProgramLink rtval_next_rtGraphicsProgramLink = NULL;
PFN_rtGraphicsProgramUniformLocation rtval_next_rtGraphicsProgramUniformLocation = NULL;
PFN_rtComputeProgramCreate rtval_next_rtComputeProgramCreate = NULL;
PFN_rtComputeProgramDestroy rtval_next_rtComputeProgramDestroy = NULL;
PFN_rtComputeProgramShader rtval_next_rtComputeProgramShader = NULL;
PFN_rtComputeProgramLink rtval_next_rtComputeProgramLink = NULL;
PFN_rtCmdCreate rtval_next_rtCmdCreate = NULL;
PFN_rtCmdDestroy rtval_next_rtCmdDestroy = NULL;
PFN_rtCmdBegin rtval_next_rtCmdBegin = NULL;
PFN_rtCmdBeginRendering rtval_next_rtCmdBeginRendering = NULL;
PFN_rtCmdClearColor rtval_next_rtCmdClearColor = NULL;
PFN_rtCmdClearDepth rtval_next_rtCmdClearDepth = NULL;
PFN_rtCmdClearStencil rtval_next_rtCmdClearStencil = NULL;
PFN_rtCmdUseGraphicsProgram rtval_next_rtCmdUseGraphicsProgram = NULL;
PFN_rtCmdSetScissor rtval_next_rtCmdSetScissor = NULL;
PFN_rtCmdUseComputeProgram rtval_next_rtCmdUseComputeProgram = NULL;
PFN_rtCmdUniformBuffer rtval_next_rtCmdUniformBuffer = NULL;
PFN_rtCmdUniformTexture rtval_next_rtCmdUniformTexture = NULL;
PFN_rtCmdStorageBuffer rtval_next_rtCmdStorageBuffer = NULL;
PFN_rtCmdStorageTexture rtval_next_rtCmdStorageTexture = NULL;
PFN_rtCmdComputeBarrier rtval_next_rtCmdComputeBarrier = NULL;
PFN_rtCmdBindVertexBuffer rtval_next_rtCmdBindVertexBuffer = NULL;
PFN_rtCmdDraw rtval_next_rtCmdDraw = NULL;
PFN_rtCmdDispatch rtval_next_rtCmdDispatch = NULL;
PFN_rtCmdEndRendering rtval_next_rtCmdEndRendering = NULL;
PFN_rtCmdEnd rtval_next_rtCmdEnd = NULL;
PFN_rtQueueQuery rtval_next_rtQueueQuery = NULL;
PFN_rtQueueWait rtval_next_rtQueueWait = NULL;
PFN_rtQueueSubmit rtval_next_rtQueueSubmit = NULL;
PFN_rtQueueFlush rtval_next_rtQueueFlush = NULL;
PFN_rtTimepointWait rtval_next_rtTimepointWait = NULL;
PFN_rtTimepointReached rtval_next_rtTimepointReached = NULL;
PFN_rtSwapchainCreate rtval_next_rtSwapchainCreate = NULL;
PFN_rtSwapchainDestroy rtval_next_rtSwapchainDestroy = NULL;
PFN_rtSwapchainResize rtval_next_rtSwapchainResize = NULL;
PFN_rtSwapchainAcquire rtval_next_rtSwapchainAcquire = NULL;
PFN_rtSwapchainPresent rtval_next_rtSwapchainPresent = NULL;
PFN_rtSwapchainBindWindowGLFW rtval_next_rtSwapchainBindWindowGLFW = NULL;
PFN_rtSetOutput rtval_next_rtSetOutput = NULL;

/*===============================================================================================*/
/* central handle registry: open-addressing hashmap. Each slot owns a heap-allocated payload so   */
/* pointers returned by rtval_handle_payload stay valid across registry grows — entries that      */
/* nest into rtval_handle_create (eg. acquire wrapping a framebuffer) rely on this stability.     */
/*===============================================================================================*/

#define RTVAL_HANDLE_PAYLOAD_SIZE 64
#define RTVAL_HANDLE_EMPTY        NULL
#define RTVAL_HANDLE_TOMBSTONE    ((void*)~(uintptr_t)0)

typedef struct rtval_handle_slot {
	void*             key;
	rtval_handle_type type;
	void*             payload;  /* heap-allocated so the pointer stays stable across registry grows */
} rtval_handle_slot;

static const char* rtval_handle_type_name(rtval_handle_type t) {
	switch (t) {
		case RTVAL_HANDLE_TYPE_BUFFER:           return "buffer";
		case RTVAL_HANDLE_TYPE_TEXTURE:          return "texture";
		case RTVAL_HANDLE_TYPE_TEXTURE_VIEW:     return "texture_view";
		case RTVAL_HANDLE_TYPE_FRAMEBUFFER:      return "framebuffer";
		case RTVAL_HANDLE_TYPE_GRAPHICS_PROGRAM: return "graphics_program";
		case RTVAL_HANDLE_TYPE_COMPUTE_PROGRAM:  return "compute_program";
		case RTVAL_HANDLE_TYPE_COMMAND_BUFFER:   return "command_buffer";
		case RTVAL_HANDLE_TYPE_QUEUE:            return "queue";
		case RTVAL_HANDLE_TYPE_SWAPCHAIN:        return "swapchain";
		default:                                 return "unknown";
	}
}

static rtval_handle_slot* rtval_handle_slots    = NULL;
static usize              rtval_handle_capacity = 0;  /* power of two */
static usize              rtval_handle_count    = 0;  /* live entries (excludes tombstones) */
static usize              rtval_handle_used     = 0;  /* live + tombstones */

static usize rtval_hash_pointer(void* h) {
	uintptr_t p = (uintptr_t)h;
	p ^= p >> 33;
	p *= (uintptr_t)0xff51afd7ed558ccdull;
	p ^= p >> 33;
	p *= (uintptr_t)0xc4ceb9fe1a85ec53ull;
	p ^= p >> 33;
	return (usize)p;
}

static rtval_handle_slot* rtval_find_handle_slot(void* key, rtval_handle_slot* slots, usize capacity) {
	usize mask = capacity - 1;
	usize i = rtval_hash_pointer(key) & mask;
	rtval_handle_slot* tombstone = NULL;
	for (usize probe = 0; probe < capacity; ++probe) {
		void* k = slots[i].key;
		if (k == RTVAL_HANDLE_EMPTY)          { return tombstone ? tombstone : &slots[i]; }
		if (k == RTVAL_HANDLE_TOMBSTONE)      { if (!tombstone) { tombstone = &slots[i]; } }
		else if (k == key)                    { return &slots[i]; }
		i = (i + 1) & mask;
	}
	return tombstone;
}

static void rtval_grow_registry(void) {
	usize new_capacity = rtval_handle_capacity ? rtval_handle_capacity * 2 : 64;
	rtval_handle_slot* new_slots = calloc(new_capacity, sizeof(*new_slots));
	if (!new_slots) { return; }
	for (usize i = 0; i < rtval_handle_capacity; i++) {
		void* k = rtval_handle_slots[i].key;
		if (k == RTVAL_HANDLE_EMPTY || k == RTVAL_HANDLE_TOMBSTONE) { continue; }
		rtval_handle_slot* dst = rtval_find_handle_slot(k, new_slots, new_capacity);
		if (!dst) {
			free(new_slots);
			return;
		}
		*dst = rtval_handle_slots[i];
	}
	free(rtval_handle_slots);
	rtval_handle_slots = new_slots;
	rtval_handle_capacity = new_capacity;
	rtval_handle_used = rtval_handle_count;
}

void* rtval_handle_create(rtval_handle_type type) {
	if (!rtval_handle_capacity || rtval_handle_used * 2 >= rtval_handle_capacity) {
		rtval_grow_registry();
	}
	if (!rtval_handle_capacity) { return NULL; }
	void* key = malloc(1);
	if (!key) { return NULL; }

	rtval_handle_slot* slot = rtval_find_handle_slot(key, rtval_handle_slots, rtval_handle_capacity);
	if (!slot) {
		free(key);
		return NULL;
	}
	if (slot->key == RTVAL_HANDLE_EMPTY) { rtval_handle_used++; }
	void* payload = calloc(1, RTVAL_HANDLE_PAYLOAD_SIZE);
	if (!payload) {
		free(key);
		return NULL;
	}
	slot->key = key;
	slot->type = type;
	slot->payload = payload;
	rtval_handle_count++;
	return key;
}

static rtval_handle_slot* rtval_find_handle_slot_by_payload(void* payload, rtval_handle_slot* slots, usize capacity) {
	if (!payload || !slots || !capacity) { return NULL; }
	for (usize i = 0; i < capacity; i++) {
		if (slots[i].key == RTVAL_HANDLE_EMPTY || slots[i].key == RTVAL_HANDLE_TOMBSTONE) { continue; }
		if (slots[i].payload == payload) { return &slots[i]; }
	}
	return NULL;
}

void rtval_handle_report_leaks(void) {
	u32 counts[RTVAL_HANDLE_TYPE_COUNT] = { 0 };
	for (usize i = 0; i < rtval_handle_capacity; i++) {
		void* k = rtval_handle_slots[i].key;
		if (k == RTVAL_HANDLE_EMPTY || k == RTVAL_HANDLE_TOMBSTONE) { continue; }
		rtval_handle_type t = rtval_handle_slots[i].type;
		if ((u32)t < RTVAL_HANDLE_TYPE_COUNT) { counts[t]++; }
	}
	bool any = false;
	for (u32 t = 0; t < RTVAL_HANDLE_TYPE_COUNT; t++) {
		if (!counts[t]) { continue; }
		if (!any) { rtval_printf("[validation] leaked handles at shutdown:\n"); any = true; }
		rtval_printf("[validation]   %s: %u\n", rtval_handle_type_name((rtval_handle_type)t), counts[t]);
	}
	if (!any) { return; }
	for (usize i = 0; i < rtval_handle_capacity; i++) {
		void* k = rtval_handle_slots[i].key;
		if (k == RTVAL_HANDLE_EMPTY || k == RTVAL_HANDLE_TOMBSTONE) { continue; }
		rtval_printf("[validation]     live %s handle=%p\n", rtval_handle_type_name(rtval_handle_slots[i].type), k);
	}
}

void* rtval_handle_payload(void* key) {
	if (!key || key == RTVAL_HANDLE_TOMBSTONE || !rtval_handle_capacity || !rtval_handle_slots) { return NULL; }
	rtval_handle_slot* slot = rtval_find_handle_slot(key, rtval_handle_slots, rtval_handle_capacity);
	if (!slot) { return NULL; }
	if (slot->key != key) { return NULL; }
	return slot->payload;
}

bool rtval_handle_is_live(void* key) {
	return rtval_handle_payload(key) != NULL;
}

void rtval_handle_destroy(void* key) {
	if (!key || key == RTVAL_HANDLE_TOMBSTONE || !rtval_handle_capacity || !rtval_handle_slots) { return; }
	rtval_handle_slot* slot = rtval_find_handle_slot(key, rtval_handle_slots, rtval_handle_capacity);
	if (!slot || (slot->key != key && slot->payload != key)) {
		slot = rtval_find_handle_slot_by_payload(key, rtval_handle_slots, rtval_handle_capacity);
		if (!slot) { return; }
	}
	if (slot->key) {
		free(slot->key);
		free(slot->payload);
		slot->payload = NULL;
		slot->key = RTVAL_HANDLE_TOMBSTONE;
		rtval_handle_count--;
	}
}

void rtval_handle_reset_registry(void) {
	for (usize i = 0; i < rtval_handle_capacity; i++) {
		void* k = rtval_handle_slots ? rtval_handle_slots[i].key : NULL;
		if (k == RTVAL_HANDLE_EMPTY || k == RTVAL_HANDLE_TOMBSTONE) { continue; }
		free(k);
		free(rtval_handle_slots[i].payload);
	}
	free(rtval_handle_slots);
	rtval_handle_slots = NULL;
	rtval_handle_capacity = 0;
	rtval_handle_count = 0;
	rtval_handle_used = 0;
}

#undef RTVAL_HANDLE_TOMBSTONE
#undef RTVAL_HANDLE_EMPTY
#undef RTVAL_HANDLE_PAYLOAD_SIZE

/*===============================================================================================*/
/* queue wrapper registry: backend rt_queue -> rtval_queue (identity preserved across queries)    */
/*===============================================================================================*/

#define RTVAL_MAX_QUEUES 16

typedef struct rtval_queue_slot {
	rt_queue             backend;
	struct rtval_queue*  handle;
} rtval_queue_slot;

static rtval_queue_slot rtval_queue_slots[RTVAL_MAX_QUEUES];
static u32 rtval_queue_slot_count = 0;

struct rtval_queue* rtval_queue_wrap(rt_queue backend) {
	if (!backend) { return NULL; }
	for (u32 i = 0; i < rtval_queue_slot_count; i++) {
		if (rtval_queue_slots[i].backend == backend) {
			return rtval_queue_slots[i].handle;
		}
	}
	if (rtval_queue_slot_count == RTVAL_MAX_QUEUES) { return NULL; }
	struct rtval_queue* handle = rtval_handle_create(RTVAL_HANDLE_TYPE_QUEUE);
	if (!handle) { return NULL; }
	struct rtval_queue* state = RTVAL_PAYLOAD(handle, struct rtval_queue);
	state->backend = backend;
	rtval_queue_slots[rtval_queue_slot_count++] = (rtval_queue_slot){ backend, handle };
	return handle;
}

void rtval_queue_release_all(void) {
	for (u32 i = 0; i < rtval_queue_slot_count; i++) {
		rtval_handle_destroy(rtval_queue_slots[i].handle);
	}
	rtval_queue_slot_count = 0;
}

rt_timepoint rtval_timepoint_wrap(rt_timepoint backend_tp) {
	rt_timepoint out = backend_tp;
	out.queue = rtval_queue_to_handle(rtval_queue_wrap(backend_tp.queue));
	return out;
}

rt_timepoint rtval_timepoint_unwrap(rt_timepoint public_tp) {
	rt_timepoint out = public_tp;
	struct rtval_queue* state = RTVAL_PAYLOAD(rtval_queue_from_handle(public_tp.queue), struct rtval_queue);
	if (state) {
		out.queue = state->backend;
	}
	return out;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT void rtLayerSetNext(rt_proc_chain next) {
	rtval_next_rtInit = (PFN_rtInit)next.get_proc(&next, "rtInit");
	rtval_next_rtExit = (PFN_rtExit)next.get_proc(&next, "rtExit");
	rtval_next_rtError = (PFN_rtError)next.get_proc(&next, "rtError");
	rtval_next_rtErrorMessage = (PFN_rtErrorMessage)next.get_proc(&next, "rtErrorMessage");
	rtval_next_rtClearError = (PFN_rtClearError)next.get_proc(&next, "rtClearError");
	rtval_next_rtGetName = (PFN_rtGetName)next.get_proc(&next, "rtGetName");
	rtval_next_rtQueryFormatCapabilities = (PFN_rtQueryFormatCapabilities)next.get_proc(&next, "rtQueryFormatCapabilities");
	rtval_next_rtBufferCreate = (PFN_rtBufferCreate)next.get_proc(&next, "rtBufferCreate");
	rtval_next_rtBufferDestroy = (PFN_rtBufferDestroy)next.get_proc(&next, "rtBufferDestroy");
	rtval_next_rtBufferData = (PFN_rtBufferData)next.get_proc(&next, "rtBufferData");
	rtval_next_rtBufferSubdata = (PFN_rtBufferSubdata)next.get_proc(&next, "rtBufferSubdata");
	rtval_next_rtBufferRead = (PFN_rtBufferRead)next.get_proc(&next, "rtBufferRead");
	rtval_next_rtBufferMap = (PFN_rtBufferMap)next.get_proc(&next, "rtBufferMap");
	rtval_next_rtBufferUnmap = (PFN_rtBufferUnmap)next.get_proc(&next, "rtBufferUnmap");
	rtval_next_rtTextureCreate = (PFN_rtTextureCreate)next.get_proc(&next, "rtTextureCreate");
	rtval_next_rtTextureDestroy = (PFN_rtTextureDestroy)next.get_proc(&next, "rtTextureDestroy");
	rtval_next_rtTextureViewCreate = (PFN_rtTextureViewCreate)next.get_proc(&next, "rtTextureViewCreate");
	rtval_next_rtTextureViewDestroy = (PFN_rtTextureViewDestroy)next.get_proc(&next, "rtTextureViewDestroy");
	rtval_next_rtTextureViewFilter = (PFN_rtTextureViewFilter)next.get_proc(&next, "rtTextureViewFilter");
	rtval_next_rtTextureViewAddress = (PFN_rtTextureViewAddress)next.get_proc(&next, "rtTextureViewAddress");
	rtval_next_rtTextureViewAnisotropy = (PFN_rtTextureViewAnisotropy)next.get_proc(&next, "rtTextureViewAnisotropy");
	rtval_next_rtTextureViewLod = (PFN_rtTextureViewLod)next.get_proc(&next, "rtTextureViewLod");
	rtval_next_rtTextureCopy = (PFN_rtTextureCopy)next.get_proc(&next, "rtTextureCopy");
	rtval_next_rtTextureData = (PFN_rtTextureData)next.get_proc(&next, "rtTextureData");
	rtval_next_rtTextureSubcopy = (PFN_rtTextureSubcopy)next.get_proc(&next, "rtTextureSubcopy");
	rtval_next_rtTextureSubdata = (PFN_rtTextureSubdata)next.get_proc(&next, "rtTextureSubdata");
	rtval_next_rtTextureViewCopyToBuffer = (PFN_rtTextureViewCopyToBuffer)next.get_proc(&next, "rtTextureViewCopyToBuffer");
	rtval_next_rtTextureViewExtent = (PFN_rtTextureViewExtent)next.get_proc(&next, "rtTextureViewExtent");
	rtval_next_rtFramebufferCreate = (PFN_rtFramebufferCreate)next.get_proc(&next, "rtFramebufferCreate");
	rtval_next_rtFramebufferDestroy = (PFN_rtFramebufferDestroy)next.get_proc(&next, "rtFramebufferDestroy");
	rtval_next_rtFramebufferColorView = (PFN_rtFramebufferColorView)next.get_proc(&next, "rtFramebufferColorView");
	rtval_next_rtFramebufferSetColorView = (PFN_rtFramebufferSetColorView)next.get_proc(&next, "rtFramebufferSetColorView");
	rtval_next_rtFramebufferDepthView = (PFN_rtFramebufferDepthView)next.get_proc(&next, "rtFramebufferDepthView");
	rtval_next_rtGraphicsProgramCreate = (PFN_rtGraphicsProgramCreate)next.get_proc(&next, "rtGraphicsProgramCreate");
	rtval_next_rtGraphicsProgramDestroy = (PFN_rtGraphicsProgramDestroy)next.get_proc(&next, "rtGraphicsProgramDestroy");
	rtval_next_rtGraphicsProgramVertexLayout = (PFN_rtGraphicsProgramVertexLayout)next.get_proc(&next, "rtGraphicsProgramVertexLayout");
	rtval_next_rtGraphicsProgramVertexShader = (PFN_rtGraphicsProgramVertexShader)next.get_proc(&next, "rtGraphicsProgramVertexShader");
	rtval_next_rtGraphicsProgramFragmentShader = (PFN_rtGraphicsProgramFragmentShader)next.get_proc(&next, "rtGraphicsProgramFragmentShader");
	rtval_next_rtGraphicsProgramRasterState = (PFN_rtGraphicsProgramRasterState)next.get_proc(&next, "rtGraphicsProgramRasterState");
	rtval_next_rtGraphicsProgramBlendState = (PFN_rtGraphicsProgramBlendState)next.get_proc(&next, "rtGraphicsProgramBlendState");
	rtval_next_rtGraphicsProgramLink = (PFN_rtGraphicsProgramLink)next.get_proc(&next, "rtGraphicsProgramLink");
	rtval_next_rtGraphicsProgramUniformLocation = (PFN_rtGraphicsProgramUniformLocation)next.get_proc(&next, "rtGraphicsProgramUniformLocation");
	rtval_next_rtComputeProgramCreate = (PFN_rtComputeProgramCreate)next.get_proc(&next, "rtComputeProgramCreate");
	rtval_next_rtComputeProgramDestroy = (PFN_rtComputeProgramDestroy)next.get_proc(&next, "rtComputeProgramDestroy");
	rtval_next_rtComputeProgramShader = (PFN_rtComputeProgramShader)next.get_proc(&next, "rtComputeProgramShader");
	rtval_next_rtComputeProgramLink = (PFN_rtComputeProgramLink)next.get_proc(&next, "rtComputeProgramLink");
	rtval_next_rtCmdCreate = (PFN_rtCmdCreate)next.get_proc(&next, "rtCmdCreate");
	rtval_next_rtCmdDestroy = (PFN_rtCmdDestroy)next.get_proc(&next, "rtCmdDestroy");
	rtval_next_rtCmdBegin = (PFN_rtCmdBegin)next.get_proc(&next, "rtCmdBegin");
	rtval_next_rtCmdBeginRendering = (PFN_rtCmdBeginRendering)next.get_proc(&next, "rtCmdBeginRendering");
	rtval_next_rtCmdClearColor = (PFN_rtCmdClearColor)next.get_proc(&next, "rtCmdClearColor");
	rtval_next_rtCmdClearDepth = (PFN_rtCmdClearDepth)next.get_proc(&next, "rtCmdClearDepth");
	rtval_next_rtCmdClearStencil = (PFN_rtCmdClearStencil)next.get_proc(&next, "rtCmdClearStencil");
	rtval_next_rtCmdUseGraphicsProgram = (PFN_rtCmdUseGraphicsProgram)next.get_proc(&next, "rtCmdUseGraphicsProgram");
	rtval_next_rtCmdSetScissor = (PFN_rtCmdSetScissor)next.get_proc(&next, "rtCmdSetScissor");
	rtval_next_rtCmdUseComputeProgram = (PFN_rtCmdUseComputeProgram)next.get_proc(&next, "rtCmdUseComputeProgram");
	rtval_next_rtCmdUniformBuffer = (PFN_rtCmdUniformBuffer)next.get_proc(&next, "rtCmdUniformBuffer");
	rtval_next_rtCmdUniformTexture = (PFN_rtCmdUniformTexture)next.get_proc(&next, "rtCmdUniformTexture");
	rtval_next_rtCmdStorageBuffer = (PFN_rtCmdStorageBuffer)next.get_proc(&next, "rtCmdStorageBuffer");
	rtval_next_rtCmdStorageTexture = (PFN_rtCmdStorageTexture)next.get_proc(&next, "rtCmdStorageTexture");
	rtval_next_rtCmdComputeBarrier = (PFN_rtCmdComputeBarrier)next.get_proc(&next, "rtCmdComputeBarrier");
	rtval_next_rtCmdBindVertexBuffer = (PFN_rtCmdBindVertexBuffer)next.get_proc(&next, "rtCmdBindVertexBuffer");
	rtval_next_rtCmdDraw = (PFN_rtCmdDraw)next.get_proc(&next, "rtCmdDraw");
	rtval_next_rtCmdDispatch = (PFN_rtCmdDispatch)next.get_proc(&next, "rtCmdDispatch");
	rtval_next_rtCmdEndRendering = (PFN_rtCmdEndRendering)next.get_proc(&next, "rtCmdEndRendering");
	rtval_next_rtCmdEnd = (PFN_rtCmdEnd)next.get_proc(&next, "rtCmdEnd");
	rtval_next_rtQueueQuery = (PFN_rtQueueQuery)next.get_proc(&next, "rtQueueQuery");
	rtval_next_rtQueueWait = (PFN_rtQueueWait)next.get_proc(&next, "rtQueueWait");
	rtval_next_rtQueueSubmit = (PFN_rtQueueSubmit)next.get_proc(&next, "rtQueueSubmit");
	rtval_next_rtQueueFlush = (PFN_rtQueueFlush)next.get_proc(&next, "rtQueueFlush");
	rtval_next_rtTimepointWait = (PFN_rtTimepointWait)next.get_proc(&next, "rtTimepointWait");
	rtval_next_rtTimepointReached = (PFN_rtTimepointReached)next.get_proc(&next, "rtTimepointReached");
	rtval_next_rtSwapchainCreate = (PFN_rtSwapchainCreate)next.get_proc(&next, "rtSwapchainCreate");
	rtval_next_rtSwapchainDestroy = (PFN_rtSwapchainDestroy)next.get_proc(&next, "rtSwapchainDestroy");
	rtval_next_rtSwapchainResize = (PFN_rtSwapchainResize)next.get_proc(&next, "rtSwapchainResize");
	rtval_next_rtSwapchainAcquire = (PFN_rtSwapchainAcquire)next.get_proc(&next, "rtSwapchainAcquire");
	rtval_next_rtSwapchainPresent = (PFN_rtSwapchainPresent)next.get_proc(&next, "rtSwapchainPresent");
	rtval_next_rtSwapchainBindWindowGLFW = (PFN_rtSwapchainBindWindowGLFW)next.get_proc(&next, "rtSwapchainBindWindowGLFW");
	rtval_next_rtSetOutput = (PFN_rtSetOutput)next.get_proc(&next, "rtSetOutput");
}
