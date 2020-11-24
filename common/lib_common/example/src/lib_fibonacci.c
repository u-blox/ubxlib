#include "u_lib_internal.h"
#include "lib_fibonacci.h"
#include "u_error_common.h"

typedef struct {
    uLibLibc_t *pLibc;
    int lastRes;
} state_t;

int libFibCalc(void *ctx, int series)
{
    int f0 = 1;
    int f1 = 1;
    int res = 1;
    if (series < 0) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (series < 1) {
        return res;
    }
    while (series--) {
        res = f0 + f1;
        f0 = f1;
        f1 = res;
    }
    if (ctx) {
        ((state_t *)ctx)->lastRes = res;
    }
    return res;
}

const char *libFibHelloWorld(void *ctx)
{
    (void)ctx;
    return "Hello world from libfib\n";
}

int libFibLastRes(void *ctx)
{
    if (ctx == 0) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    return ((state_t *)ctx)->lastRes;
}

// U_LIB_I_OPEN_FUNC is automatically detected as the library initialiser
// prototype u_lib_internal.h:ulibOpenFn_t
int U_LIB_I_OPEN_FUNC(uLibLibc_t *pLibc, uint32_t flags, void **pCtx)
{
    if (pCtx == 0) {
        // cannot happen unless someone tries to load library by hand
        return U_ERROR_COMMON_UNKNOWN;
    }
    if (pLibc == 0 || pLibc->fnmalloc == 0 || pLibc->fnfree == 0) {
        // We need malloc and free for this library, so no play
        return U_ERROR_COMMON_NO_MEMORY;
    }

    // allocate our context
    state_t *state = pLibc->fnmalloc(sizeof(state_t));
    if (!state) {
        return U_ERROR_COMMON_NO_MEMORY;
    }
    *pCtx = state;
    state->pLibc = pLibc;
    state->lastRes = 0;
    return U_ERROR_COMMON_SUCCESS;
}

// U_LIB_I_CLOSE_FUNC is automatically detected as the library finaliser
// prototype u_lib_internal.h:ulibCloseFn_t
void U_LIB_I_CLOSE_FUNC(void *ctx)
{
    if (ctx) {
        // free our context
        ((state_t *)ctx)->pLibc->fnfree(ctx);
    }
}
