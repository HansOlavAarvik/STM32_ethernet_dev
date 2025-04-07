/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "app_netxduo.h"
#include <stdio.h>
#include "json_handle.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TX_THREAD tx_app_thread;
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */
  CHAR *pointer;

  /* Allocate the stack for tx app thread  */
  if (tx_byte_allocate(byte_pool, (VOID**) &pointer,
                       TX_APP_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  /* Create tx app thread.  */
  if (tx_thread_create(&tx_app_thread, "tx app thread", tx_app_thread_entry, 0, pointer,
                       TX_APP_STACK_SIZE, TX_APP_THREAD_PRIO, TX_APP_THREAD_PREEMPTION_THRESHOLD,
                       TX_APP_THREAD_TIME_SLICE, TX_APP_THREAD_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* USER CODE BEGIN App_ThreadX_Init */
  json_handle_initialize();
  /* USER CODE END App_ThreadX_Init */

  return ret;
}
/**
  * @brief  Function implementing the tx_app_thread_entry thread.
  * @param  thread_input: Hardcoded to 0.
  * @retval None
  */
void tx_app_thread_entry(ULONG thread_input)
{
  /* USER CODE BEGIN tx_app_thread_entry */

// #define VIBRATION_DATA        1
// #define OUTSIDE_TEMP_DATA     3
// #define INSIDE_HUMID_DATA     4
// #define OUTSIDE_HUMID_DATA    5
// #define TOF_DATA              6
  uint16_t in_temp = 24;
  uint16_t in_humid  =  20;
  uint16_t out_temp  =  14;
  uint16_t out_humid  =  15;
  uint16_t tof  =  3;
  UINT switch_flag = 0;
  while(1){
    if(in_temp > 30){
      switch_flag =1;
    }
    if(in_temp < 5){
      switch_flag = 0;
    }
  if (switch_flag == 0){
    in_temp     += 1;
    in_humid    += 1;
    out_temp    += 1;
    out_humid   += 1;
    tof         += 1;
  }
  if(switch_flag == 1){
    in_temp     -= 1;
    in_humid    -= 1;
    out_temp    -= 1;
    out_humid   -= 1;
    tof         -= 1;
  }
  add_sensor_data(in_temp,INSIDE_TEMP_DATA);
  add_sensor_data(out_temp,OUTSIDE_TEMP_DATA);
  add_sensor_data(in_humid,INSIDE_HUMID_DATA);
  add_sensor_data(out_humid,OUTSIDE_HUMID_DATA);
  add_sensor_data(out_humid+2,OUTSIDE_HUMID_DATA);
  add_sensor_data(tof,TOF_DATA);
  tx_thread_sleep(100);
  }
  
  /* USER CODE END tx_app_thread_entry */
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN Before_Kernel_Start */

  /* USER CODE END Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN Kernel_Start_Error */

  /* USER CODE END Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
