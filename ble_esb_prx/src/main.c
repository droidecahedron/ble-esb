/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/*
MODIFIED SAMPLE TO INCLUDE EXTENSIONS ++
*/

#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <zephyr/logging/log.h>

#include <zephyr/drivers/gpio.h> 

#include "app_bt_lbs.h"

#include "app_esb.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Debug radio activity */
#include <hal/nrf_radio.h>
#include <nrfx_gpiote.h>
#include <nrfx_dppi.h>
#include <helpers/nrfx_gppi.h>

#define RADIO_READY_DEBUG_PIN 43 /** P1.11 **/
static void dbg_pin_cfg(uint32_t pin, uint32_t event_addr)
{
	//wiggle
	nrf_gpio_cfg_output(RADIO_READY_DEBUG_PIN);
	nrf_gpio_pin_clear(RADIO_READY_DEBUG_PIN);
	for(int i=0;i<2;i++) {
	nrf_gpio_pin_set(RADIO_READY_DEBUG_PIN);
	k_msleep(500);
	nrf_gpio_pin_clear(RADIO_READY_DEBUG_PIN);
	k_msleep(500);
	}

    nrfx_gpiote_t gpiote_instance = NRFX_GPIOTE_INSTANCE(20);
    uint8_t ppi_channel;
 
    int ret = nrfx_gpiote_init(&gpiote_instance, 7);
    if(ret != NRFX_SUCCESS && ret != NRFX_ERROR_ALREADY) {
        LOG_ERR("Failed to init gpiote: 0x%x", ret);
    }
 
    nrfx_gpiote_output_config_t gpiote_config = NRFX_GPIOTE_DEFAULT_OUTPUT_CONFIG;
 
    uint8_t gpiote_channel;
    ret = nrfx_gpiote_channel_alloc(&gpiote_instance, &gpiote_channel);
    if(ret != NRFX_SUCCESS && ret != NRFX_ERROR_ALREADY) {
        LOG_ERR("Failed to allocate gpiote channel: 0x%x", ret);
    }
    nrfx_gpiote_task_config_t gpiote_task_config;
    gpiote_task_config.polarity = NRF_GPIOTE_POLARITY_TOGGLE;
    gpiote_task_config.task_ch = gpiote_channel;
    gpiote_task_config.init_val = NRF_GPIOTE_INITIAL_VALUE_LOW;
 
    nrfx_gpiote_output_configure(&gpiote_instance, pin, &gpiote_config, &gpiote_task_config);
    nrfx_gpiote_out_task_enable(&gpiote_instance, pin);
 
    ret = nrfx_gppi_channel_alloc(&ppi_channel);
    if(ret != NRFX_SUCCESS) {
        LOG_ERR("Could not allocate PPI channel: 0x%x", ret);
    }
    uint32_t gpiote_task_addr = nrfx_gpiote_out_task_address_get(&gpiote_instance, pin);
    nrfx_gppi_channel_endpoints_setup(ppi_channel, (uint32_t) (event_addr), gpiote_task_addr);
    nrfx_gppi_channels_enable(1 << ppi_channel);
    LOG_INF("Configured PPI ch %d between 0x%x and 0x%x on GPIOTE ch %d (pin=%d) ", ppi_channel, event_addr, gpiote_task_addr, gpiote_channel, pin);
}

void on_esb_callback(app_esb_event_t *event)
{
	static uint32_t last_counter = 0;
	static uint32_t counter;
	switch(event->evt_type) {
		case APP_ESB_EVT_TX_SUCCESS:
			LOG_INF("ESB TX success");
			break;
		case APP_ESB_EVT_TX_FAIL:
			LOG_INF("ESB TX failed");
			break;
		case APP_ESB_EVT_RX:
			memcpy((uint8_t*)&counter, event->buf, sizeof(counter));
			if(counter != (last_counter + 1)) {
				LOG_WRN("Packet content error! Counter: %i, last counter %i", counter, last_counter);
			}
			LOG_INF("ESB RX: 0x%.2X-0x%.2X-0x%.2X-0x%.2X", event->buf[0], event->buf[1], event->buf[2], event->buf[3]);
			last_counter = counter;
			break;
		default:
			LOG_ERR("Unknown APP ESB event!");
			break;
	}
}

int main(void)
{
	int err;

	LOG_INF("ESB PRX BLE Multiprotocol Example");

	dbg_pin_cfg(RADIO_READY_DEBUG_PIN,
    	nrf_radio_event_address_get(NRF_RADIO, NRF_RADIO_EVENT_READY));
	
	err = app_bt_init();
	if (err) {
		LOG_ERR("app_bt init failed (err %d)", err);
		return err;
	}

	err = app_esb_init(APP_ESB_MODE_PRX, on_esb_callback);
	if (err) {
		LOG_ERR("app_esb init failed (err %d)", err);
		return err;
	}

	while (1) {
		k_sleep(K_MSEC(2000));
	}

	return 0;
}
