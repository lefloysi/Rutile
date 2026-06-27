#ifndef RTDX_ATOMIC_H
#define RTDX_ATOMIC_H

#include "types.h"

#define RTDX_ATOMIC_ALIGNAS(size) alignas(size)

typedef struct atomic_bool {
	RTDX_ATOMIC_ALIGNAS(1)
	unsigned char storage[1];
} atomic_bool;

typedef struct atomic_u32 {
	RTDX_ATOMIC_ALIGNAS(4)
	unsigned char storage[4];
} atomic_u32;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtdx_atomic_bool_init(atomic_bool* value, bool initial);
void rtdx_atomic_bool_finish(atomic_bool* value);
void rtdx_atomic_bool_store(atomic_bool* value, bool next);
bool rtdx_atomic_bool_load(const atomic_bool* value);
void rtdx_atomic_u32_init(atomic_u32* value, u32 initial);
void rtdx_atomic_u32_finish(atomic_u32* value);
void rtdx_atomic_store(atomic_u32* value, u32 next);
u32 rtdx_atomic_load(const atomic_u32* value);
u32 rtdx_atomic_inc(atomic_u32* value);
u32 rtdx_atomic_dec(atomic_u32* value);

#undef RTDX_ATOMIC_ALIGNAS

#endif /* RTDX_ATOMIC_H */
