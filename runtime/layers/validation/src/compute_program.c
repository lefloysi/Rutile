#include "compute_program.h"
#include "logger.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_compute_program rtComputeProgramCreate(void) {
	return rtval_compute_program_to_handle(rtval_compute_program_create());
}

RT_EXPORT void rtComputeProgramDestroy(rt_compute_program program) {
	rtval_compute_program_destroy(rtval_compute_program_from_handle(program));
}

RT_EXPORT void rtComputeProgramShader(rt_compute_program program, u64 size, const void* data) {
	rtval_compute_program_shader(rtval_compute_program_from_handle(program), size, data);
}

RT_EXPORT void rtComputeProgramLink(rt_compute_program program) {
	rtval_compute_program_link(rtval_compute_program_from_handle(program));
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_compute_program* rtval_compute_program_create(void) {
	rt_compute_program backend = rtval_next_rtComputeProgramCreate();
	if (!backend) { rtval_report_error("rtComputeProgramCreate"); return NULL; }
	struct rtval_compute_program* handle = rtval_handle_create(RTVAL_HANDLE_TYPE_COMPUTE_PROGRAM);
	if (!handle) { rtval_next_rtComputeProgramDestroy(backend); return NULL; }
	struct rtval_compute_program* state = RTVAL_PAYLOAD(handle, struct rtval_compute_program);
	state->backend = backend;
	rtval_report_error("rtComputeProgramCreate");
	return handle;
}

void rtval_compute_program_destroy(struct rtval_compute_program* program) {
	if (!program) { return; }
	struct rtval_compute_program* state = RTVAL_PAYLOAD(program, struct rtval_compute_program);
	if (!state) { RTVAL_DROP("rtComputeProgramDestroy: invalid handle"); return; }
	rtval_next_rtComputeProgramDestroy(state->backend);
	rtval_handle_destroy(program);
}

void rtval_compute_program_shader(struct rtval_compute_program* program, u64 size, const void* data) {
	struct rtval_compute_program* state = RTVAL_PAYLOAD(program, struct rtval_compute_program);
	if (!state) { RTVAL_DROP("rtComputeProgramShader: invalid handle"); return; }
	if (!data || size == 0) { RTVAL_DROP("rtComputeProgramShader: empty shader data"); return; }
	rtval_next_rtComputeProgramShader(state->backend, size, data);
	rtval_report_error("rtComputeProgramShader");
}

void rtval_compute_program_link(struct rtval_compute_program* program) {
	struct rtval_compute_program* state = RTVAL_PAYLOAD(program, struct rtval_compute_program);
	if (!state) { RTVAL_DROP("rtComputeProgramLink: invalid handle"); return; }
	rtval_next_rtComputeProgramLink(state->backend);
	rtval_report_error("rtComputeProgramLink");
}

#undef RTVAL_DROP
