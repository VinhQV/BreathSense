#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "gatt_db.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"

// Thu vien FreeRTOS
#include "FreeRTOS.h"
#include "semphr.h"

// Extern Semaphore tu file app_freertos.c de dung chung
extern SemaphoreHandle_t breath_event_sem;


static int today_coughs = 0;
static int current_day = 1;

static float alpha = 0.2f;
static float alarm_threshold_pct = 0.4f;
static float min_buffer_coughs = 5.0f;

static float baseline = 0.0f;
static bool is_first_day = true;

// Bien luu tru trang thai ngat tu nut bam
typedef enum {
  EVENT_NONE = 0,
  EVENT_COUGH_DETECTED, // Bam Nut 0
  EVENT_END_OF_DAY      // Bam Nut 1
} isr_event_t;

volatile isr_event_t current_isr_event = EVENT_NONE;


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

void app_init(void) { 
  app_init_bt();
}

void app_process_action(void) {}

// =============================================================================
// XU LY NGAT PHAN CUNG 
// ============================================================================= 
void sl_button_on_change(const sl_button_t *handle) {
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
      
      // Gan gia tri su kien de Task xu ly
      if (handle == &sl_button_btn0) {
          current_isr_event = EVENT_COUGH_DETECTED; 
      } else if (handle == &sl_button_btn1) {
          current_isr_event = EVENT_END_OF_DAY;     
      }

      // Kich hoat Semaphore de danh thuc FreeRTOS Task
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      if (breath_event_sem != NULL) {
          xSemaphoreGiveFromISR(breath_event_sem, &xHigherPriorityTaskWoken);
      }
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

// =============================================================================
// HAM THUC THI CHINH CUA FREERTOS TASK 
// =============================================================================
void app_send_breath_data_task_cb(void) {
    isr_event_t evt = current_isr_event;
    current_isr_event = EVENT_NONE;

    char msg[32];
    int len = 0;

    // Detect coughs
    if (evt == EVENT_COUGH_DETECTED) {
        today_coughs++;
    //    printf("[MIC GIA LAP] Phat hien 1 tieng ho! (Tong hom nay: %d)\n", today_coughs);

        if (connection_handle != 0xff && is_subscribed) {
            len = snprintf(msg, sizeof(msg), "Cough #%d", today_coughs);
            // LINH GAC 1
            if (len > 0 && len < 32) {
                sl_bt_gatt_server_send_notification(
                    connection_handle, gattdb_breath_event, (uint8_t)len, (uint8_t *)msg);
            }
        }
    } 
    // Bam Nut 1  -> End Day
    else if (evt == EVENT_END_OF_DAY) {
        printf("\n--- TONG KET NGAY %d ---\n", current_day);
        printf("Tong so tieng ho: %d\n", today_coughs);

        if (is_first_day) {
            baseline = (float)today_coughs;
            is_first_day = false;
            printf("  -> [Khoi tao] Muc nen goc ghim o: %.2f\n", baseline);
        } else {
            float allowed_increase = baseline * alarm_threshold_pct;
            if (allowed_increase < min_buffer_coughs) {
                allowed_increase = min_buffer_coughs;
            }
            float max_allowed = baseline + allowed_increase;

            if ((float)today_coughs > max_allowed) {
    //            printf("  -> [CANH BAO DO] Bat thuong! (Vuot nguong: %.1f)\n", max_allowed);
                
                if (connection_handle != 0xff && is_subscribed) {
                    len = snprintf(msg, sizeof(msg), "Warning #%d", today_coughs);
                    //gui len server
                    if (len > 0 && len < 32) {
                        sl_bt_gatt_server_send_notification(
                            connection_handle, gattdb_breath_event, (uint8_t)len, (uint8_t *)msg);
                    }
                }
            } else {
    //            printf("  -> Trang thai on dinh.\n");
            }

            baseline = (alpha * today_coughs) + ((1.0f - alpha) * baseline);
            printf("  -> Muc nen EWMA moi cho ngay mai: %.2f\n", baseline);
        }

        if (connection_handle != 0xff && is_subscribed) {
            len = snprintf(msg, sizeof(msg), "DayEnd B:%d", (int)baseline);
            // Chong tran bo dem
            if (len > 0 && len < 32) {
                sl_bt_gatt_server_send_notification(
                    connection_handle, gattdb_breath_event, (uint8_t)len, (uint8_t *)msg);
            } else {
         //       printf("[LOI FIRMWARE] snprintf tinh toan sai length, chan khong gui BLE!\n");
            }
        }

        printf("==================================================\n\n");
        current_day++;
        today_coughs = 0; 
    }
}
// =============================================================================
// CAU HINH CAC SU KIEN CUA BLUETOOTH
// =============================================================================
void sl_bt_on_event(sl_bt_msg_t *evt) {
  sl_status_t sc;
  
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id: {
      uint8_t name[] = "BreathSense-Vinh";
      sc = sl_bt_gatt_server_write_attribute_value(gattdb_device_name, 0, sizeof(name) - 1, name);
      app_assert_status(sc);
      
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle, sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      sc = sl_bt_advertiser_set_timing(advertising_set_handle, 160, 160, 0, 0);
      app_assert_status(sc);
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_legacy_advertiser_connectable);
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
      is_subscribed = false; 
      
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle, sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      
      led_show_disconnected();
      break;

    case sl_bt_evt_gatt_server_characteristic_status_id: {
      if (evt->data.evt_gatt_server_characteristic_status.status_flags == sl_bt_gatt_server_client_config) {
        uint16_t flags = evt->data.evt_gatt_server_characteristic_status.client_config_flags;
        if (flags & sl_bt_gatt_server_notification) {
          is_subscribed = true;  
        } else {
          is_subscribed = false; 
        }
      }
      break;
    }
    
    case sl_bt_evt_gatt_server_user_read_request_id: {
      uint16_t att = evt->data.evt_gatt_server_user_read_request.characteristic;
      uint8_t conn = evt->data.evt_gatt_server_user_read_request.connection;
      
      if (att == gattdb_breath_config) {
        sl_bt_gatt_server_send_user_read_response(conn, att, 0, sizeof(cfg), (uint8_t *)&cfg, NULL);
      } else if (att == gattdb_breath_status) {
        sl_bt_gatt_server_send_user_read_response(conn, att, 0, sizeof(sts), (uint8_t *)&sts, NULL);
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
        sl_bt_gatt_server_send_user_write_response(conn, att, SL_STATUS_INVALID_PARAMETER);
      }
      break;
    }
    
    default: break;
  }
}