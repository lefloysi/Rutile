#ifndef RTVK_CONTEXT_H
#define RTVK_CONTEXT_H

#include "types.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

typedef struct rtvk_context_flags {
	unsigned presentation : 1;
} rtvk_context_flags;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_queue;

struct rtvk_context {
	VkInstance vk_instance;
	VkDebugUtilsMessengerEXT vk_debug_messenger;
	VkPhysicalDevice vk_physical_device;
	VkDevice vk_device;
	VmaAllocator vma_allocator;
	struct rtvk_queue** queues;
	u32 queue_count;
	rtvk_context_flags flags;
	enum rt_error error_status;
	char error_text[1024];
	PFN_rtOutput output;
	void* output_user_data;
};
extern struct rtvk_context* current_context;

struct rtvk_context* rtvk_get_current_context(void);
struct rtvk_context* rtvk_create_context(rtvk_context_flags flags);
void rtvk_context_init(struct rtvk_context* ctx);
void rtvk_context_finish(struct rtvk_context* ctx);
void rtvk_context_destroy(struct rtvk_context* ctx);


#endif /* RTVK_CONTEXT_H */


