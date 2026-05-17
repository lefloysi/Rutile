#ifndef RTVK_ATOMIC_H
#define RTVK_ATOMIC_H

#include "types.h"

#ifdef __cplusplus
#define RTVK_ATOMIC_ALIGNAS(size) alignas(size)
extern "C" {
#else
#define RTVK_ATOMIC_ALIGNAS(size) _Alignas(size)
#endif

#include <stdbool.h>

typedef struct atomic_u32 {
	RTVK_ATOMIC_ALIGNAS(4) unsigned char storage[4];
} atomic_u32;

typedef struct atomic_bool {
	RTVK_ATOMIC_ALIGNAS(1) unsigned char storage[1];
} atomic_bool;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtvk_atomic_bool_init(atomic_bool* value, bool initial);
void rtvk_atomic_bool_finish(atomic_bool* value);
void rtvk_atomic_bool_store(atomic_bool* value, bool next);
bool rtvk_atomic_bool_load(const atomic_bool* value);
void rtvk_atomic_u32_init(atomic_u32* value, u32 initial);
void rtvk_atomic_u32_finish(atomic_u32* value);
void rtvk_atomic_store(atomic_u32* value, u32 next);
u32 rtvk_atomic_load(const atomic_u32* value);
u32 rtvk_atomic_inc(atomic_u32* value);
u32 rtvk_atomic_dec(atomic_u32* value);

#ifdef __cplusplus
}
#endif

#undef RTVK_ATOMIC_ALIGNAS

#endif


