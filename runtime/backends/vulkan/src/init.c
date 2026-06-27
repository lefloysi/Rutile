#include "core.h"

#include "context.h"
#include "error.h"

#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

/*
** SPEC.html §5.2 Initialization, §5.3 Shutdown
** Implements rtInit and rtExit plus feature-string validation.
** rtExit is a no-op when initialization never succeeded.
*/
