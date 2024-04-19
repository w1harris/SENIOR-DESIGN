## Description

Program to read heart rate from AD8232 based on QRS intervals. Program outputs calculated ADC value every 10 ms and when a heart beat is detected it is also printed to the console. Can also be used to collect data with #define COLLECT_DATA. For data collection, adc values are output to the terminal and can be saved to putty log file.

## Software

### Project-Specific Build Notes

ADC is configured to use internal 1.22V reference. AIN3 is connected through a resistor divider to get ECG output within ADC range and heart beat detection is whenever voltage goes below a certain threshold.

## Required Connections
-   Open a terminal application on the PC and connect to the EV kit's console UART at 115200, 8-N-1.
-   Connect a 3kohm and a 2kohm resistor in series with ground, place a jumper between them and connect it to AIN3.

## Expected Output

The Console UART of the device will output these messages:

```
******************** ADC Example ********************

ADC readings are taken on ADC channel 3 every 10ms
and are subsequently printed to the terminal.

500
342
634
1 beat detected
150
345
.
.
.
```
