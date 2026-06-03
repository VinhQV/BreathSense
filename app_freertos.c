/***************************************************************************//**
 * @file
 * @brief FreeRTOS helper functions for the application task.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "sl_main_init.h"
#include "app_assert.h"
#include "app.h"

#define BREATH_TASK_NAME       "breath_task"
#define BREATH_TASK_STACK_SIZE 512u
#define BREATH_TASK_PRIO       24u

// Extern the data processing function from app.c
extern void app_send_breath_data_task_cb(void);

static void breath_task(void *p_arg);

static TaskHandle_t      breath_task_handle  = NULL;
SemaphoreHandle_t        breath_event_sem = NULL; 

void app_init_bt(void)
{
  BaseType_t ret;
  
  // Create a Binary Semaphore to signal button press events
  breath_event_sem = xSemaphoreCreateBinary();
  app_assert(breath_event_sem != NULL, "Semaphore creation failed.");

  // Create a dedicated Task to handle breath events
  ret = xTaskCreate(breath_task,
                    BREATH_TASK_NAME,
                    BREATH_TASK_STACK_SIZE,
                    NULL,
                    BREATH_TASK_PRIO,
                    &breath_task_handle);
  app_assert(ret == pdPASS, "Task creation failed.");
}

/******************************************************************************
 * Task Execution Logic
 *****************************************************************************/
static void breath_task(void *p_arg)
{
  (void)p_arg;
  
  while (1) {
    // The task will block here
    // It only wakes up when the button is pressed and the interrupt gives this semaphore
    if (xSemaphoreTake(breath_event_sem, portMAX_DELAY) == pdTRUE) {
        
        // Execute the logic and send BLE data 
        app_send_breath_data_task_cb();
    }
  }
}