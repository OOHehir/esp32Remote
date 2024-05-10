/*
 * Copyright (c) 2024 Owen O'Hehir
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define SLEEP_TIME_MS 250

#define LED0_NODE DT_ALIAS(led0)
#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

#ifndef CONFIG_SOC_SERIES_ESP32S3
#error "Unsupported SoC series"
#endif

#ifndef CONFIG_PCNT_ESP32
#error "CONFIG_PCNT_ESP32 not set"
#endif

#ifdef CONFIG_SOC_SERIES_ESP32C3
#error "Unsupported SoC series"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

static const struct device *get_pcnt_device(void) {
  const struct device *const dev = DEVICE_DT_GET_ANY(espressif_esp32_pcnt);

  if (dev == NULL) {
    printf("\nError: no device found.\n");
    return NULL;
  }

  if (!device_is_ready(dev)) {
    printf("\nError: Device \"%s\" is not ready; "
           "check the driver initialization logs for errors.\n",
           dev->name);
    return NULL;
  }

  printf("Found device \"%s\", getting sensor data\n", dev->name);

  return dev;
}

void button_pressed(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins) {
  printf("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
}

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

  if (!gpio_is_ready_dt(&button)) {
    printf("Error: button device %s is not ready\n", button.port->name);
    return 0;
  }

  ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
  if (ret != 0) {
    printf("Error %d: failed to configure %s pin %d\n", ret, button.port->name,
           button.pin);
    return 0;
  }

  ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
  if (ret != 0) {
    printf("Error %d: failed to configure interrupt on %s pin %d\n", ret,
           button.port->name, button.pin);
    return 0;
  }

  gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
  gpio_add_callback(button.port, &button_cb_data);
  printf("Set up button at %s pin %d\n", button.port->name, button.pin);

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
