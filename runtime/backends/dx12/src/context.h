#ifndef RTDX_CONTEXT_H
#define RTDX_CONTEXT_H

#include "types.h"

#include <d3d12.h>
#include <dxgi1_6.h>

typedef struct rtdx_context_flags {
	unsigned presentation : 1;
} rtdx_context_flags;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_queue;

struct rtdx_context {
	IDXGIFactory6 *dxgi_factory;
	IDXGIAdapter1 *dxgi_adapter;
	ID3D12Device *d3d_device;
	struct rtdx_queue **queues;
	u32 queue_count;
	rtdx_context_flags flags;
	bool allow_tearing;
	bool shutting_down;
};

extern struct rtdx_context *current_context;

struct rtdx_context *rtdx_get_current_context(void);
struct rtdx_context *rtdx_create_context(rtdx_context_flags flags);
void rtdx_context_init(struct rtdx_context *ctx);
void rtdx_context_finish(struct rtdx_context *ctx);
void rtdx_context_destroy(struct rtdx_context *ctx);

template <typename T>
static inline void rtdx_release(T **object) {
	if (*object) {
		(*object)->Release();
		*object = NULL;
	}
}

#endif /* RTDX_CONTEXT_H */
