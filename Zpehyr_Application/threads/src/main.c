/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
// #include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/__assert.h>
#include <string.h>

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7



void blink0(void)
{
	// blink(&led0, 100, 0);
	while (1)
	{
		printk("I am here\n");
	}
}

// void blink1(void)
// {
// 	blink(&led1, 1000, 1);
// }

void uart_out(void)
{
	int cnt = 0;
	while (1) {
		// struct printk_data_t *rx_data = k_fifo_get(&printk_fifo,
		// 					   K_FOREVER);
		++cnt;
		printk("counter=%d\n", cnt);
		// k_free(rx_data);
	}
}

K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL,
		PRIORITY, 0, 0);
// K_THREAD_DEFINE(blink1_id, STACKSIZE, blink1, NULL, NULL, NULL,
// 		PRIORITY, 0, 0);
K_THREAD_DEFINE(uart_out_id, STACKSIZE, uart_out, NULL, NULL, NULL,
		PRIORITY, 0, 0);
