/*
 * main.c
 */

/* IMPORTS -------------------------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/printk.h>

// Part 1
#include "LED.h"
#include <zephyr/sys/util.h>
#include <ctype.h>

// Part 2
#include "BTN.h"

/* MACROS --------------------------------------------------------------------------------------- */

#define BLE_CUSTOM_SERVICE_UUID \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define BLE_CUSTOM_CHARACTERISTIC_UUID \
  BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

#define BLE_CUSTOM_CHARACTERISTIC_MAX_DATA_LENGTH 20

/* PROTOTYPES ----------------------------------------------------------------------------------- */

static ssize_t ble_custom_service_read(struct bt_conn* conn, const struct bt_gatt_attr* attr,
                                       void* buf, uint16_t len, uint16_t offset);
static ssize_t ble_custom_service_write(struct bt_conn* conn, const struct bt_gatt_attr* attr,
                                        const void* buf, uint16_t len, uint16_t offset,
                                        uint8_t flags);

/* VARIABLES ------------------------------------------------------------------------------------ */

static const struct bt_uuid_128 ble_custom_service_uuid = BT_UUID_INIT_128(BLE_CUSTOM_SERVICE_UUID);
static const struct bt_uuid_128 ble_custom_characteristic_uuid =
    BT_UUID_INIT_128(BLE_CUSTOM_CHARACTERISTIC_UUID);

static const struct bt_data ble_advertising_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BLE_CUSTOM_SERVICE_UUID),
};

static const struct bt_data ble_scan_response_data[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static uint8_t ble_custom_characteristic_user_data[BLE_CUSTOM_CHARACTERISTIC_MAX_DATA_LENGTH + 1] =
    {'E', 'i', 'E'};

static int32_t notify_dir = 1; // +1 = up, -1 = down

/* BLE SERVICE SETUP ---------------------------------------------------------------------------- */

BT_GATT_SERVICE_DEFINE(
    ble_custom_service,  // Name of the struct that will store the config for this service
    BT_GATT_PRIMARY_SERVICE(&ble_custom_service_uuid),  // Setting the service UUID

    // Now to define the characteristic:
    BT_GATT_CHARACTERISTIC(
        &ble_custom_characteristic_uuid.uuid,  // Setting the characteristic UUID
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,  // Possible operations
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,  // Permissions that connecting devices have
        ble_custom_service_read,             // Callback for when this characteristic is read from
        ble_custom_service_write,            // Callback for when this characteristic is written to
        ble_custom_characteristic_user_data  // Initial data stored in this characteristic
        ),
    BT_GATT_CCC(  // Client characteristic configuration for the above custom characteristic
        NULL,     // Callback for when this characteristic is changed
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE  // Permissions that connecting devices have
        ),

    // End of service definition
);

/* FUNCTIONS ------------------------------------------------------------------------------------ */

static ssize_t ble_custom_service_read(struct bt_conn* conn, const struct bt_gatt_attr* attr,
                                       void* buf, uint16_t len, uint16_t offset) {
  // Send the data that is stored in the characteristic ("EiE" by default, can change if written to)
  // by fetching it directly from the characteristic object
  const char* data_to_send_to_connected_device = attr->user_data;

  return bt_gatt_attr_read(conn, attr, buf, len, offset, data_to_send_to_connected_device,
                           strlen(data_to_send_to_connected_device));
}

static ssize_t ble_custom_service_write(struct bt_conn* conn, const struct bt_gatt_attr* attr,
                                        const void* buf, uint16_t len, uint16_t offset,
                                        uint8_t flags) {
  uint8_t* value = attr->user_data;

  if (offset + len > BLE_CUSTOM_CHARACTERISTIC_MAX_DATA_LENGTH) {
    printk("[BLE] ble_custom_service_write: Bad offset %u\n", (unsigned)(offset + len));
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  }
  if (len > 0) {
    memcpy(value + offset, buf, len);

    // Null terminate safely (cap at max)
    uint16_t end = offset + len;
    if (end > BLE_CUSTOM_CHARACTERISTIC_MAX_DATA_LENGTH) {
      end = BLE_CUSTOM_CHARACTERISTIC_MAX_DATA_LENGTH;
    }
    value[end] = 0;
  }
  printk("[BLE] ble_custom_service_write (offset=%u, len=%u, flags=0x%02X)\n", offset, len, flags);
  printk("[BLE] Now stored: '%s'\n", (char *)value);

  // If this is a "prepare write", don't act yet, wait for the execute step
  if (flags & BT_GATT_WRITE_FLAG_PREPARE) {
    return len;
  }
  // Trim trailing whitespace
  size_t n = strlen((char *)value);
  while (n > 0 && isspace((unsigned char)value[n - 1])) {
    value[n - 1] = '\0';
    n--;
  }
  // --- Challenge 1: Interpret commands --- //
  if (strcmp((char *)value, "LED ON") == 0) {
    LED_set(LED1, LED_ON);
    printk("[BLE] Command: LED1 ON\n");
  }
  else if (strcmp((char *)value, "LED OFF") == 0) {
    LED_set(LED1, LED_OFF);
    printk("[BLE] Command: LED1 OFF\n");
  }
  return len;
}

static void ble_custom_service_notify() {
  static int32_t counter = 0;
  bt_gatt_notify(NULL, &ble_custom_service.attrs[2], &counter, sizeof(counter));
  counter += notify_dir;
}

/* MAIN ----------------------------------------------------------------------------------------- */

int main(void) {
  int err = LED_init();
  if (err) {
    printk("LED init failed (err %d)\n", err);
    return 0;
  }
  err = BTN_init();
  if (err) {
    printk("BTN init failed (err %d)\n", err);
    return 0;
  }
  err = bt_enable(NULL);
  if (err) {
    printk("Bluetooth init failed (err %d)\n", err);
    return 0;
  } else {
    printk("Bluetooth initialized!\n");
  }

  err =
      bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ble_advertising_data, ARRAY_SIZE(ble_advertising_data),
                      ble_scan_response_data, ARRAY_SIZE(ble_scan_response_data));
  if (err) {
    printk("Advertising failed to start (err %d)\n", err);
    return 0;
  }

  while (1) {
    k_sleep(K_MSEC(1000));
    if (BTN_check_clear_pressed(BTN1)) {
      notify_dir = -notify_dir;
      printk("[BTN] BTN1 pressed -> notify_dir = %d\n", notify_dir);
    }
    ble_custom_service_notify();
  }
}
