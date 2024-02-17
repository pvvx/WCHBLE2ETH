/********************************** (C) COPYRIGHT *******************************
 * File Name          : observer.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2018/12/10
 * Description        : Observer application, initialize scan parameters,
 *                      then scan regularly, if the scan result is not empty, 
 *                      print the scanned broadcast address
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "CONFIG.h"
#include "string.h"
#include "observer.h"
#include "wchnet.h"
#include "eth.h"

/*********************************************************************
 * MACROS
 */
#if(DEBUG)
  #define DEBUGPRINT(format, ...)    printf(format, ##__VA_ARGS__)
#else
  #define DEBUGPRINT(X...)
#endif
/*********************************************************************
 * CONSTANTS
 */

// Maximum number of scan responses
#define DEFAULT_MAX_SCAN_RES			64 // =0 unlimited

// Scan duration in (625us)
#define DEFAULT_SCAN_DURATION			8000 // 10000 ms

// Discovey mode (limited, general, all)
#define DEFAULT_DISCOVERY_MODE			DEVDISC_MODE_ALL

// TRUE to use active scan
#define DEFAULT_DISCOVERY_ACTIVE_SCAN	FALSE

// TRUE to use white list during discovery
#define DEFAULT_DISCOVERY_WHITE_LIST	FALSE


#define APP_TX_BUFFER_LENGTH			4096
/*********************************************************************
 * TYPEDEFS
 */

typedef struct _adv_msg_t {
	uint8_t		len;
    uint8_t		adTypes;                  //address type: @ref GAP_ADDR_TYPE_DEFINES | adv type
    uint8_t		phyTypes;  				  // PHY primary | secondary
    int8_t		rssi;                     //!< Advertisement or SCAN_RSP RSSI
    uint8_t		addr[B_ADDR_LEN];         //!< Address of the advertisement or SCAN_RSP
	uint8_t		data[255];
}adv_msg_t;

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
uint8_t gStatus;

// Task ID for internal task/event processing
static uint8_t ObserverTaskId;

// Number of scan results and scan result index
//static uint8_t ObserverScanRes;

// Scan result list
//static gapDevRec_t ObserverDevList[DEFAULT_MAX_SCAN_RES];

app_drv_fifo_t app_tx_fifo;

adv_msg_t adv_msg;

uint8_t app_tx_buffer[APP_TX_BUFFER_LENGTH];

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void ObserverEventCB(gapRoleEvent_t *pEvent);
static void Observer_ProcessTMOSMsg(tmos_event_hdr_t *pMsg);
static void ObserverAddDeviceInfo(uint8_t *pAddr, uint8_t addrType);

/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static const gapRoleObserverCB_t ObserverRoleCB = {
    ObserverEventCB // Event callback
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      Observer_Init
 *
 * @brief   Initialization function for the Simple BLE Observer App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notification).
 *
 * @param   task_id - the ID assigned by TMOS.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void Observer_Init()
{
    ObserverTaskId = TMOS_ProcessEventRegister(Observer_ProcessEvent);

    app_drv_fifo_init(&app_tx_fifo, app_tx_buffer, APP_TX_BUFFER_LENGTH);

    // Setup Observer Profile
    uint8_t scanRes = DEFAULT_MAX_SCAN_RES;
    GAPRole_SetParameter(GAPROLE_MAX_SCAN_RES, sizeof(uint8_t), &scanRes);

    // Setup GAP
    GAP_SetParamValue(TGAP_DISC_SCAN, DEFAULT_SCAN_DURATION);
    GAP_SetParamValue(TGAP_DISC_SCAN_PHY, GAP_PHY_BIT_LE_1M | GAP_PHY_BIT_LE_CODED);
    GAP_SetParamValue(TGAP_FILTER_ADV_REPORTS, 0);


    // Setup a delayed profile startup
    tmos_set_event(ObserverTaskId, START_DEVICE_EVT);
}

/*********************************************************************
 * @fn      Observer_ProcessEvent
 *
 * @brief   Simple BLE Observer Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The TMOS assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16_t Observer_ProcessEvent(uint8_t task_id, uint16_t events)
{
    //  VOID task_id; // TMOS required parameter that isn't used in this function

    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;

        if((pMsg = tmos_msg_receive(ObserverTaskId)) != NULL)
        {
            Observer_ProcessTMOSMsg((tmos_event_hdr_t *)pMsg);

            // Release the TMOS message
            tmos_msg_deallocate(pMsg);
        }

        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }

    if(events & START_DEVICE_EVT)
    {
        // Start the Device
        GAPRole_ObserverStartDevice((gapRoleObserverCB_t *)&ObserverRoleCB);

        return (events ^ START_DEVICE_EVT);
    }

    // Discard unknown events
    return 0;
}

/*********************************************************************
 * @fn      Observer_ProcessTMOSMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void Observer_ProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    switch(pMsg->event)
    {
        case GATT_MSG_EVENT:
            break;

        default:
            break;
    }
}

/*********************************************************************
 * @fn      ObserverEventCB
 *
 * @brief   Observer event callback function.
 *
 * @param   pEvent - pointer to event structure
 *
 * @return  none
 */
static void ObserverEventCB(gapRoleEvent_t *pEvent)
{
	uint16_t len;
    switch(pEvent->gap.opcode)
    {
        case GAP_DEVICE_INIT_DONE_EVENT:
        {
            GAPRole_ObserverStartDiscovery(DEFAULT_DISCOVERY_MODE,
                                           DEFAULT_DISCOVERY_ACTIVE_SCAN,
                                           DEFAULT_DISCOVERY_WHITE_LIST);
            DEBUGPRINT("Discovering...\r\n");
        }
        break;

        case GAP_DEVICE_INFO_EVENT:
        {
        	if(!socket_connected)
        		break;
        	len = (uint8_t)pEvent->deviceInfo.dataLen;
       		adv_msg.len = (uint8_t)len;
           	adv_msg.adTypes = pEvent->deviceInfo.eventType | (pEvent->deviceInfo.addrType << 4);
           	adv_msg.phyTypes = GAP_PHY_BIT_LE_1M | (GAP_PHY_BIT_LE_1M << 4);
           	adv_msg.rssi = pEvent->deviceInfo.rssi;
           	memcpy(adv_msg.addr, pEvent->deviceInfo.addr, B_ADDR_LEN);
        	if(len)
        		memcpy(adv_msg.data, pEvent->deviceInfo.pEvtData, len);
        	len += 10;
        	app_drv_fifo_write(&app_tx_fifo, (uint8_t *)&adv_msg, &len);
        	if(app_drv_fifo_length(&app_tx_fifo) >= RECE_BUF_LEN)
        		tmos_set_event(eth_TaskID, ETH_SENG_DATA_EVENT);
        }
        break;

        case GAP_DEVICE_DISCOVERY_EVENT:
        {
            GAPRole_ObserverStartDiscovery(DEFAULT_DISCOVERY_MODE,
                                           DEFAULT_DISCOVERY_ACTIVE_SCAN,
                                           DEFAULT_DISCOVERY_WHITE_LIST);
//			if(socket_connected == 0)
//				break;
//			tmos_set_event(eth_TaskID, ETH_SENG_DATA_EVENT);
        }
        break;

        case GAP_EXT_ADV_DEVICE_INFO_EVENT:
        {
        	if(socket_connected == 0)
        		break;
        	len = (uint8_t)pEvent->deviceExtAdvInfo.dataLen;
       		adv_msg.len = (uint8_t)len;
           	adv_msg.adTypes = pEvent->deviceExtAdvInfo.eventType | (pEvent->deviceExtAdvInfo.addrType << 4);
           	adv_msg.phyTypes = pEvent->deviceExtAdvInfo.primaryPHY | (pEvent->deviceExtAdvInfo.secondaryPHY << 4);
           	adv_msg.rssi = pEvent->deviceExtAdvInfo.rssi;
           	memcpy(adv_msg.addr, pEvent->deviceExtAdvInfo.addr, B_ADDR_LEN);
           	if(len)
           		memcpy(adv_msg.data, pEvent->deviceExtAdvInfo.pEvtData, len);
        	len += 10;
           	app_drv_fifo_write(&app_tx_fifo, (uint8_t *)&adv_msg,  &len);
        	if(app_drv_fifo_length(&app_tx_fifo) >= RECE_BUF_LEN)
        		tmos_set_event(eth_TaskID, ETH_SENG_DATA_EVENT);
        }
        break;

        case GAP_DIRECT_DEVICE_INFO_EVENT:
        {
            PRINT("Recv dir adv");
            for(int i = B_ADDR_LEN-1; i >= 0; i--) {
                PRINT("%0x", pEvent->deviceDirectInfo.addr[i]);
            }
            PRINT("\r\n");
        	if(socket_connected == 0)
        		break;
       		adv_msg.len = 0;
           	adv_msg.adTypes = pEvent->deviceDirectInfo.eventType | (pEvent->deviceDirectInfo.addrType << 4);
           	adv_msg.phyTypes = GAP_PHY_BIT_LE_1M | (GAP_PHY_BIT_LE_1M << 4);
           	adv_msg.rssi = pEvent->deviceDirectInfo.rssi;
           	memcpy(adv_msg.addr, pEvent->deviceDirectInfo.addr, B_ADDR_LEN);
        	len = 10;
        	app_drv_fifo_write(&app_tx_fifo, (uint8_t *)&adv_msg, &len);
        	if(app_drv_fifo_length(&app_tx_fifo) >= RECE_BUF_LEN)
        		tmos_set_event(eth_TaskID, ETH_SENG_DATA_EVENT);
        }
        break;
/*
        case GAP_PERIODIC_ADV_DEVICE_INFO_EVENT:
        	if(socket_connected == 0)
        		break;
        	len = (uint8_t)pEvent->devicePeriodicInfo.dataLength;
       		adv_msg.len = (uint8_t)len;
           	adv_msg.eventType = pEvent->devicePeriodicInfo.;
           	adv_msg.addrType = pEvent->devicePeriodicInfo.;
           	adv_msg.rssi = pEvent->devicePeriodicInfo.rssi;
           	memcpy(adv_msg.addr, pEvent->devicePeriodicInfo., B_ADDR_LEN);
           	if(len)
           		memcpy(adv_msg.data, pEvent->devicePeriodicInfo.pEvtData, len);
        	len += 9;
           	app_drv_fifo_write(&app_tx_fifo, (uint8_t *)&adv_msg,  &len);
        	if(app_drv_fifo_length(&app_tx_fifo) >= RECE_BUF_LEN)
        		tmos_set_event(eth_TaskID, ETH_SENG_DATA_EVENT);
*/
        default:
            break;
    }
}

/*********************************************************************
*********************************************************************/
