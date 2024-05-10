/*
 * Copyright (c) 2024 Owen O'Hehir
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define SLEEP_TIME_MS 250

#define LED0_NODE DT_ALIAS(led0)
#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

// #define TOUCH1_NODE DT_ALIAS(touch1)
// #if !DT_NODE_HAS_STATUS(TOUCH1_NODE, okay)
// #error "Unsupported board: touch1 devicetree alias is not defined"
// #endif

#ifndef CONFIG_SOC_SERIES_ESP32S3
#error "Unsupported SoC series"
#endif

#ifndef CONFIG_PCNT_ESP32
#error "CONFIG_PCNT_ESP32 not set"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct device *get_pcnt_device(void) {
  const struct device *const dev = DEVICE_DT_GET_ANY(espressif_esp32_pcnt);

  if (dev == NULL) {
    /* No such node, or the node does not have status "okay". */
    printk("\nError: no device found.\n");
    return NULL;
  }

  if (!device_is_ready(dev)) {
    printk("\nError: Device \"%s\" is not ready; "
           "check the driver initialization logs for errors.\n",
           dev->name);
    return NULL;
  }

  printk("Found device \"%s\", getting sensor data\n", dev->name);

  return dev;
}

static void input_cb(struct input_event *evt) {
    printf("Input event: %d\n", evt->type);
}

INPUT_CALLBACK_DEFINE(NULL, input_cb);

int main(void) {
    k_msleep(SLEEP_TIME_MS);

    printf("Startup of board: %s\n", CONFIG_BOARD);

    if (!gpio_is_ready_dt(&led)) {
        printf("GPIO not ready to for LED at %s pin %d\n", led.port->name, led.pin);
        return 0;
    }

    int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printf("Failed to configure LED at %s pin %d\n", led.port->name, led.pin);
        return 0;
    }

    gpio_pin_set_dt(&led, 0);

    const struct device *dev = get_pcnt_device();

    if (dev == NULL) {
        printf("Failed to get PCNT device\n");
        return 0;
    }

    int old_result = 0;

    while (1) {
        struct sensor_value rotation = {0};

        int rc = sensor_sample_fetch(dev);
        if (rc != 0) {
          printf("Failed to fetch sample (%d)\n", rc);
          return 0;
        }

        rc = sensor_channel_get(dev, SENSOR_CHAN_ROTATION, &rotation);
        if (rc != 0) {
          printf("Failed to get data (%d)\n", rc);
          return 0;
        }

        if (rotation.val1 != old_result) {
            gpio_pin_toggle_dt(&led);
            old_result = rotation.val1;
            printf("Rotation: %d\n", rotation.val1);
        }
    }
    return 0;
}
