#ifndef __STEP_API_H__
#define __STEP_API_H__

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include "step_common.h"

#ifndef STEP_ARG
#define STEP_ARG int32_t   // INTEGER*4
#endif

#define STEP_API(name) name##_

extern void STEP_API(step_init_fortran_order)(void);
extern void STEP_INIT_FORTRAN_ORDER(void);
extern void STEP_API(step_init_c_order)(void);
extern void STEP_INIT_C_ORDER(void);
extern void STEP_API(step_finalize)(void);
extern void STEP_FINALIZE(void);

extern void STEP_API(step_get_commsize)(STEP_ARG *size);
#define STEP_GET_COMMSIZE STEP_API(step_get_commsize)
extern void STEP_API(step_get_rank)(STEP_ARG *rank);
#define STEP_GET_RANK STEP_API(step_get_rank)

extern void STEP_API(step_construct_begin)(const STEP_ARG *construction);
extern void STEP_CONSTRUCT_BEGIN(const STEP_ARG construction);
extern void STEP_API(step_construct_end)(const STEP_ARG *construction);
extern void STEP_CONSTRUCT_END(const STEP_ARG construction);

extern void STEP_API(step_critical_autorisation)(const STEP_ARG *construction);
extern void STEP_API(step_critical_liberation)(const STEP_ARG *construction);

extern void STEP_API(step_compute_loopslices)(STEP_ARG *begin, STEP_ARG *end, STEP_ARG *incr, STEP_ARG *nb_workchunk);
extern void STEP_COMPUTE_LOOPSLICES(STEP_ARG begin, STEP_ARG end, STEP_ARG incr, STEP_ARG nb_workchunk);
extern void STEP_API(step_get_loopbounds)(STEP_ARG *id_workchunk, STEP_ARG *begin, STEP_ARG *end);
extern void STEP_GET_LOOPBOUNDS(STEP_ARG id_workchunk, STEP_ARG *begin, STEP_ARG *end);

extern void STEP_API(step_init_arrayregions)(void *array, STEP_ARG *type, STEP_ARG *dim, ...);
extern void STEP_INIT_ARRAYREGIONS(void *array, STEP_ARG type, STEP_ARG dim, ...);
extern void STEP_API(step_set_sendregions)(void *array, STEP_ARG *nb_workchunk, STEP_ARG *regions);
extern void STEP_SET_SENDREGIONS(void *array, STEP_ARG nb_workchunk, STEP_ARG *regions);
extern void STEP_API(step_set_interlaced_sendregions)(void *array, STEP_ARG *nb_workchunk, STEP_ARG *regions);
extern void STEP_API(step_set_reduction_sendregions)(void *array, STEP_ARG *nb_workchunk, STEP_ARG *regions);
extern void STEP_API(step_set_recvregions)(void *array, STEP_ARG *nb_workchunk, STEP_ARG *regions);
extern void STEP_SET_RECVREGIONS(void *array, STEP_ARG nb_workchunk, STEP_ARG *regions);

extern void STEP_API(step_register_alltoall_partial)(void *userArray, STEP_ARG *algorithm, STEP_ARG *tag);
extern void STEP_REGISTER_ALLTOALL_PARTIAL(void *userArray, STEP_ARG algorithm, STEP_ARG tag);
extern void STEP_API(step_alltoall_full)(void *array, STEP_ARG *algorithm, STEP_ARG *tag);
extern void STEP_ALLTOALL_FULL(void *array, STEP_ARG algorithm, STEP_ARG tag);
extern void STEP_API(step_alltoall_full_interlaced)(void *array, STEP_ARG *algorithm, STEP_ARG *tag);
extern void STEP_API(step_alltoall_partial)(void *array, STEP_ARG *algorithm, STEP_ARG *tag);
extern void STEP_API(step_alltoall_partial_interlaced)(void *array, STEP_ARG *algorithm, STEP_ARG *tag);
extern void STEP_API(step_alltoall)(STEP_ARG *algorithm, STEP_ARG *tag);
extern void STEP_ALLTOALL(STEP_ARG algorithm, STEP_ARG tag);


extern void STEP_API(step_mastertoallscalar)(void *scalar, STEP_ARG *algorithm, STEP_ARG *type);
extern void STEP_API(step_mastertoallregion)(void *array, STEP_ARG *algorithm);
extern void STEP_API(step_alltomasterregion)(void *array, STEP_ARG *algorithm);

extern void STEP_API(step_critical_get_nextprocess)();
extern void STEP_API(step_critical_request)(const STEP_ARG *num_current_critical);
extern void STEP_API(step_critical_release)();
//extern void STEP_API(step_critical_spawn)();
extern void STEP_API(step_critical_check_n_threads) (STEP_ARG * STEP_I_UP, STEP_ARG * STEP_I_LOW);

extern void STEP_API(step_critical_set_currentuptodatescalar)(void *scalar, STEP_ARG *type);
extern void STEP_API(step_critical_set_currentuptodateregion)(void *array);

extern void STEP_API(step_critical_get_currentuptodatescalar)(void *scalar, STEP_ARG *type);
extern void STEP_API(step_critical_get_currentuptodateregion)(void *array);

extern void STEP_API(step_critical_finaluptodatescalar)(void *scalar, STEP_ARG *type);
extern void STEP_API(step_critical_finaluptodateregion)(void *array);


extern void STEP_API(step_flush)(void);
#define STEP_FLUSH STEP_API(step_flush)
extern void STEP_API(step_waitall)(void);
#define STEP_WAITALL STEP_API(step_waitall)
extern void STEP_API(step_barrier)(void);

extern void STEP_API(step_initreduction)(void *variable, STEP_ARG *op, STEP_ARG *type);
extern void STEP_API(step_reduction)(void *variable);


extern void STEP_API(step_timer_init)(size_t *timer_);
extern void STEP_API(step_timer_event)(size_t *timer_);
extern void STEP_API(step_timer_dump)(size_t *timer_, char *filename, STEP_ARG *id_file, STEP_ARG filename_len);
extern void STEP_API(step_timer_reset)(size_t *timer_);
extern void STEP_API(step_timer_finalize)(size_t *timer_);

#endif //__STEP_API_H__
