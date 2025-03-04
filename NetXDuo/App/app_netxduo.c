/* USER CODE BEGIN Header */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_netxduo.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include <stdio.h>
#include <stdbool.h> 
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
TX_THREAD      NxAppThread;
NX_PACKET_POOL NxAppPool;
NX_IP          NetXDuoEthIpInstance;
/* USER CODE BEGIN PV */
NX_UDP_SOCKET UDPSocket;
ULONG IpAddress;
ULONG NetMask;
TX_THREAD SensorDataThread;
TX_THREAD AppLinkThread;
TX_EVENT_FLAGS_GROUP sensor_events;
extern UART_HandleTypeDef huart3;
extern  ETH_HandleTypeDef heth;



// mic /dma 
extern int32_t data_i2s[];
extern int32_t temp_buffer[];
extern volatile uint8_t half;
int16_t processed_audio[HALF_BUFFER_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID nx_app_thread_entry (ULONG thread_input);
/* USER CODE BEGIN PFP */
UINT MX_NetXDuo_Init(VOID *memory_ptr);
static VOID sensor_thread_entry(ULONG thread_input);
UINT UDP_Send(UDP_Data_Packet* packet, UINT destination_port);
void process_i2s_data(uint32_t* source, int16_t* dest, uint16_t size);

/* USER CODE END PFP */

/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

   /* USER CODE BEGIN App_NetXDuo_MEM_POOL */
  /* USER CODE END App_NetXDuo_MEM_POOL */
  /* USER CODE BEGIN 0 */

  /* USER CODE END 0 */

  /* Initialize the NetXDuo system. */
  CHAR *pointer;
  nx_system_initialize();

    /* Allocate the memory for packet_pool.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the Packet pool to be used for packet allocation,
   * If extra NX_PACKET are to be used the NX_APP_PACKET_POOL_SIZE should be increased
   */
  ret = nx_packet_pool_create(&NxAppPool, "NetXDuo App Pool", DEFAULT_PAYLOAD_SIZE, pointer, NX_APP_PACKET_POOL_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_POOL_ERROR;
  }

    /* Allocate the memory for Ip_Instance */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, Nx_IP_INSTANCE_THREAD_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

   /* Create the main NX_IP instance */
  ret = nx_ip_create(&NetXDuoEthIpInstance, "NetX Ip instance", NX_APP_DEFAULT_IP_ADDRESS, NX_APP_DEFAULT_NET_MASK, &NxAppPool, nx_stm32_eth_driver,
                     pointer, Nx_IP_INSTANCE_THREAD_SIZE, NX_APP_INSTANCE_PRIORITY);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

    /* Allocate the memory for ARP */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, DEFAULT_ARP_CACHE_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Enable the ARP protocol and provide the ARP cache size for the IP instance */

  /* USER CODE BEGIN ARP_Protocol_Initialization */
  /* USER CODE END ARP_Protocol_Initialization */

  ret = nx_arp_enable(&NetXDuoEthIpInstance, (VOID *)pointer, DEFAULT_ARP_CACHE_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

  /* Enable the UDP protocol required for  DHCP communication */

  /* USER CODE BEGIN UDP_Protocol_Initialization */
  /* USER CODE END UDP_Protocol_Initialization */

  ret = nx_udp_enable(&NetXDuoEthIpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_SUCCESSFUL;
  }

   /* Allocate the memory for main thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main thread */
  ret = tx_thread_create(&NxAppThread, "NetXDuo App thread", nx_app_thread_entry , 0, pointer, NX_APP_THREAD_STACK_SIZE,
                         NX_APP_THREAD_PRIORITY, NX_APP_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* USER CODE BEGIN MX_NetXDuo_Init */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, NX_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  ret = tx_thread_create(&SensorDataThread, "Sensor Data Thread", sensor_thread_entry, 0, 
                        pointer, NX_APP_THREAD_STACK_SIZE,
                        SENSOR_THREAD_PRIORITY, SENSOR_THREAD_PRIORITY, 
                        TX_NO_TIME_SLICE, TX_AUTO_START);

if (ret != TX_SUCCESS)
{
return TX_THREAD_ERROR;
}
  /* USER CODE BEGIN MX_NetXDuo_Init */
  tx_event_flags_create(&sensor_events, "Sensor Events");
  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}

/**
* @brief  Main thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID nx_app_thread_entry (ULONG thread_input)
{
  /* USER CODE BEGIN Nx_App_Thread_Entry 0 */
  (void)thread_input;
  tx_thread_sleep(400); /* Sleep for 1 second to allow Ethernet to initialize */
  printf("Starting UDP server after initialization delay\r\n");
  UINT ret;
  ULONG bytes_read;
  UCHAR data_buffer[512];
  ULONG source_ip_address;
  NX_PACKET *server_packet;
  UINT source_port;
/* Check PHY link status using the correct function signature */
uint32_t regvalue = 0;
/* Basic Status Register address is 0x01 for most PHYs */
#define PHY_BASIC_STATUS_REG    0x01
/* Link status bit is usually bit 2 in the Basic Status Register */
#define PHY_LINK_STATUS_BIT     0x0004

/* We need to provide the PHY address - usually 0 or 1 for most STM32 boards */
uint32_t PHYAddr = 0; /* Use 0 for most common configurations */
  /* Add hardware status check */
  extern ETH_HandleTypeDef heth; /* Make sure this matches your Ethernet handle name */
  printf("Ethernet hardware initialized: %s\r\n", 
         (heth.gState == HAL_ETH_STATE_READY) ? "Ready" : "Not Ready");
         
HAL_StatusTypeDef status = HAL_ETH_ReadPHYRegister(&heth, PHYAddr, PHY_BASIC_STATUS_REG, &regvalue);
if (status == HAL_OK) {
    printf("PHY status register: 0x%08lX\r\n", regvalue);
    printf("Link status: %s\r\n", (regvalue & PHY_LINK_STATUS_BIT) ? "UP" : "DOWN");
} else {
    printf("Failed to read PHY register, status: %d\r\n", status);
}

/* Get current MAC address */
printf("Current MAC: %02X-%02X-%02X-%02X-%02X-%02X\r\n", 
  heth.Init.MACAddr[0], heth.Init.MACAddr[1], heth.Init.MACAddr[2],
  heth.Init.MACAddr[3], heth.Init.MACAddr[4], heth.Init.MACAddr[5]);
  /*
   * Print IPv4
   */
  ret = nx_ip_address_get(&NetXDuoEthIpInstance, &IpAddress, &NetMask);
  if (ret != TX_SUCCESS)
  {
      printf("Failed to get IP address, error: %d\r\n", ret);
      Error_Handler();
  }
  else
  {
      printf("IP Configuration:\r\n");
      PRINT_IP_ADDRESS(IpAddress);
      printf("Subnet mask: %lu.%lu.%lu.%lu\r\n", 
             (NetMask >> 24) & 0xFF, (NetMask >> 16) & 0xFF,
             (NetMask >> 8) & 0xFF, NetMask & 0xFF);
  }

/*Create a UDP Socket and bind to it*/
  ret = nx_udp_socket_create(&NetXDuoEthIpInstance, &UDPSocket,
                             "UDP Server Socket", NX_IP_NORMAL,
                              NX_FRAGMENT_OKAY, NX_IP_TIME_TO_LIVE, QUEUE_MAX_SIZE);
  if (ret != NX_SUCCESS)
  {
      Error_Handler();
      printf("Socket not created.");
  }
/* Bind to port 6000 */
  ret = nx_udp_socket_bind(&UDPSocket, DEFAULT_PORT, TX_WAIT_FOREVER);
  if (ret != NX_SUCCESS)
  {
    printf("Socket bind failed with error: %d\r\n", ret);
     Error_Handler();
  }
  else
  {
      printf("UDP Server listening on PORT 6000.. \r\n");
  }
/*  Main Task Loop
    Waits 1 second (100 centiseconds) for each UDP packet. If received, print out message*/
  while (1)
  {
      TX_MEMSET(data_buffer, '\0', sizeof(data_buffer));
      /* wait for data for 1 sec */
      ret = nx_udp_socket_receive(&UDPSocket, &server_packet, 100);
      if (ret == NX_SUCCESS)
{
    nx_packet_data_retrieve(server_packet, data_buffer, &bytes_read);
    nx_udp_source_extract(server_packet, &source_ip_address, &source_port);
    /* Print our received data on UART com port*/
    PRINT_DATA(source_ip_address, source_port, data_buffer);
    // new line to make print out more readable
    nx_packet_release(server_packet);
}
else
{
    /* Print error information */
    //printf("Socket receive status: %d (0=Success, 7=No data)\r\n", ret);
}
      //static ULONG counter = 0;
      //printf("Loop iteration %lu, waiting for packets...\r\n", counter++);
  }
  /* USER CODE END Nx_App_Thread_Entry 0 */

}
/* USER CODE BEGIN 1 */
void sensor_thread_entry(ULONG thread_input)
{
    tx_thread_sleep(500);
    (void)thread_input;
    UDP_Data_Packet packet;
    packet.destination_ip = DESTINATION_IP;
    ULONG actual_flags;
    
    while(1)
    {
        // Wait for ANY flag to be set (blocking approach)
        tx_event_flags_get(&sensor_events, 
                          AUDIO_DATA_FLAG | TEMPHUMID_DATA_FLAG | BUTTON_DATA_FLAG,
                          TX_OR_CLEAR,  // Get any flag and clear it
                          &actual_flags, 
                          TX_WAIT_FOREVER);  // Block until any flag is set
        
        // Process the flags in priority order (if timing is critical)
        if(actual_flags & AUDIO_DATA_FLAG)
        {
            // Process audio data first (highest priority)
            uint32_t* source_buffer = data_i2s + (half ? HALF_BUFFER_SIZE : 0);
            
            process_i2s_data(source_buffer, processed_audio, HALF_BUFFER_SIZE);
            packet.data_ptr = processed_audio;
            packet.data_size = HALF_BUFFER_SIZE * sizeof(int16_t);
            packet.data_type = 0;
            
            UDP_Send(&packet, AUDIO_PORT);
        }
        
        if(actual_flags & TEMPHUMID_DATA_FLAG)
        {
            // Process temperature data
            packet.data_ptr = temp_buffer;
            packet.data_size = TEMP_BUFFER_SIZE;
            packet.data_type = 1;
            
            UDP_Send(&packet, TEMPHUMID_PORT);
        }
        
        if(actual_flags & BUTTON_DATA_FLAG)
        {
            // Process button press (lowest priority)
            char button_msg[32];
            static UINT button_count = 0;
            snprintf(button_msg, sizeof(button_msg), "Button pressed! Count: %d", button_count++);
            
            packet.data_ptr = button_msg;
            packet.data_size = strlen(button_msg);
            packet.data_type = 2;
            
            UDP_Send(&packet, BUTTON_PORT);
        }
    }
}
/* UDP sending function */
UINT UDP_Send(UDP_Data_Packet* packet, UINT destination_port )
{
  UINT ret;
  NX_PACKET* nx_packet_ptr;
  
  /* Validate inputs */
  if (packet == NULL || packet->data_ptr == NULL || packet->data_size == 0)
  {
      return NX_INVALID_PARAMETERS;
  }
  
  /* Allocate a packet */
  ret = nx_packet_allocate(&NxAppPool, &nx_packet_ptr, NX_UDP_PACKET, TX_WAIT_FOREVER);
  if (ret != NX_SUCCESS)
  {
      printf("Packet allocation failed: %d\r\n", ret);
      return ret;
  }
  
  /* Append data to the packet */
  ret = nx_packet_data_append(nx_packet_ptr, packet->data_ptr, packet->data_size, 
                             &NxAppPool, TX_WAIT_FOREVER);
  if (ret != NX_SUCCESS)
  {
      printf("Data append failed: %d\r\n", ret);
      nx_packet_release(nx_packet_ptr);
      return ret;
  }
  
  /* Send the UDP packet */
  ret = nx_udp_socket_send(&UDPSocket, nx_packet_ptr, packet->destination_ip, destination_port);
  if (ret != NX_SUCCESS)
  {
      printf("UDP send failed: %d\r\n", ret);
      nx_packet_release(nx_packet_ptr);
      return ret;
  }
  
 /*
  if (packet->data_type == 0) 
  {
      printf("Sent audio packet (%u bytes) to %lu.%lu.%lu.%lu:%u\r\n", 
             packet->data_size,
             (packet->destination_ip >> 24) & 0xFF, 
             (packet->destination_ip >> 16) & 0xFF,
             (packet->destination_ip >> 8) & 0xFF, 
             packet->destination_ip & 0xFF,
             packet->destination_port);
  }
  */
  return NX_SUCCESS;
}
void process_i2s_data(uint32_t* source, int16_t* dest, uint16_t size) {
  for (uint16_t i = 0; i < size; i++) {
      // Extract the 24-bit audio sample (stored in the first 24 bits of each 32-bit word)
      int32_t sample = source[i] & 0xFFFFFF;
      
      // Sign-extend from 24-bit to 32-bit if MSB is set
      if (sample & 0x800000) {
          sample |= 0xFF000000;
      }
      
      // Convert to 16-bit by scaling down (losing some precision)
      // Shift right by 8 bits (divide by 256)
      dest[i] = (int16_t)(sample >> 8);
  }
}


/* USER CODE END 1 */
