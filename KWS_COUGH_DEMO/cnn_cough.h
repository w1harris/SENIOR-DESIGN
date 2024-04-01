/**************************************************************************************************
* Copyright (C) 2019-2021 Maxim Integrated Products, Inc. All Rights Reserved.
*
* Maxim Integrated Products, Inc. Default Copyright Notice:
* https://www.maximintegrated.com/en/aboutus/legal/copyrights.html
**************************************************************************************************/

/*
 * This header file was automatically @generated for the new_cough network from a template.
 * Please do not edit; instead, edit the template and regenerate.
 */

#ifndef __CNN_H__
#define __CNN_H__

#include <stdint.h>
typedef int32_t q31_t;
typedef int16_t q15_t;

/* Return codes */
#define CNN_COUGH_FAIL 0
#define CNN_COUGH_OK 1

/*
  SUMMARY OF OPS
  Hardware: 8,402,528 ops (8,345,344 macc; 54,496 comp; 2,688 add; 0 mul; 0 bitwise)
    Layer 0: 1,651,200 ops (1,638,400 macc; 12,800 comp; 0 add; 0 mul; 0 bitwise)
    Layer 1: 3,640,896 ops (3,628,800 macc; 12,096 comp; 0 add; 0 mul; 0 bitwise)
    Layer 2: 1,177,344 ops (1,161,216 macc; 16,128 comp; 0 add; 0 mul; 0 bitwise)
    Layer 3: 565,104 ops (562,176 macc; 2,928 comp; 0 add; 0 mul; 0 bitwise)
    Layer 4: 281,280 ops (276,480 macc; 4,800 comp; 0 add; 0 mul; 0 bitwise)
    Layer 5: 518,784 ops (516,096 macc; 2,688 comp; 0 add; 0 mul; 0 bitwise)
    Layer 6: 407,288 ops (403,200 macc; 1,400 comp; 2,688 add; 0 mul; 0 bitwise)
    Layer 7: 155,256 ops (153,600 macc; 1,656 comp; 0 add; 0 mul; 0 bitwise)
    Layer 8: 5,376 ops (5,376 macc; 0 comp; 0 add; 0 mul; 0 bitwise)

  RESOURCE USAGE
  Weight memory: 169,472 bytes out of 442,368 bytes total (38.3%)
  Bias memory:   0 bytes out of 2,048 bytes total (0.0%)
*/

/* Number of outputs for this network */
#define CNN_COUGH_NUM_OUTPUTS 21

/* Port pin actions used to signal that processing is active */

#define CNN_COUGH_START LED_On(1)
#define CNN_COUGH_COMPLETE LED_Off(1)
#define SYS_START LED_On(0)
#define SYS_COMPLETE LED_Off(0)

void CNN_COUGH_ISR(void);

/* Enable clocks and power to accelerator, enable interrupt */
int cnn_cough_enable(uint32_t clock_source, uint32_t clock_divider);

/* Disable clocks and power to accelerator */
int cnn_cough_disable(void);

/* Perform minimum accelerator initialization so it can be configured */
int cnn_cough_init(void);

/* Configure accelerator for the given network */
int cnn_cough_configure(void);

/* Load accelerator weights */
int cnn_cough_load_weights(void);

/* Verify accelerator weights (debug only) */
int cnn_cough_verify_weights(void);

/* Load accelerator bias values (if needed) */
int cnn_cough_load_bias(void);

/* Start accelerator processing */
int cnn_cough_start(void);

/* Force stop accelerator */
int cnn_cough_stop(void);

/* Continue accelerator after stop */
int cnn_cough_continue(void);

/* Unload results from accelerator */
int cnn_cough_unload(uint32_t *out_buf);

/* Turn on the boost circuit */
int cnn_cough_boost_enable(mxc_gpio_regs_t *port, uint32_t pin);

/* Turn off the boost circuit */
int cnn_cough_boost_disable(mxc_gpio_regs_t *port, uint32_t pin);

#endif // __CNN_H__
