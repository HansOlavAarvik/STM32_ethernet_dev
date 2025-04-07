#include "json_handle.h"
#include "main.h"
#include "app_threadx.h"
#include "app_netxduo.h"
#include <stdint.h>
#include "stm32h5xx_hal.h"
#include "stm32h5xx_hal_rtc.h"

//thread defines
#define JSON_HANDLE_PRIORITY 11
#define JSON_HANDLE_STACK_SIZE 1024*8
#define QUEUE_SIZE 100  
#define JSON_TRANSMIT_EVENT 0x01

typedef struct {
  int16_t value;    
  UINT tag;         
} sensor_data_t;

UINT Json_port = JSON_PORT;
ULONG json_handle_thread_stack[JSON_HANDLE_STACK_SIZE / sizeof(ULONG)];

TX_THREAD JsonHandleThread;
static TX_QUEUE sensor_data_queue;
static ULONG sensor_queue_area[QUEUE_SIZE * (sizeof(sensor_data_t)/sizeof(ULONG) + 1)];
static TX_TIMER transmission_timer;

// For communication between timer and JSON thread
static TX_EVENT_FLAGS_GROUP json_events;
// JSON array globally init
static cJSON *vibration_array = NULL;
static cJSON *inside_temp_array = NULL;
static cJSON *outside_temp_array = NULL;
static cJSON *inside_humidity_array = NULL;
static cJSON *outside_humidity_array = NULL;
static cJSON *tof_array = NULL;
static cJSON *timestamp_array = NULL;

//realtime clock handle
extern RTC_HandleTypeDef hrtc;

// Prototypes
static VOID json_handle_thread_entry(ULONG thread_input);
void data_to_json(int16_t data, UINT tag, cJSON* constr);
void data_to_json_number(int16_t number, const char* string, cJSON* contstru);
void free_json_string(char* json_string);
extern UINT UDP_Send(void* data_ptr, UINT data_size, ULONG destination_ip, UINT destination_port);
UINT add_sensor_data(int16_t data, UINT tag);
static void transmission_timer_callback(ULONG timer_input);
void create_arrays(cJSON* root);
char* get_timestamp(void);

UINT json_handle_initialize(void)
{
    UINT ret = NX_SUCCESS;
    ret = tx_queue_create(&sensor_data_queue, "Sensor Data Queue", 
                          sizeof(sensor_data_t)/sizeof(ULONG) + 1,
                          sensor_queue_area,                        
                          sizeof(sensor_queue_area));               

    if (ret != TX_SUCCESS)
    {
    return TX_QUEUE_ERROR;
    }
    ret = tx_event_flags_create(&json_events, "JSON Events");
      if (ret != TX_SUCCESS)
      {
          return; //TX_EVENT_FLAGS_ERROR;
      }
    ret = tx_timer_create(&transmission_timer, "Transmission Timer", 
      transmission_timer_callback, 0, 
      300, 300,  
      TX_AUTO_ACTIVATE);
    if (ret != TX_SUCCESS)
    {
      return TX_TIMER_ERROR;
    }
    ret = tx_thread_create(&JsonHandleThread, "Json thread",
                          json_handle_thread_entry , 0,
                          json_handle_thread_stack, JSON_HANDLE_STACK_SIZE,
                          JSON_HANDLE_PRIORITY, JSON_HANDLE_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);
    if (ret != TX_SUCCESS)
    {
      return TX_THREAD_ERROR;
    }
    RTC_HandleTypeDef hrtc;
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    
    // Configure RTC (usually in main initialization)
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    HAL_RTC_Init(&hrtc);

    return ret;
}

void json_handle_thread_entry(ULONG thread_input)
{
    UNUSED(thread_input);
    ULONG actual_events;
    sensor_data_t sensor_data;
    char* json_data = NULL;

    while(1) {
        tx_event_flags_get(&json_events, JSON_TRANSMIT_EVENT, 
                          TX_OR_CLEAR, &actual_events, TX_WAIT_FOREVER);

        cJSON *root = cJSON_CreateObject();
        if (root == NULL) {
            continue;
        }
        create_arrays(root);
        UINT data_count = 0;

        while (tx_queue_receive(&sensor_data_queue, &sensor_data, TX_NO_WAIT) == TX_SUCCESS) {
            data_to_json(sensor_data.value, sensor_data.tag, root);
            data_count++;
        }
        char* timestamp = get_timestamp();
        data_to_json(timestamp,TIMESTAMP_DATA, root);
        if (data_count > 0) {
            json_data = cJSON_Print(root);
            if (json_data != NULL) {
                UINT json_size = strlen(json_data);
                UDP_Send(json_data, json_size, DESTINATION_IP, Json_port);
                free_json_string(json_data);
            }
        }
        cJSON_Delete(root);
        
    }
}
UINT add_sensor_data(int16_t data, UINT tag)
{
    sensor_data_t sensor_data;
    sensor_data.value = data;
    sensor_data.tag = tag;
    return tx_queue_send(&sensor_data_queue, &sensor_data, TX_NO_WAIT);
}
void data_to_json(int16_t data, UINT tag, cJSON* constr) {
  switch (tag) {
    case 1: // Vibration
        cJSON_AddItemToArray(vibration_array, cJSON_CreateNumber(data));
        break;
    case 2: // Inside temperature
        cJSON_AddItemToArray(inside_temp_array, cJSON_CreateNumber(data));
        break;
    case 3: // Outside temperature
        cJSON_AddItemToArray(outside_temp_array, cJSON_CreateNumber(data));
        break;
    case 4: // Inside humidity
        cJSON_AddItemToArray(inside_humidity_array, cJSON_CreateNumber(data));
        break;
    case 5: // Outside humidity
        cJSON_AddItemToArray(outside_humidity_array, cJSON_CreateNumber(data));
        break;
    case 6: // Time of flight
        cJSON_AddItemToArray(tof_array, cJSON_CreateNumber(data));
        break;
    case 7: 
      cJSON_AddItemToArray(timestamp_array, data);
      break;
  }
}
void free_json_string(char* json_string) {
  if (json_string != NULL) {
      cJSON_free(json_string);
  }
}
void transmission_timer_callback(ULONG timer_input)
{
    UNUSED(timer_input);
    tx_event_flags_set(&json_events, JSON_TRANSMIT_EVENT, TX_OR);
}

void create_arrays(cJSON* root){
  vibration_array = cJSON_CreateArray();
  inside_temp_array = cJSON_CreateArray();
  outside_temp_array = cJSON_CreateArray();
  inside_humidity_array = cJSON_CreateArray();
  outside_humidity_array = cJSON_CreateArray();
  tof_array = cJSON_CreateArray();
  timestamp_array = cJSON_CreateArray();
  
  // Add arrays to root object
  cJSON_AddItemToObject(root, "Vibration", vibration_array);
  cJSON_AddItemToObject(root, "Inside_temperature", inside_temp_array);
  cJSON_AddItemToObject(root, "Outside_temperature", outside_temp_array);
  cJSON_AddItemToObject(root, "Inside_humidity", inside_humidity_array);
  cJSON_AddItemToObject(root, "Outside_humidity", outside_humidity_array);
  cJSON_AddItemToObject(root, "Time_of_flight", tof_array);
  cJSON_AddItemToObject(root, "Timestamp", timestamp_array);
}
char* get_timestamp(void)
{
  static char timestamp_buffer[20] = "00:00:00"; // Default value
    
  // Try with just a static timestamp first to isolate the issue
  return timestamp_buffer;
  
  /* Uncomment this only after the above works
  ULONG system_ticks = tx_time_get();
  ULONG seconds = system_ticks / TX_TIMER_TICKS_PER_SECOND;
  ULONG minutes = seconds / 60;
  ULONG hours = minutes / 60;
  
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  
  sprintf(timestamp_buffer, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  
  return timestamp_buffer;
  */
}