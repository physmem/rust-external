#pragma once
//#define USE_VMP

#ifdef USE_VMP
#include <VMProtectSDK.h>
#define VMP_BEGIN(x) VMProtectBegin(x)
#define VMP_BEGIN_VIRTUALIZATION(x) VMProtectBeginVirtualization(x)
#define VMP_BEGIN_MUTATION(x) VMProtectBeginMutation(x)
#define VMP_BEGIN_ULTRA(x) VMProtectBeginUltra(x)
#define VMP_END VMProtectEnd()

#define VMP_START(x) VMP_BEGIN_VIRTUALIZATION(x)
#define VMP_MUTATE VMP_BEGIN_MUTATION("mutation")
#else
#define VMP_BEGIN(x)
#define VMP_BEGIN_VIRTUALIZATION(x)
#define VMP_BEGIN_MUTATION(x)
#define VMP_BEGIN_ULTRA(x)
#define VMP_END

#define VMP_START(x)
#define VMP_MUTATE
#endif
