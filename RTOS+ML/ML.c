#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "mxc.h"
#include "cnn.h"
#include "sampledata.h"
#include "sampleoutput.h"
#include "ML.h"
#include <math.h>

/* Enable/Disable Features */
//#define ENABLE_PRINT_ENVELOPE // enables printing average waveform envelope for samples
#define ENABLE_CLASSIFICATION_DISPLAY  // enables printing classification result
#define ENABLE_SILENCE_DETECTION // Starts collecting only after avg > THRESHOLD_HIGH, otherwise starts from first sample
#undef EIGHT_BIT_SAMPLES // samples from Mic or Test vectors are eight bit, otherwise 16-bit
#define ENABLE_MIC_PROCESSING 1 // enables capturing Mic, otherwise a header file Test vector is used as sample data

//ML model declarations/defines
#define SLEEP_MODE 0 // 0: no sleep,  1: sleep,   2:deepsleep(LPM)
#define WUT_ENABLE 1 // enables WUT timer
#define WUT_USEC 380 // continuous WUT duration close to I2S polling time in usec


#define SAMPLE_SIZE 16384 // size of input vector for CNN, keep it multiple of 128
#define CHUNK \
    128 // number of data points to read at a time and average for threshold, keep multiple of 128
#define TRANSPOSE_WIDTH 128 // width of 2d data model to be used for transpose
#define NUM_OUTPUTS 2 // number of classes, either no cough or cough
#define I2S_RX_BUFFER_SIZE 64 // I2S buffer size

/* Adjustable Parameters */
#ifdef ENABLE_MIC_PROCESSING
#define SAMPLE_SCALE_FACTOR \
    4 // multiplies 16-bit samples by this scale factor before converting to 8-bit
#define THRESHOLD_HIGH 350 // voice detection threshold to find beginning of a keyword
#define THRESHOLD_LOW 100 // voice detection threshold to find end of a keyword
#define SILENCE_COUNTER_THRESHOLD \
    20 // [>20] number of back to back CHUNK periods with avg < THRESHOLD_LOW to declare the end of a word
#define PREAMBLE_SIZE 30 * CHUNK // how many samples before beginning of a keyword to include
#define INFERENCE_THRESHOLD 60 // min probability (0-100) to accept an inference
#else
#define SAMPLE_SCALE_FACTOR \
    1 // multiplies 16-bit samples by this scale factor before converting to 8-bit
#define THRESHOLD_HIGH 130 // voice detection threshold to find beginning of a keyword
#define THRESHOLD_LOW 70 // voice detection threshold to find end of a keyword
#define SILENCE_COUNTER_THRESHOLD \
    20 // [>20] number of back to back CHUNK periods with avg < THRESHOLD_LOW to declare the end of a word
#define PREAMBLE_SIZE 30 * CHUNK // how many samples before beginning of a keyword to include
#define INFERENCE_THRESHOLD 49 // min probability (0-100) to accept an inference
#endif

#if defined(ENABLE_CODEC_MIC)
#define PMIC_AUDIO_I2C MXC_I2C1
#define CODEC_MCLOCK 12288000
#endif

volatile uint32_t cnn_time; // Stopwatch
volatile uint32_t fileCount = 0;

int8_t micBuff[SAMPLE_SIZE];
int micBufIndex = 0;

int utteranceIndex = 0;
uint16_t utteranceAvg = 0;
int zeroPad = 0;

static int32_t ml_data[NUM_OUTPUTS];
static q15_t ml_softmax[NUM_OUTPUTS];
uint8_t pAI85Buffer[SAMPLE_SIZE];

int16_t Max, Min;
uint16_t thresholdHigh = THRESHOLD_HIGH;
uint16_t thresholdLow = THRESHOLD_LOW;

volatile uint8_t i2s_flag = 0;
int32_t i2s_rx_buffer[I2S_RX_BUFFER_SIZE];


/* **** Constants **** */
typedef enum _mic_processing_state {
    STOP = 0, /* No processing  */
    SILENCE = 1, /* Threshold not detected yet  */
    KEYWORD = 2 /* Threshold has been detected, gathering keyword samples */
} mic_processing_state;

/* Set of detected words */
const char keywords[NUM_OUTPUTS][10] = { "NO_COUGH", "COUGH"};

uint32_t sampleCounter = 0;
    mxc_tmr_unit_t units;

    uint8_t pChunkBuff[CHUNK];

    uint16_t avg = 0;
    uint16_t ai85Counter = 0;
    uint16_t wordCounter = 0;

    uint16_t avgSilenceCounter = 0;

    mic_processing_state procState = STOP;

void fail(void);
uint8_t cnn_load_data(uint8_t *pIn);
uint8_t MicReadChunk(uint16_t *avg);
uint8_t AddTranspose(uint8_t *pIn, uint8_t *pOut, uint16_t inSize, uint16_t outSize,
                     uint16_t width);
uint8_t check_inference(q15_t *ml_soft, int32_t *ml_data, int16_t *out_class, double *out_prob);
void I2SInit();
void HPF_init(void);
int16_t HPF(int16_t input);

void fail(void)
{
  printf("\n*** FAIL ***\n\n");
  while (1);
}

void i2s_isr(void)
{
    i2s_flag = 1;
    /* Clear I2S interrupt flag */
    MXC_I2S_ClearFlags(MXC_F_I2S_INTFL_RX_THD_CH0);
}

/* **************************************************************************** */

uint8_t runModel(){//Function to run CNN
    uint8_t enoughSamples = 0;
  /* Read samples */
    #ifndef ENABLE_MIC_PROCESSING

        /* end of test vectors */
        if (sampleCounter >= sizeof(voiceVector) / sizeof(voiceVector[0])) {
            PR_DEBUG("End of test Vector\n");
            break;
        }

    #endif

    /* Read from Mic driver to get CHUNK worth of samples, otherwise next sample*/
        if (MicReadChunk(&avg) == 0) {
    #ifdef WUT_ENABLE
    #ifdef ENERGY

            // keep LED on for about 10sec for energy measurement
            if (tot_usec > 10 * 1000 * 1000) {
                LED_Off(LED1);
                tot_usec = -10000000; // wait for 10sec before measuring again
            } else if (tot_usec > 0) {
                LED_On(LED1);
            }

    #endif
    #endif

    #if SLEEP_MODE == 1
            __WFI();
    #elif SLEEP_MODE == 2
    #ifdef WUT_ENABLE
            MXC_LP_ClearWakeStatus();
            SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; // SLEEPDEEP=1
            __WFI();
    #endif
    #endif // #if SLEEP_MODE == 1
            return enoughSamples;
        }

        sampleCounter += CHUNK;

        /* wait for at least PREAMBLE_SIZE samples before detecting the utterance */
        if (sampleCounter < PREAMBLE_SIZE) return enoughSamples;

#ifdef ENABLE_SILENCE_DETECTION // disable to start collecting data immediately.

            /* Display average envelope as a bar */
#ifdef ENABLE_PRINT_ENVELOPE
        PR_DEBUG("%.6d|", sampleCounter);

        for (int i = 0; i < avg / 10; i++) {
            PR_DEBUG("=");
        }

        if (avg >= thresholdHigh) {
            PR_DEBUG("*");
        }

        PR_DEBUG("[%d]\n", avg);
#endif

        /* if we have not detected voice, check the average*/
        if (procState == SILENCE) {
            /* compute average, proceed if greater than threshold */
            if (avg >= thresholdHigh) {
                /* switch to keyword data collection*/
                procState = KEYWORD;

                /* record the average and index of the begining of the word */
                utteranceAvg = avg;
                utteranceIndex = micBufIndex;

                ai85Counter += PREAMBLE_SIZE;
                return enoughSamples;
            }
        }
        /* if it is in data collection, add samples to buffer*/
        else if (procState == KEYWORD)
#endif //#ifdef ENABLE_SILENCE_DETECTION
        {
            
            uint8_t ret = 0;

            /* increment number of stored samples */
            ai85Counter += CHUNK;

            /* if there is silence after at least 1/3 of samples passed, increment number of times back to back silence to find end of keyword */
            if ((avg < thresholdLow) && (ai85Counter >= SAMPLE_SIZE / 3)) {
                avgSilenceCounter++;
            } else {
                avgSilenceCounter = 0;
            }

            /* if this is the last sample and there are not enough samples to
             * feed to CNN, or if it is long silence after keyword,  append with zero (for reading file)
             */
            if (avgSilenceCounter > SILENCE_COUNTER_THRESHOLD)
            {
                memset(pChunkBuff, 0, CHUNK);

                zeroPad = SAMPLE_SIZE - ai85Counter;
                ai85Counter = SAMPLE_SIZE;
            }

            /* if enough samples are collected, start CNN */
            if (ai85Counter >= SAMPLE_SIZE) {
                int16_t out_class = -1;
                double probability = 0;

                /* end of the utterance */
                int endIndex =
                    (utteranceIndex + SAMPLE_SIZE - PREAMBLE_SIZE - zeroPad) % SAMPLE_SIZE;

                PR_DEBUG("Word starts from index %d to %d, padded with %d zeros, avg:%d > %d \n",
                         utteranceIndex, endIndex, zeroPad, utteranceAvg, thresholdHigh);

                // zero padding
                memset(pChunkBuff, 0, CHUNK);

                /* PREAMBLE copy  */
                if (utteranceIndex - PREAMBLE_SIZE >= 0) {
                    if (AddTranspose((uint8_t *)&micBuff[utteranceIndex - PREAMBLE_SIZE],
                                     pAI85Buffer, PREAMBLE_SIZE, SAMPLE_SIZE, TRANSPOSE_WIDTH)) {
                        PR_DEBUG("ERROR: Transpose ended early \n");
                    }
                } else {
                    /* copy oldest samples to the beginning*/
                    if (AddTranspose(
                            (uint8_t *)&micBuff[SAMPLE_SIZE - PREAMBLE_SIZE + utteranceIndex],
                            pAI85Buffer, PREAMBLE_SIZE - utteranceIndex, SAMPLE_SIZE,
                            TRANSPOSE_WIDTH)) {
                        PR_DEBUG("ERROR: Transpose ended early \n");
                    }

                    /* copy latest samples afterwards */
                    if (AddTranspose((uint8_t *)&micBuff[0], pAI85Buffer, utteranceIndex,
                                     SAMPLE_SIZE, TRANSPOSE_WIDTH)) {
                        PR_DEBUG("ERROR: Transpose ended early \n");
                    }
                }

                /* Utterance copy */
                if (utteranceIndex < endIndex) {
                    /* copy from utternace to the end */
                    if (AddTranspose((uint8_t *)&micBuff[utteranceIndex], pAI85Buffer,
                                     endIndex - utteranceIndex, SAMPLE_SIZE, TRANSPOSE_WIDTH)) {
                        PR_DEBUG("ERROR: Transpose ended early \n");
                    }
                    // copy zero padding
                    while (!ret) {
                        ret = AddTranspose(pChunkBuff, pAI85Buffer, CHUNK, SAMPLE_SIZE,
                                           TRANSPOSE_WIDTH);
                    }
                } else {
                    /* copy from utternace to the end*/
                    if (AddTranspose((uint8_t *)&micBuff[utteranceIndex], pAI85Buffer,
                                     SAMPLE_SIZE - utteranceIndex, SAMPLE_SIZE, TRANSPOSE_WIDTH)) {
                        PR_DEBUG("ERROR: Transpose ended early \n");
                    }

                    /* copy from begining*/
                    if (AddTranspose((uint8_t *)&micBuff[0], pAI85Buffer, endIndex, SAMPLE_SIZE,
                                     TRANSPOSE_WIDTH)) {
                        PR_DEBUG("ERROR: Transpose ended early \n");
                    }
                    // copy zero padding
                    while (!ret) {
                        ret = AddTranspose(pChunkBuff, pAI85Buffer, CHUNK, SAMPLE_SIZE,
                                           TRANSPOSE_WIDTH);
                    }
                }

                /* reset counters */
                ai85Counter = 0;
                avgSilenceCounter = 0;

                /* new word */
                wordCounter++;

                /* change state to silence */
                procState = SILENCE;

                /* sanity check, last transpose should have returned 1, as enough samples should have already been added */
                if (ret != 1) {
                    PR_DEBUG("ERROR: Transpose incomplete!\n");
                    fail();
                }

                //----------------------------------  : invoke AI85 CNN
                PR_DEBUG("%.6d: Starts CNN: %d\n", sampleCounter, wordCounter);
                /* enable CNN clock */
                MXC_SYS_ClockEnable(MXC_SYS_PERIPH_CLOCK_CNN);

                /* load to CNN */
                if (!cnn_load_data(pAI85Buffer)) {
                    PR_DEBUG("ERROR: Loading data to CNN! \n");
                    fail();
                }

                /* Start CNN */
                if (!cnn_start()) {
                    PR_DEBUG("ERROR: Starting CNN! \n");
                    fail();
                }

#if SLEEP_MODE == 0

                /* Wait for CNN  to complete */
                while (cnn_time == 0) {
                    __WFI();
                }

#elif SLEEP_MODE == 1

                while (cnn_time == 0) {
                    __WFI();
                }

#elif SLEEP_MODE == 2
                SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; // SLEEPDEEP=1

                while (cnn_time == 0) {
#ifdef WUT_ENABLE
                    MXC_LP_ClearWakeStatus();
                    __WFI();
#endif
                }

#endif // #if SLEEP_MODE==0

                /* Read CNN result */
                cnn_unload((uint32_t *)ml_data);
                /* Stop CNN */
                cnn_stop();
                /* Disable CNN clock to save power */
                MXC_SYS_ClockDisable(MXC_SYS_PERIPH_CLOCK_CNN);
                /* Get time */
                MXC_TMR_GetTime(MXC_TMR0, cnn_time, (void *)&cnn_time, &units);
                PR_DEBUG("%.6d: Completes CNN: %d\n", sampleCounter, wordCounter);

                switch (units) {
                case TMR_UNIT_NANOSEC:
                    cnn_time /= 1000;
                    break;

                case TMR_UNIT_MILLISEC:
                    cnn_time *= 1000;
                    break;

                case TMR_UNIT_SEC:
                    cnn_time *= 1000000;
                    break;

                default:
                    break;
                }

                PR_DEBUG("CNN Time: %d us\n", cnn_time);

                /* run softmax */
                softmax_q17p14_q15((const q31_t *)ml_data, NUM_OUTPUTS, ml_softmax);

#ifdef ENABLE_CLASSIFICATION_DISPLAY
                PR_DEBUG("\nClassification results:\n");
                
                
                for (int i = 0; i < NUM_OUTPUTS; i++) {
                    int digs = (1000 * ml_softmax[i] + 0x4000) >> 15;
                    int tens = digs % 10;
                    digs = digs / 10;

                    PR_DEBUG("[%+.7d] -> Class %.2d %8s: %d.%d%%\n", ml_data[i], i, keywords[i],
                             digs, tens);
                }

#endif
                /* find detected class with max probability */
                ret = check_inference(ml_softmax, ml_data, &out_class, &probability);

                PR_DEBUG("----------------------------------------- \n");
                /* Treat low confidence detections as unknown*/
                if (!ret || out_class == NUM_OUTPUTS - 1) {
                    PR_DEBUG("Detected word: %s", "Unknown");
                } else {
                    PR_DEBUG("Detected word: %s (%0.1f%%)", keywords[out_class], probability);
                }
                PR_DEBUG("\n----------------------------------------- \n");

                if (!ret || out_class == 21) {
                    PR_DEBUG("Nothing Detected");
                    //send_data("")
                } else {
                    PR_DEBUG("Detected: %s (%0.1f%%)", keywords[out_class], probability);
                    //uart_bluetooth(10 + out_class);
                    /*else if (model_config == 2) {
                        uart_bluetooth(20 + out_class);                       
                    }*/
                }
                PR_DEBUG("\n----------------------------------------- \n");

                Max = 0;
                Min = 0;
                //------------------------------------------------------------

                /* clear the buffer */
                memset(micBuff, 0, SAMPLE_SIZE);
                micBufIndex = 0;
                //sampleCounter = 0;  // comment to start immediately after the last utterance
            }
        }

    /* Turn off LED2 (Red) */
    LED_Off(LED2);
    //PR_DEBUG("Total Samples:%d, Total Words: %d \n", sampleCounter, wordCounter);
    return TRUE;
}

/* **************************************************************************** */

void init_ML(){
  printf("Initializing model...\n");
  /* Enable cache */
  MXC_ICC_Enable(MXC_ICC0);

  #ifdef ENABLE_MIC_PROCESSING
    #if defined(ENABLE_CODEC_MIC)
        int err;
        if ((err = max9867_init(PMIC_AUDIO_I2C, CODEC_MCLOCK, 1)) != E_NO_ERROR) {
            PR_DEBUG("\nError in max9867_init: %d\n", err);
        }
    #elif defined(BOARD_FTHR_REVA)
        /* Enable microphone power on Feather board only if codec is not enabled */
        Microphone_Power(POWER_ON);
    #endif
  #endif

  /* Enable peripheral, enable CNN interrupt, turn on CNN clock */
  /* CNN clock: 50 MHz div 1 */
  cnn_enable(MXC_S_GCR_PCLKDIV_CNNCLKSEL_PCLK, MXC_S_GCR_PCLKDIV_CNNCLKDIV_DIV1);

  /* Configure P2.5, turn on the CNN Boost */
  cnn_boost_enable(MXC_GPIO2, MXC_GPIO_PIN_5);

  memset(pAI85Buffer, 0x0, sizeof(pAI85Buffer));

  /* Bring state machine into consistent state */
    cnn_init();
    /* Load kernels */
    cnn_load_weights();
    /* Configure state machine */
    cnn_configure();

  #if SLEEP_MODE == 1
    NVIC_EnableIRQ(CNN_IRQn);
  #endif

  #ifdef WUT_ENABLE
    // Get ticks based off of microseconds
    mxc_wut_cfg_t cfg;
    uint32_t ticks;

    MXC_WUT_GetTicks(WUT_USEC, MXC_WUT_UNIT_MICROSEC, &ticks);
    // config structure for one shot timer to trigger in a number of ticks
    cfg.mode = MXC_WUT_MODE_CONTINUOUS;
    cfg.cmp_cnt = ticks;
    // Init WUT
    MXC_WUT_Init(MXC_WUT_PRES_1);
    //Config WUT
    MXC_WUT_Config(&cfg);

    MXC_LP_EnableWUTAlarmWakeup();
    NVIC_EnableIRQ(WUT_IRQn);
  #endif

  /* Disable CNN clock  */
    MXC_SYS_ClockDisable(MXC_SYS_PERIPH_CLOCK_CNN);

    /* switch to silence state*/
    procState = SILENCE;

  #ifdef ENABLE_MIC_PROCESSING
    /* initialize I2S interface to Mic */
    I2SInit();
  #endif  

  #ifdef WUT_ENABLE
    MXC_WUT_Enable(); // Start WUT
  #endif

  printf("Model successfully initialized\n");
}

/* **************************************************************************** */

#ifdef ENABLE_MIC_PROCESSING
void I2SInit(){
    mxc_i2s_req_t req;
    int32_t err;

    PR_INFO("\n*** I2S & Mic Init ***\n");
    /* Initialize High Pass Filter */
    HPF_init();
    /* Initialize I2S RX buffer */
    memset(i2s_rx_buffer, 0, sizeof(i2s_rx_buffer));
    /* Configure I2S interface parameters */
    req.wordSize = MXC_I2S_DATASIZE_WORD;
    req.sampleSize = MXC_I2S_SAMPLESIZE_THIRTYTWO;
    req.justify = MXC_I2S_MSB_JUSTIFY;
    req.wsPolarity = MXC_I2S_POL_NORMAL;
    req.channelMode = MXC_I2S_INTERNAL_SCK_WS_0;
    /* Get only left channel data from on-board microphone. Right channel samples are zeros */
    req.stereoMode = MXC_I2S_MONO_LEFT_CH;
    req.bitOrder = MXC_I2S_MSB_FIRST;
    /* I2S clock = PT freq / (2*(req.clkdiv + 1)) */
    /* I2S sample rate = I2S clock/64 = 16kHz */
    req.clkdiv = 5;
    req.rawData = NULL;
    req.txData = NULL;
    req.rxData = i2s_rx_buffer;
    req.length = I2S_RX_BUFFER_SIZE;

    if ((err = MXC_I2S_Init(&req)) != E_NO_ERROR) {
        PR_DEBUG("\nError in I2S_Init: %d\n", err);

        while (1) {}
    }

    /* Set I2S RX FIFO threshold to generate interrupt */
    MXC_I2S_SetRXThreshold(4);

#ifndef WUT_ENABLE
    MXC_NVIC_SetVector(I2S_IRQn, i2s_isr);
    NVIC_EnableIRQ(I2S_IRQn);
    /* Enable RX FIFO Threshold Interrupt */
    MXC_I2S_EnableInt(MXC_F_I2S_INTEN_RX_THD_CH0);
#endif

    MXC_I2S_RXEnable();
    __enable_irq();
}
#endif

/* **************************************************************************** */

uint8_t check_inference(q15_t *ml_soft, int32_t *ml_data, int16_t *out_class, double *out_prob){
    int32_t temp[NUM_OUTPUTS];
    q15_t max = 0; // soft_max output is 0->32767
    int32_t max_ml = 1 << 31; // ml before going to soft_max
    int16_t max_index = -1;

    memcpy(temp, ml_data, sizeof(int32_t) * NUM_OUTPUTS);

    /* find the top 5 classes */
    for (int top = 0; top < 5; top++) {
        /* find the class with highest */
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            if ((int32_t)temp[i] > max_ml) {
                max_ml = (int32_t)temp[i];
                max = ml_soft[i];
                max_index = i;
            }
        }

        /* print top 1 separately */
        if (top == 0) {
            *out_class = max_index;
            *out_prob = 100.0 * max / 32768.0;
            break;
        }

    PR_DEBUG("Min: %d,   Max: %d \n", Min, Max);

    /* check if probability is low */
    if (*out_prob > INFERENCE_THRESHOLD) {
        return 1;
    } else {
        return 0;
    }
  }
}

/* **************************************************************************** */

uint8_t cnn_load_data(uint8_t *pIn){
    uint32_t mem;
    uint16_t index = 0;

    /* data should already be formatted correctly */
    /* pIn is 16KB, each 1KB belongs to a memory group */
    for (mem = 0x50400000; mem <= 0x50418000; mem += 0x8000) {
        memcpy((uint8_t *)mem, &pIn[index], 1024);
        //PR_DEBUG("%.10X \n",(uint8_t *)mem);
        index += 1024;
    }

    for (mem = 0x50800000; mem <= 0x50818000; mem += 0x8000) {
        memcpy((uint8_t *)mem, &pIn[index], 1024);
        index += 1024;
    }

    for (mem = 0x50C00000; mem <= 0x50C18000; mem += 0x8000) {
        memcpy((uint8_t *)mem, &pIn[index], 1024);
        index += 1024;
    }

    for (mem = 0x51000000; mem <= 0x51018000; mem += 0x8000) {
        memcpy((uint8_t *)mem, &pIn[index], 1024);
        index += 1024;
    }

    return CNN_OK;
}

/* **************************************************************************** */

uint8_t AddTranspose(uint8_t *pIn, uint8_t *pOut, uint16_t inSize, uint16_t outSize, uint16_t width){
    /* Data order in Ai85 memory (transpose is included):
    input(series of 8 bit samples): (0,0) ...  (0,127)  (1,0) ... (1,127) ...... (127,0)...(127,127)    16384 samples
    output (32bit word): 16K samples in a buffer. Later, each 1K goes to a separate CNN memory group
    0x0000:
        (0,3)(0,2)(0,1)(0,0)
        (0,67)(0,66)(0,65)(0,64)
        (1,3)(1,2)(1,1)(1,0)
        (1,67)(1,66)(1,65)(1,64)
        ....
        (127,67)(127,66)(127,65)(127,64)
    0x0400:
        (0,7)(0,6)(0,5)(0,4)
        (0,71)(0,70)(0,69)(0,68)
        ....
        (127,71)(127,70)(127,69)(127,68)
    ...
    0x3C00:
        (0,63)(0,62)(0,61)(0,60)
        (0,127)(0,126)(0,125)(0,124)
        ....
        (127,127)(127,126)(127,125)(127,124)
    */

    static uint16_t row = 0, col = 0, total = 0;
    uint16_t secondHalf = 0, wordRow = 0, byteInWord = 0, group = 0, index = 0;

    for (int i = 0; i < inSize; i++) {
        /* is it above 63? */
        if (col >= (width >> 1)) {
            secondHalf = 1; // odd word rows
        } else {
            secondHalf = 0; // even word rows
        }

        /* which group (0-15) it should be */
        group = (col % (width >> 1)) / 4;

        /* which word row (0-255) within the group */
        wordRow = secondHalf + (row << 1);

        /* which byte (0-3) in the word */
        byteInWord = col % 4;

        /* find output index */
        index = 1024 * group + 4 * wordRow + byteInWord;

        /* place sample in correct output location */
        pOut[index] = pIn[i];

        total++;

        /* increment row and col index */
        col++;

        if (col >= width) {
            col = 0;
            row++;
        }
    }

    if (total >= outSize) {
        /* sanity check */
        if (row != width) {
            PR_DEBUG("ERROR: Rearranging!\n");
        }

        total = 0;
        row = 0;
        col = 0;
        return 1;
    } else {
        return 0;
    }
}

/* **************************************************************************** */

#ifndef ENABLE_MIC_PROCESSING
int8_t MicReader(int32_t *sample)
{
    static uint32_t micSampleCount = 0;
    int16_t temp;

    /* reads from Test Vector file and return one sample */
    temp = voiceVector[(micSampleCount++) % KWS20_TEST_VECTOR_SIZE];
    *sample = temp;
    return (1);
}
#endif

/* **************************************************************************** */

uint8_t MicReadChunk(uint16_t *avg){
    static uint16_t chunkCount = 0;
    static uint16_t sum = 0;
    int32_t sample = 0;
    int16_t temp = 0;
    uint32_t rx_size = 0;

#ifndef ENABLE_MIC_PROCESSING
    rx_size = 16;
#else
    static uint32_t index = 0;
    /* sample not ready */
    if (!i2s_flag) {
        *avg = 0;
        return 0;
    }

    /* Clear flag */
    i2s_flag = 0;
    /* Read number of samples in I2S RX FIFO */
    rx_size = MXC_I2S->dmach0 >> MXC_F_I2S_DMACH0_RX_LVL_POS;
    //  PR_DEBUG("%d ", rx_size);
#endif
    /* read until fifo is empty or enough samples are collected */
    while ((rx_size--) && (chunkCount < CHUNK)) {
#ifndef ENABLE_MIC_PROCESSING
        /* Read one sample from mic emulated by test vector and add to buffer*/
        MicReader(&sample);
        temp = sample;
#else
        /* Read microphone sample from I2S FIFO */
        sample = (int32_t)MXC_I2S->fifoch0;

        /* The actual value is 18 MSB of 32-bit word */
#ifdef ENABLE_CODEC_MIC
        temp = sample >> 19; // adjusted for codec
#else
        temp = sample >> 14;
#endif

        /* Remove DC from microphone signal */
        sample = HPF((int16_t)temp); // filter needs about 1K sample to converge

        /* Discard first 10k samples due to microphone charging cap effect */
        if (index++ < 10000) {
            continue;
        }
#endif

#ifndef ENERGY
        /* Turn on LED2 (Red) */
        LED_On(LED2);
#endif

        /* absolute for averaging */
        if (sample >= 0) {
            sum += sample;
        } else {
            sum -= sample;
        }

        /* Convert to 8 bit unsigned */
        micBuff[micBufIndex] = (sample)*SAMPLE_SCALE_FACTOR / 256;
        chunkCount++;

        temp = (int8_t)micBuff[micBufIndex];
        /* record max and min */
        if (temp > Max) {
            Max = temp;
        }

        if (temp < Min) {
            Min = temp;
        }

        micBufIndex = (micBufIndex + 1) % SAMPLE_SIZE;
    }

    /* if not enough samples, return 0 */
    if (chunkCount < CHUNK) {
        *avg = 0;
        return 0;
    }

    /* enough samples are collected, calculate average and return 1 */
    *avg = ((uint16_t)(sum / CHUNK));

    chunkCount = 0;
    sum = 0;
    return 1;
}

/* **************************************************************************** */

static int16_t x_0, x_1, Coeff;
static int32_t y_0, y_1;
void HPF_init(void){
    Coeff = 32604; //0.995
    x_0 = 0;
    y_0 = 0;
    y_1 = y_0;
    x_1 = x_0;
}

/* **************************************************************************** */

int16_t HPF(int16_t input){
    int16_t Acc, output;
    int32_t tmp;

    /* a 1st order IIR high pass filter (100 Hz cutoff frequency)  */
    /* y(n)=x(n)-x(n-1)+A*y(n-1) and A =.995*2^15 */

    x_0 = input;

    tmp = (Coeff * y_1);
    Acc = (int16_t)((tmp + (1 << 14)) >> 15);
    y_0 = x_0 - x_1 + Acc;

    /* Clipping */
    if (y_0 > 32767) {
        y_0 = 32767;
    }

    if (y_0 < -32768) {
        y_0 = -32768;
    }

    /* Update filter state */
    y_1 = y_0;
    x_1 = x_0;

    output = (int16_t)y_0;

    return (output);
}

/* **************************************************************************** */