#ifndef UART_READ_H
#define UART_READ_H

#define RXD2 4
#define TXD2 5
#define LED_GPIO GPIO_NUM_18
#define GPS_BAUD 9600
#define BUF_SIZE 1024

void uart_task(void *arg);
void uart_task_test(void *arg);
int parse_gpgga(const char *nmea_sentence, float *latitude, float *longitude);

#endif