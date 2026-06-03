#include <stdio.h>
#include <string.h>
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "gatt_db.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"

// Include FreeRTOS to use Semaphore
#include "FreeRTOS.h"
#include "semphr.h"

// Extern the Semaphore handle from app_freertos.c
extern SemaphoreHandle_t breath_event_sem;

// Struct definitions
typedef enum {
  BREATH_INHALE  = 0x01, 
  BREATH_EXHALE  = 0x02,
  BREATH_COUGH   = 0x03, 
  BREATH_UNKNOWN = 0xFF
} breath_event_type_t;

typedef struct __attribute__((packed)) {
  uint8_t mode, threshold, sample_rate, reserved;
} breath_config_t;

typedef struct __attribute__((packed)) {
  uint8_t state, battery_pct;
  uint16_t error_code;
  uint32_t event_count;
} device_status_t;

static uint8_t advertising_set_handle = 0xff;
static uint8_t connection_handle = 0xff;
static uint32_t breath_count = 0;
static breath_config_t cfg = { 1, 128, 16, 0 };
static device_status_t sts = { 1, 95, 0, 0 };
static bool is_subscribed = false;

static inline void led_show_disconnected(void) {
  sl_led_turn_on(&sl_led_led_discn);   
  sl_led_turn_off(&sl_led_led_cn);     
}

static inline void led_show_connected(void) {
  sl_led_turn_off(&sl_led_led_discn);
  sl_led_turn_on(&sl_led_led_cn);
}

// BUTTON INTERRUPT HANDLER (INTERRUPT CONTEXT)
void sl_button_on_change(const sl_button_t *handle) {
  if (handle == &sl_button_bt_01 && sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
      
      // Since this is inside an interrupt, we MUST use FreeRTOS FromISR APIs
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      
      if (breath_event_sem != NULL) {
          xSemaphoreGiveFromISR(breath_event_sem, &xHigherPriorityTaskWoken);
      }
      
      // Force a context switch immediately if the woken task has a higher priority
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

void app_init(void) {}

void app_process_action(void) {}

// this function will be called from a FreeRTOS when button is pressed
void app_send_breath_data_task_cb(void) {
    if (connection_handle != 0xff && is_subscribed) {
      breath_event_type_t type = (breath_event_type_t)((breath_count % 3) + 1);
      breath_count++;
      sts.event_count = breath_count;

      const char *type_str;
      switch (type) {
        case BREATH_INHALE: type_str = "Inhale"; break;
        case BREATH_EXHALE: type_str = "Exhale"; break;
        case BREATH_COUGH:  type_str = "Cough";  break;
        default:            type_str = "Unknown"; break;
      }

      char msg[32];
      int len = snprintf(msg, sizeof(msg), "%s #%lu", type_str, (unsigned long)breath_count);

      sl_bt_gatt_server_send_notification(
          connection_handle, gattdb_breath_event, (uint8_t)len, (uint8_t *)msg);

      sl_bt_gatt_server_send_notification(
          connection_handle, gattdb_breath_status, sizeof(sts), (uint8_t *)&sts);
    }
}

void sl_bt_on_event(sl_bt_msg_t *evt) {
  sl_status_t sc;
  
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id: {
      uint8_t name[] = "BreathSense-Vinh";
      sc = sl_bt_gatt_server_write_attribute_value(
          gattdb_device_name, 0, sizeof(name) - 1, name);
      app_assert_status(sc);
      
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);
      
      sc = sl_bt_legacy_advertiser_generate_data(
          advertising_set_handle, sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      
      sc = sl_bt_advertiser_set_timing(advertising_set_handle, 160, 160, 0, 0);
      app_assert_status(sc);
      
      sc = sl_bt_legacy_advertiser_start(
          advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      
      led_show_disconnected();
      break;
    }
    
    case sl_bt_evt_connection_opened_id:
      connection_handle = evt->data.evt_connection_opened.connection;
      led_show_connected();
      break;
      
    case sl_bt_evt_connection_closed_id:
      connection_handle = 0xff;
      is_subscribed = false; // Reset the subscribe status when connection is lost
      
      sc = sl_bt_legacy_advertiser_generate_data(
          advertising_set_handle, sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      
      sc = sl_bt_legacy_advertiser_start(
          advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      
      led_show_disconnected();
      break;

    case sl_bt_evt_gatt_server_characteristic_status_id: {
      if (evt->data.evt_gatt_server_characteristic_status.status_flags == sl_bt_gatt_server_client_config) {
        uint16_t flags = evt->data.evt_gatt_server_characteristic_status.client_config_flags;
        
        if (flags & sl_bt_gatt_server_notification) {
          is_subscribed = true;  // Client has enabled Notify
        } else {
          is_subscribed = false; // Client has disabled Notify
        }
      }
      break;
    }
    
    case sl_bt_evt_gatt_server_user_read_request_id: {
      uint16_t att = evt->data.evt_gatt_server_user_read_request.characteristic;
      uint8_t conn = evt->data.evt_gatt_server_user_read_request.connection;
      
      if (att == gattdb_breath_config) {
        sl_bt_gatt_server_send_user_read_response(
            conn, att, 0, sizeof(cfg), (uint8_t *)&cfg, NULL);
      } else if (att == gattdb_breath_status) {
        sl_bt_gatt_server_send_user_read_response(
            conn, att, 0, sizeof(sts), (uint8_t *)&sts, NULL);
      } else if (att == gattdb_breath_event) {
        uint8_t snap[5] = {
          BREATH_UNKNOWN,
          (uint8_t)(breath_count), (uint8_t)(breath_count >> 8),
          (uint8_t)(breath_count >> 16), (uint8_t)(breath_count >> 24)
        };
        sl_bt_gatt_server_send_user_read_response(
            conn, att, 0, sizeof(snap), snap, NULL);
      }
      break;
    }
    
    case sl_bt_evt_gatt_server_user_write_request_id: {
      uint16_t att = evt->data.evt_gatt_server_user_write_request.characteristic;
      uint8_t conn = evt->data.evt_gatt_server_user_write_request.connection;
      uint16_t len = evt->data.evt_gatt_server_user_write_request.value.len;
      uint8_t *data = evt->data.evt_gatt_server_user_write_request.value.data;
      
      if (att == gattdb_breath_config && len >= sizeof(cfg)) {
        memcpy(&cfg, data, sizeof(cfg));
        sl_bt_gatt_server_send_user_write_response(conn, att, SL_STATUS_OK);
      } else {
        sl_bt_gatt_server_send_user_write_response(
            conn, att, SL_STATUS_INVALID_PARAMETER);
      }
      break;
    }
    
    default: break;
  }
}