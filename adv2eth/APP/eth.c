/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.2
 * Date               : 2022/06/22
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
/* Header file contains */
#include "CONFIG.h"
#include "HAL.h"
#include "eth.h"
#include "eth_driver.h"
#include "string.h"
#include "debug.h"
#include "wchnet.h"
#include "observer.h"

extern uint32_t volatile LocalTime;

u8 WCHNET_DHCPCallBack(u8 status, void *arg);
/*********************************************************************
 * GLOBAL TYPEDEFS
 */

#define KEEPLIVE_ENABLE         1                           //Enable keeplive function

u8 MACAddr[6];                                    //MAC address
u8 IPAddr[4]   = { 0, 0, 0, 0 };	//IP address
u8 GWIPAddr[4] = { 0, 0, 0, 0 };	//Gateway IP address
u8 IPMask[4]   = { 0, 0, 0, 0};		//subnet mask
u16 srcport = 1000;	//source port

u8 SocketId;
u8 SocketIdForListen;
u8 socket[WCHNET_MAX_SOCKET_NUM];                           //Save the currently connected socket
u8 SocketRecvBuf[RECE_BUF_LEN];      //socket receive buffer
u8 SocketSendBuf[RECE_BUF_LEN];
uint8_t eth_TaskID;
uint8_t socket_connected;
uint32_t SendTime;
/*********************************************************************
 * @fn      mStopIfError
 *
 * @brief   check if error.
 *
 * @param   iError - error constants.
 *
 * @return  none
 */
void mStopIfError(u8 iError)
{
    if (iError == WCHNET_ERR_SUCCESS)
        return;
    PRINT("Error: %02X\r\n", (u16) iError);
}

/*********************************************************************
 * @fn      TIM2_Init  (100Hz?)
 *
 * @brief   Initializes TIM2.
 *
 * @return  none
 */
void TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure = { 0 };

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Period = SystemCoreClock / 1000000 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = WCHNETTIMERPERIOD * 1000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    TIM_Cmd(TIM2, ENABLE);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    NVIC_EnableIRQ(TIM2_IRQn);
}

/*********************************************************************
 * @fn      WCHNET_CreateTcpSocket
 *
 * @brief   Create TCP Socket
 *
 * @return  none
 */
void WCHNET_CreateTcpSocketListen(void)
{
    u8 i;
    SOCK_INF TmpSocketInf;

    memset((void *) &TmpSocketInf, 0, sizeof(SOCK_INF));
    TmpSocketInf.SourPort = srcport;
    TmpSocketInf.ProtoType = PROTO_TYPE_TCP;
    TmpSocketInf.RecvBufLen = RECE_BUF_LEN;
    i = WCHNET_SocketCreat(&SocketIdForListen, &TmpSocketInf);
    PRINT("TCP Socket %d Create, port: %d\r\n", SocketIdForListen, srcport);
    mStopIfError(i);
    i = WCHNET_SocketListen(SocketIdForListen);                   //listen for connections
    PRINT("TCP Socket %d Listen...\r\n", SocketIdForListen);
    mStopIfError(i);
}
/*********************************************************************
 * @fn      WCHNET_CommandData
 *
 * @brief   Command data function.
 *
 * @param   id - socket id.
 *
 * @return  none
 */
void WCHNET_CommandData(u8 id)
{
    u8 i;
    u32 len;
    u32 endAddr = SocketInf[id].RecvStartPoint + SocketInf[id].RecvBufLen;       //Receive buffer end address

    if ((SocketInf[id].RecvReadPoint + SocketInf[id].RecvRemLen) > endAddr)    //Calculate the length of the received data
        len = endAddr - SocketInf[id].RecvReadPoint;
    else
        len = SocketInf[id].RecvRemLen;

//    i = WCHNET_SocketSend(id, (u8 *) SocketInf[id].RecvReadPoint, &len);         //send data
//    if (i == WCHNET_ERR_SUCCESS) {
        WCHNET_SocketRecv(id, NULL, &len);                                       //Clear sent data
//    }
}

/*********************************************************************
 * @fn      WCHNET_HandleSockInt
 *
 * @brief   Socket Interrupt Handle
 *
 * @param   socketid - socket id.
 *          intstat - interrupt status
 *
 * @return  none
 */
void WCHNET_HandleSockInt(u8 socketid, u8 intstat)
{
    u8 i;

    if (intstat & SINT_STAT_RECV)                                 //receive data
    {
    	WCHNET_CommandData(socketid);                            //Command data
    }
    if (intstat & SINT_STAT_CONNECT)                              //connect successfully
    {
#if KEEPLIVE_ENABLE                                              //Configure keeplive parameters
        WCHNET_SocketSetKeepLive(socketid, ENABLE);
#endif
        WCHNET_ModifyRecvBuf(socketid, (u32) SocketRecvBuf, RECE_BUF_LEN);
        for (i = 0; i < WCHNET_MAX_SOCKET_NUM; i++) {
            if (socket[i] == 0xff) {                              //save connected socket id
                socket[i] = socketid;
                break;
            }
        }
        app_drv_fifo_flush(&app_tx_fifo);
        socket_connected = socketid;
        SendTime = LocalTime;
        PRINT("TCP Socket %d Connect\r\n", socketid);
    }
    if (intstat & SINT_STAT_DISCONNECT)                           //disconnect
    {
        socket_connected = 0;
        for (i = 0; i < WCHNET_MAX_SOCKET_NUM; i++) {             //delete disconnected socket id
            if (socket[i] == socketid) {
                socket[i] = 0xff;
                break;
            }
        }
        PRINT("TCP Socket %d Disconnect\r\n", socketid);
    }
    if (intstat & SINT_STAT_TIM_OUT)                              //timeout disconnect
    {
        socket_connected = 0;
        for (i = 0; i < WCHNET_MAX_SOCKET_NUM; i++) {             //delete disconnected socket id
            if (socket[i] == socketid) {
                socket[i] = 0xff;
                break;
            }
        }
        PRINT("TCP Socket %d Timeout\r\n", socketid);
    }
}
/*********************************************************************
 * @fn      WCHNET_HandleGlobalInt
 *
 * @brief   Global Interrupt Handle
 *
 * @return  none
 */
void WCHNET_HandleGlobalInt(void)
{
    u8 intstat;
    u16 i;
    u8 socketint;

    intstat = WCHNET_GetGlobalInt();                              //get global interrupt flag
    if (intstat & GINT_STAT_UNREACH)                              //Unreachable interrupt
    {
        PRINT("GINT_STAT_UNREACH\r\n");
    }
    if (intstat & GINT_STAT_IP_CONFLI)                            //IP conflict
    {
        PRINT("GINT_STAT_IP_CONFLICT\r\n");
    }
    if (intstat & GINT_STAT_PHY_CHANGE)                           //PHY status change
    {
        i = WCHNET_GetPHYStatus();
        if (i & PHY_Linked_Status) {
            PRINT("PHY Link Success\r\n");
            WCHNET_DHCPStop();
            WCHNET_DHCPStart(WCHNET_DHCPCallBack);				//Start DHCP
        }
    }
    if (intstat & GINT_STAT_SOCKET) {                             //socket related interrupt
        for (i = 0; i < WCHNET_MAX_SOCKET_NUM; i++) {
            socketint = WCHNET_GetSocketInt(i);
            if (socketint)
                WCHNET_HandleSockInt(i, socketint);
        }
    }
}

/********************************************************************
 * @fn      SendFifo
 *
 * @brief   Send Fifo to TCP socket
 *
 * @param   task_id - The TMOS assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  none
 */
void SendFifo() {
	if(socket_connected) {
		uint32_t len = MIN(app_drv_fifo_length(&app_tx_fifo), RECE_BUF_LEN);
		if(len) {
			app_drv_fifo_read(&app_tx_fifo, SocketSendBuf, (uint16_t *)&len);
			uint8_t stata = WCHNET_SocketSend(socket_connected, SocketSendBuf, &len);
	        if(stata)
	            PRINT("TCP send fail %x\r\n",stata);
	    	SendTime = LocalTime;
		}
	}
}

/*********************************************************************
 * @fn      eth_ProcessEvent
 *
 * @brief   eth Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id - The TMOS assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16_t eth_ProcessEvent(uint8_t task_id, uint16_t events)
{
    //  VOID task_id; // TMOS required parameter that isn't used in this function

    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;

        if((pMsg = tmos_msg_receive(eth_TaskID)) != NULL)
        {
            // Release the TMOS message
            tmos_msg_deallocate(pMsg);
        }
        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }

    if(events & ETH_SENG_DATA_EVENT)
    {
    	SendFifo();
        return (events ^ ETH_SENG_DATA_EVENT);
    }
    // Discard unknown events
    return 0;
}

/*********************************************************************
 * @fn      WCHNET_DHCPCallBack
 *
 * @brief   DHCPCallBack
 *
 * @param   status - status returned by DHCP
 *                   0x00 - Success
 *                   0x01 - Failure
 *          arg - Data returned by DHCP
 *
 * @return  DHCP status
 */
u8 WCHNET_DHCPCallBack(u8 status, void *arg)
{
    u8 *p;
    u8 tmp[4] = {0, 0, 0, 0};

    if(!status)
    {
        p = arg;
        PRINT("DHCP Success\r\n");
        /*If the obtained IP is the same as the last IP, exit this function.*/
        if(memcmp(IPAddr, p ,sizeof(IPAddr)) == 0
        	&& memcmp(GWIPAddr, &p[4] ,sizeof(GWIPAddr)) == 0
        	&& memcmp(IPMask, &p[8] ,sizeof(IPMask)) == 0)
            return READY;
        /*Determine whether it is the first successful IP acquisition*/
        if(memcmp(IPAddr, tmp ,sizeof(IPAddr))){
            /*The obtained IP is different from the last value,
             * then disconnect the last connection.*/
            PRINT("DHCP: Close Socket %x\r\n", SocketId);
            WCHNET_SocketClose(SocketId, TCP_CLOSE_NORMAL);
        }
        memcpy(IPAddr, p, sizeof(IPAddr));
        memcpy(GWIPAddr, &p[4], sizeof(GWIPAddr));
        memcpy(IPMask, &p[8], sizeof(IPMask));
        PRINT("IPAddr = %d.%d.%d.%d \r\n", (u16)IPAddr[0], (u16)IPAddr[1],
               (u16)IPAddr[2], (u16)IPAddr[3]);
        PRINT("GWIPAddr = %d.%d.%d.%d \r\n", (u16)GWIPAddr[0], (u16)GWIPAddr[1],
               (u16)GWIPAddr[2], (u16)GWIPAddr[3]);
        PRINT("IPMask = %d.%d.%d.%d \r\n", (u16)IPMask[0], (u16)IPMask[1],
               (u16)IPMask[2], (u16)IPMask[3]);
        PRINT("DNS1: %d.%d.%d.%d \r\n", p[12], p[13], p[14], p[15]);
        PRINT("DNS2: %d.%d.%d.%d \r\n", p[16], p[17], p[18], p[19]);
        //PRINT("DHCP: Create TcpSocketListen\r\n");
        WCHNET_CreateTcpSocketListen(); // Create a TCP listen
        return READY;
    }
    else
    {
        PRINT("DHCP Fail %02x \r\n", status);
        /*Determine whether it is the first successful IP acquisition*/
        if(memcmp(IPAddr, tmp ,sizeof(IPAddr))){
            /*The obtained IP is different from the last value*/
            PRINT("DHCP: Close Socket %x\r\n", SocketId);
            WCHNET_SocketClose(SocketId, TCP_CLOSE_NORMAL);
        }
        return NoREADY;
    }
}

/*********************************************************************
 * @fn      eth_init
 *
 * @brief   eth_init
 *
 * @return  none
 */
void eth_init(void)
{
    uint8_t i;
    eth_TaskID = TMOS_ProcessEventRegister(eth_ProcessEvent);
    PRINT("net version:%x\r\n", WCHNET_GetVer());
    if ( WCHNET_LIB_VER != WCHNET_GetVer()) {
        PRINT("version error.\r\n");
    }
    WCHNET_GetMacAddr(MACAddr);                                  //get the chip MAC address
    PRINT("MAC addr: ");
    for(i = 0; i < 6; i++)
    {
        PRINT("%x",MACAddr[i]);
    }
    PRINT("\r\n");
    TIM2_Init();
    WCHNET_DHCPSetHostname("WCHNET");                            //Configure DHCP host name
    i = ETH_LibInit(IPAddr, GWIPAddr, IPMask, MACAddr);          //Ethernet library initialize
    mStopIfError(i);
    if (i == WCHNET_ERR_SUCCESS)
        PRINT("WCHNET_LibInit Success\r\n");
    memset(socket, 0xff, WCHNET_MAX_SOCKET_NUM);
    WCHNET_DHCPStart(WCHNET_DHCPCallBack);				//Start DHCP
#if KEEPLIVE_ENABLE                                              //Configure keeplive parameters
    {
        struct _KEEP_CFG cfg;

        cfg.KLIdle = 20000;
        cfg.KLIntvl = 15000;
        cfg.KLCount = 9;
        WCHNET_ConfigKeepLive(&cfg);
    }
#endif
}

/*********************************************************************
 * @fn      eth_process
 *
 * @brief   eth_process
 *
 * @return  none
 */
void eth_process(void)
{
    /*Ethernet library main task function,
     * which needs to be called cyclically*/
    WCHNET_MainTask();
    /*Query the Ethernet global interrupt,
     * if there is an interrupt, call the global interrupt handler*/
    if(WCHNET_QueryGlobalInt())
    {
        WCHNET_HandleGlobalInt();
    }

    if(socket_connected && (LocalTime - SendTime) >= 250) {
    	SendTime = LocalTime;
    	SendFifo();
    }

}
/******************************** endfile @ main ******************************/
