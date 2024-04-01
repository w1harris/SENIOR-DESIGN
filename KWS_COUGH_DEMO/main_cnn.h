/* This header file was created for KWS model and cough model integration

The prototypes in this file should be ubiquitous for both KWS and cough detection
model and therefore do not need there own respective header files.

3/3/2024
Bryan Wilson
*/

#include <stdint.h>
typedef int32_t q31_t;
typedef int16_t q15_t;

/* SOFTMAX PROTOTYPES */
/* Run software SoftMax on unloaded data */
void softmax_q17p14_q15(const q31_t * vec_in, const uint16_t dim_vec, q15_t * p_out);
/* Shift the input, then calculate SoftMax */
void softmax_shift_q17p14_q15(q31_t * vec_in, const uint16_t dim_vec, uint8_t in_shift, q15_t * p_out);

/* MEMORY MANAGEMENT */
/* Custom memcopy routines used for weights and data */
void memcpy32(uint32_t *dst, const uint32_t *src, int n);
void memcpy32_const(uint32_t *dst, int n);

/* TIMER MANAGEMENT */
/* Stopwatch - holds the runtime when accelerator finishes */
extern volatile uint32_t cnn_time;