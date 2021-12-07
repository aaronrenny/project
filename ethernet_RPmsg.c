/*!
 *  \example App.c
 *
 *  \brief
 *  EtherNet/IP&trade; Adapter Example Application.
 *
 *  \author
 *  KUNBUS GmbH
 *
 *  \date
 *  2021-06-09
 *
 *  \copyright
 *  Copyright (c) 2021, KUNBUS GmbH<br /><br />
 *  All rights reserved.<br />
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:<br />
 *  <ol>
 *  <li>Redistributions of source code must retain the above copyright notice, this
 *     list of conditions and the following disclaimer.</li>
 *  <li>Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.</li>
 *  <li>Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.</li>
 *  </ol>
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include <api/EI_API.h>
#include <api/EI_API_def.h>

#include "AppPerm.h"
#include <drivers/pinmux.h>
#include <drivers/pruicss.h>
#include <drivers/sciclient/include/sciclient_pm.h>
#include <appPhyReset.h>

#include <osal.h>
#include <osal_error.h>
#include <hwal.h>
#include <pru.h>
#include <pru_EthernetIP.h>
#include <Board.h>

#include <kbTrace.h>


#include <App.h>
#include <AppClass71.h>


#include <drivers/ipc_notify.h>
#include <drivers/ipc_rpmsg.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/TaskP.h>


#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_config.h"
#include "ti_board_open_close.h"

//Board_IDInfo_v2 boardInfo;
extern PRUICSS_Handle prusshandle;
//int _vectors = 0;

// Global variables and pointers used in this example.
// has to stay, used in lib_eip_lwip_ip :-(
uint8_t EI_APP_aMacAddr_g[] = {0xc8, 0x3e, 0xa7, 0x00, 0x00, 0x59};


// Static variables and pointers used in this example.
static void *EI_APP_pvTaskMainHandle_s;
static uint8_t EI_APP_aTaskStackMainStack_s[EI_APP_STACK_MAIN_TASK_STACK_SIZE];

static EI_API_ADP_T*      pAdp_s = NULL;
static EI_API_CIP_NODE_T* pCip_s = NULL;

// Function prototypes.
static uint8_t* EI_APP_getMacAddr (void);
static bool     EI_APP_cipSetup(EI_API_CIP_NODE_T* pCipNode_p);
static bool     EI_APP_cipCreateCallback(EI_API_CIP_NODE_T* pCipNode_p);

uint32_t EI_APP_globalError_g = 0;
uint8_t  EI_APP_pruLogicalInstance_g = EI_API_ADP_PRUICCSS_INSTANCE_TWO;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




#define MAIN_TASK_PRI  (configMAX_PRIORITIES-1)

#define MAIN_TASK_SIZE (16384U/sizeof(configSTACK_DEPTH_TYPE))

/* The service names MUST match what linux is expecting
*/
/* This is used to run the echo test with linux kernel */
#define IPC_RPMESSAGE_SERVICE_PING        "ti.ipc4.ping-pong"
#define IPC_RPMESSAGE_ENDPT_PING          (13U)

/* This is used to run the echo test with user space kernel */
#define IPC_RPMESSAGE_SERVICE_CHRDEV      "rpmsg_chrdev"
#define IPC_RPMESSAGE_ENDPT_CHRDEV_PING   (14U)

/* Use by this to receive ACK messages that it sends to other RTOS cores */
#define IPC_RPMESSAGE_RNDPT_ACK_REPLY     (11U)
/* maximum size that message can have in this example */
#define IPC_RPMESSAGE_MAX_MSG_SIZE        (96u)

/*
* Number of RP Message ping "servers" we will start,
* - one for ping messages for linux kernel "sample ping" client
* - and another for ping messages from linux "user space" client using "rpmsg char"
*/
#define IPC_RPMESSAGE_NUM_RECV_TASKS         (2u)

/* Task priority, stack, stack size and task objects, these MUST be global's */
#define IPC_RPMESSAFE_TASK_PRI         (8U)
#define IPC_RPMESSAFE_TASK_STACK_SIZE  (8*1024U)


RPMessage_Object gIpcSendMsgObject[IPC_RPMESSAGE_NUM_RECV_TASKS];
uint8_t gIpcTaskStack[IPC_RPMESSAGE_NUM_RECV_TASKS][IPC_RPMESSAFE_TASK_STACK_SIZE] __attribute__((aligned(32)));


TaskP_Object gIpcTask[IPC_RPMESSAGE_NUM_RECV_TASKS];







///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  This function deinitializes / cleans the objects created
 *  before an exit.
 *
 *  \details
 *  This function deinitializes CIP node and ADP objects. It also
 *  deinitializes the IDK board GPIOs before an exit.
 *
 *  <!-- Parameters and return values: -->
 *
 *  \return     int               Exit code.
 *
 *  \retval     0                 Clean up success.
 *  \retval     1                 Clean up failed.
 *
 */
static int EI_APP_cleanup()
{
    int exit_code = 0;

    // Try to delete the CIP node.
    if (EI_API_CIP_NODE_delete(pCip_s))
    {
        // Fail.
        exit_code = 1;
    }

    // Try to delete the ADP.
    if (EI_API_ADP_delete(pAdp_s))
    {
        // Fail.
        exit_code = 1;
    }

    // Try to deinitialize the IDK board (unexport the GPIO pins).
    if (IDK_deInit(pAdp_s, eTMDX654IDKEVM))
    {
        // Fail
        exit_code = 1;
    }

    return exit_code;
}


/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  Initialize EtherNet/IP&trade; Adapter data.
 *
 *  \details
 *  Enable the device implementer to override default data, such as for example
 *  Vendor ID, Device Type, Product Code, Revision, Product Name, etc.
 *  and read permanent saved data.
 */
void EI_APP_adpInit(void)
{
    uint32_t errCode = 0;
    uint16_t vendorId = 806;
    uint16_t deviceType = 0x000c;
    uint16_t productCode = 0x6401;
    uint32_t serialNumber = 0x00000065;

    EI_API_ADP_SRevision_t revision;

    const char productName[] = "EtherNet/IP(tm) Adapter";

    errCode = EI_API_ADP_setVendorId(pAdp_s, vendorId);
    errCode = EI_API_ADP_setDeviceType(pAdp_s, deviceType);
    errCode = EI_API_ADP_setProductCode(pAdp_s, productCode);

    revision.major = 0x01;
    revision.minor = 0x03;
    errCode = EI_API_ADP_setRevision(pAdp_s, revision);

    errCode = EI_API_ADP_setSerialNumber(pAdp_s, serialNumber);
    errCode = EI_API_ADP_setProductName(pAdp_s, productName);

    // apply permanent saved data.
    EI_APP_PERM_read(true);
}

/*!
*  <!-- Description: -->
*
*  \brief
*  Generates attributes and services for a CIP&trade;class.
*
*  \details
*  Create a CIP class with a Class IDs using the value specified in parameter classId.<br />
*  Generates attributes and services for that class.<br />
*  Adds read and write services.<br />
*  Adds 64 8-bit attributes with callback function.<br />
*  Adds 32 16-bit attributes.<br />
*  Adds 16 32-bit attributes.<br />
*  Adds 8 64-bit attributes.
*
*/
void EI_APP_cipGenerateContent(EI_API_CIP_NODE_T* pCipNode_p, uint16_t classId_p, uint16_t instanceId_p)
{
    EI_API_CIP_SService_t service;
    uint16_t i = 0;

    EI_API_CIP_createClass(pCip_s, classId_p);

    service.getAttrAllResponseCnt = 0;
    service.callback = NULL;
    service.code = EI_API_CIP_eSC_GETATTRSINGLE;
    EI_API_CIP_addClassService(pCip_s, classId_p, &service);
    service.code = EI_API_CIP_eSC_SETATTRSINGLE;
    EI_API_CIP_addClassService(pCip_s, classId_p, &service);

    EI_API_CIP_createInstance(pCip_s, classId_p, instanceId_p);

    service.code = EI_API_CIP_eSC_GETATTRSINGLE;
    EI_API_CIP_addInstanceService(pCip_s, classId_p, instanceId_p, &service);
    service.code = EI_API_CIP_eSC_SETATTRSINGLE;
    EI_API_CIP_addInstanceService(pCip_s, classId_p, instanceId_p, &service);

    uint16_t attribID = 0x300;

    // 64 USINT (uint8_t).
    for (i = 0; i < 64; i++)
    {
        EI_API_CIP_SAttr_t attr;
        memset(&attr, 0, sizeof(attr));
        attr.id = attribID;
        attr.edt = EI_API_CIP_eEDT_USINT;
        attr.accessRule = EI_API_CIP_eAR_GET_AND_SET;
        attr.pvValue = &i;

        EI_API_CIP_addInstanceAttr(pCip_s, classId_p, instanceId_p, &attr);
        EI_API_CIP_setInstanceAttr(pCip_s, classId_p, instanceId_p, &attr);

        attribID++;
    }

    // 32 UINT (uint16_t).
    for (i = 0; i < 32; i++)
    {
        EI_API_CIP_SAttr_t attr;
        memset(&attr, 0, sizeof(attr));
        attr.id = attribID;
        attr.edt = EI_API_CIP_eEDT_UINT;
        attr.accessRule = EI_API_CIP_eAR_GET_AND_SET;
        attr.pvValue = &i;

        EI_API_CIP_addInstanceAttr(pCip_s, classId_p, instanceId_p, &attr);
        EI_API_CIP_setInstanceAttr(pCip_s, classId_p, instanceId_p, &attr);

        attribID++;
    }

    // 16 UDINT (uint32_t).
    for (i = 0; i < 16; i++)
    {
        EI_API_CIP_SAttr_t attr;
        memset(&attr, 0, sizeof(attr));
        attr.id = attribID;
        attr.edt = EI_API_CIP_eEDT_UDINT;
        attr.accessRule = EI_API_CIP_eAR_GET_AND_SET;
        attr.pvValue = &i;

        EI_API_CIP_addInstanceAttr(pCip_s, classId_p, instanceId_p, &attr);
        EI_API_CIP_setInstanceAttr(pCip_s, classId_p, instanceId_p, &attr);

        attribID++;
    }

    // 8 ULINT (uint64_t).
    for (i = 0; i < 8; i++)
    {
        EI_API_CIP_SAttr_t attr;
        memset(&attr, 0, sizeof(attr));
        attr.id = attribID;
        attr.edt = EI_API_CIP_eEDT_ULINT;
        attr.accessRule = EI_API_CIP_eAR_GET_AND_SET;
        attr.pvValue = &i;

        EI_API_CIP_addInstanceAttr(pCip_s, classId_p, instanceId_p, &attr);
        EI_API_CIP_setInstanceAttr(pCip_s, classId_p, instanceId_p, &attr);

        attribID++;
    }
}

/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  Setup the application with classes, instances, attributes, and assemblies.
 *
 *  \details
 *  Setup the application with classes, instances, attributes, and assemblies.<br />
 *  For the assemblies, use instances in the Vendor Specific range of IDs.
 *
 */
static bool EI_APP_cipSetup(EI_API_CIP_NODE_T* pCipNode_p)
{
    uint32_t errCode = 0;
    uint16_t i = 0;

    uint16_t classId = 0x70;
    uint16_t instanceId = 0x01;


    // Create new class 0x70 with read and write service and several attributes.
    EI_APP_cipGenerateContent(pCipNode_p, classId, instanceId);

    errCode = EI_API_CIP_createAssembly(pCipNode_p, 0xfe, EI_API_CIP_eAR_GET); // Input-only.
    errCode = EI_API_CIP_createAssembly(pCipNode_p, 0xff, EI_API_CIP_eAR_GET); // Listen-only.

    errCode = EI_API_CIP_createAssembly(pCipNode_p, 0x64, EI_API_CIP_eAR_GET_AND_SET);
    errCode = EI_API_CIP_createAssembly(pCipNode_p, 0x65, EI_API_CIP_eAR_GET_AND_SET);

    for (i = 0x300; i < 0x305; i++)
    {
        errCode = EI_API_CIP_addAssemblyMember(pCipNode_p, 0x64, classId, instanceId, i);
        if (errCode != EI_API_CIP_eERR_OK)
        {
            OSAL_printf("Failed to add Class ID %#x, Instance ID %#x, Attribute ID %#x to Assembly Instance 0x64:  Error code: 0x%08x\n", classId, instanceId, (uint16_t)i, errCode);
        }

        errCode = EI_API_CIP_addAssemblyMember(pCipNode_p, 0x65, classId, instanceId, (uint16_t)(8 + i));
        if (errCode != EI_API_CIP_eERR_OK) {
            OSAL_printf("Failed to add Class ID %#x, Instance ID %#x, Attribute ID %#x to Assembly Instance 0x65:  Error code: 0x%08x\n", classId, instanceId, (uint16_t)(8 + i), errCode);
        }

    }

    return true;
}


/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  Creates several callbacks.
 *
 *  \details
 *  Creates and registers callbacks which are needed for program flow.
 *  - Store non-volatile data callback.
 *  - Set_Attribute_Single service callback.
 *  - Reset service callback.
 *
 */
static bool EI_APP_cipCreateCallback(EI_API_CIP_NODE_T* pCipNode_p)
{
    uint8_t errorCnt = 0;   // Variable to check sum up return errors.

    // Your callback function which stores your data when triggered.
    EI_API_CIP_CBService ptr_my_config_cb = EI_APP_PERM_configCb;

    // Register callbacks for Set_Attribute_Single service.
    EI_API_CIP_SService_t srvc = { EI_API_CIP_eSC_SETATTRSINGLE, 0, NULL, ptr_my_config_cb };
    EI_API_CIP_setInstanceServiceFunc(pCipNode_p, 0x00F5, 0x0001, &srvc);
    EI_API_CIP_setInstanceServiceFunc(pCipNode_p, 0x00F6, 0x0001, &srvc);
    EI_API_CIP_setInstanceServiceFunc(pCipNode_p, 0x00F6, 0x0002, &srvc);
    EI_API_CIP_setInstanceServiceFunc(pCipNode_p, 0x0048, 0x0001, &srvc);

    // Add callback for Reset service of class ID 0x01.
    EI_API_CIP_SService_t srvcReset = { EI_API_CIP_eSC_RESET, 0, NULL, EI_APP_PERM_reset };
    if (EI_API_CIP_setInstanceServiceFunc(pCipNode_p, 0x01, 0x01, &srvcReset) != EI_API_CIP_eERR_OK) errorCnt++;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t set_val(EI_API_CIP_NODE_T* pCipNode_p, uint16_t classId, uint16_t instanceId, uint16_t attrId, uint16_t len, void* pvValue)
{


    printf("Callback called from attribute 0x02!\n");
}




void set_attribute_single(EI_API_CIP_NODE_T *pCipNode_p, uint16_t classId_p, uint16_t instanceId_p, uint16_t attrId_p, EI_API_CIP_ESc_t serviceCode_p, int16_t serviceFlag_p)
{
    printf("Callback called from instance 0x02!\n");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  Basic initialization function.
 *
 *  \details
 *  Creates a new EtherNet/IP&trade; Adapter.<br />
 *  Initializes data structures from non-volatile storage.<br />
 *  Registers stack error handler.<br />
 *  Initializes the Adapter.<br />
 *  Create a CIP&trade; node.<br />
 *
 */
bool EI_APP_init(void)
{
    bool result = 1;
    EI_API_ADP_SParam_t macAddr;


    // Initialize adapter for 1 (one) interface.
    pAdp_s = EI_API_ADP_new(1);

    // Init module for permanent data. Get a flash handle
    EI_APP_PERM_init(pAdp_s);

    // Setup error handler for the EtherNet/IP stack.
    EI_API_ADP_setErrorHandlerFunc(EI_APP_stackErrorHandlerCb);

    EI_APP_stackInit(EI_APP_pruLogicalInstance_g); //loads mac address and start firmware

    // Create a CIP node.
    pCip_s = EI_API_CIP_NODE_new();

    // Create callbacks for changed values.
    EI_APP_cipCreateCallback(pCip_s);

    // Create vendor specific classes.
    EI_APP_cipSetup(pCip_s);

    /////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////


    uint16_t classId_1 = 0x80;
    uint16_t instanceId_1 = 0x02;
    EI_API_CIP_SAttr_t attr_1;
    uint32_t led_val;



    uint32_t errCode = EI_API_CIP_createClass(pCip_s, classId_1);
    errCode = EI_API_CIP_createInstance(pCip_s, classId_1,instanceId_1 );

    EI_API_CIP_SService_t service_instance_1;
    service_instance_1.code = EI_API_CIP_eSC_SETATTRSINGLE;
    service_instance_1.callback = set_attribute_single;

    errCode = EI_API_CIP_addInstanceService(pCip_s, classId_1, instanceId_1, &service_instance_1);

    errCode = EI_API_CIP_setInstanceServiceFunc(pCip_s, classId_1, instanceId_1, &service_instance_1);




    attr_1.id = 0x01;
    attr_1.accessRule = EI_API_CIP_eAR_SET;  // set
    attr_1.edt = EI_API_CIP_eEDT_UINT; //elementary data type
    attr_1.cdt = EI_API_CIP_eCDT_NO;  //constructed data type ,i.e, array, structure
    attr_1.edtSize = 0;
    attr_1.cdtSize = 0;
    attr_1.pvValue = &led_val;
    attr_1.get_callback=NULL;
    attr_1.set_callback = &set_val;



    errCode = EI_API_CIP_addInstanceAttr(pCip_s, classId_1, instanceId_1, &attr_1);

    errCode = EI_API_CIP_setInstanceAttrFunc(pCip_s, classId_1, instanceId_1, &attr_1);







       ///////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////

    IDK_init(pAdp_s, eTMDX654IDKEVM);

    EI_APP_CLASS71_init(pCip_s);

    // Initialize data for the adapter.
    EI_APP_adpInit();

    // Finally apply.
    EI_API_ADP_init(pAdp_s);

    EI_API_ADP_getMacAddr(pAdp_s, &macAddr);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////

    errCode = EI_API_ADP_setIpConfig(pAdp_s,true, 0xC0A8010A, 0xFFFFFF00, 0xC0A80101, 0x0, 0x0, "", false); //enable DHCP functionality



    uint32_t adpIpAddr;
    char str[10];
    errCode = EI_API_ADP_getIpAddr(pAdp_s, &adpIpAddr);

    sprintf( str, "%X", adpIpAddr );



    RPMessage_send(
               str, strlen(str),
               CSL_CORE_ID_A53SS0_0, 1025,
               RPMessage_getLocalEndPt(&gIpcSendMsgObject[1]),
               SystemP_WAIT_FOREVER);



    char sendMsg[64] = "STACK INITIALIZED";

        RPMessage_send(
            sendMsg, strlen(sendMsg),
            CSL_CORE_ID_A53SS0_0, 1025,
            RPMessage_getLocalEndPt(&gIpcSendMsgObject[1]),
            SystemP_WAIT_FOREVER);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////

    OSAL_printf("EI_API_ADP_getMacAddr:  %02x:%02x:%02x:%02x:%02x:%02x\r\n",
        macAddr.data[0],
        macAddr.data[1],
        macAddr.data[2],
        macAddr.data[3],
        macAddr.data[4],
        macAddr.data[5]);


    return result;
}

/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  Cyclically called run function.
 *
 *  \details
 *  Cyclically called run function, handles the EtherNet/IP stack and hardware
 *  specific functionality, such as reading switches, reading inputs, setting outputs
 *  and LEDs.
 *
 */
void EI_APP_run(void)
{
    EI_API_ADP_run();
    //IDK_run(pCip_s);
    //EI_APP_CLASS71_run();
}

/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  Cyclically called run function.
 *
 *  \details
 *  Cyclically called run function, handles the EtherNet/IP stack and hardware
 *  specific functionality, such as reading switches, reading inputs, setting outputs
 *  and LEDs.
 *
 *  We return with the following exit codes from this function:
 *  Code 10 for EIP Identity Reset Type 0.
 *  Code 11 for EIP Identity Reset Type 1.
 *  Code 12 for EIP Identity Reset Type 2.
 *  Code 130 on reception of SIGINT signal.
 *  Code 143 on reception of SIGTERM signal.
 *
 *
 */
void EI_APP_mainTask(
    void* pvTaskArg_p)
{
    Board_init();
    Drivers_open();
    Board_driversOpen(); //initialises ethPhy
    //EIP_socInit();
    //GPIO_init();

    int16_t resetServiceFlag;
    if (false == EI_APP_init())  //creates new adapter, gets flash handle for writing permentant data , initialises stack
    {
        OSAL_printf("Fatal error: application initialization failed\n");
        return;
    }



    for (;;)
    {
        EI_APP_run();
        resetServiceFlag = EI_APP_PERM_getResetRequired();
        if (resetServiceFlag  != -1)
        {
            break;
        }

        OSAL_SCHED_yield();
    }

    EI_APP_cleanup();

    PRU_EIP_stop();

    Board_deinit();

    System_deinit();

    Sciclient_pmDeviceReset(0xFFFFFFFFU);
}

/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  Main entry point.
 *
 *  \details
 *  Main entry point.<br />
 *  Initializes the general operating system abstraction layer,
 *  starts the EtherNet/IP&trade; main application task, and starts the
 *  operating system abstraction layer.
 *
 */
int main(
    int argc,
    char* argv[])
{
    uint32_t err;

    System_init ();

    Board_init ();

    err = HWAL_init ();
    if (err != OSAL_NO_ERROR)
    {
        goto laError;
    }

    OSAL_registerErrorHandler (EI_APP_osErrorHandlerCb);

    err = OSAL_init ();
    if (err != OSAL_NO_ERROR)
    {
        goto laError;
    }


    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



    int32_t status;

        RPMessage_CreateParams createParams;


            /* This API MUST be called by applications when its ready to talk to Linux */
            status = RPMessage_waitForLinuxReady(SystemP_WAIT_FOREVER);
            DebugP_assert(status==SystemP_SUCCESS);

            RPMessage_CreateParams_init(&createParams);
            createParams.localEndPt = IPC_RPMESSAGE_ENDPT_CHRDEV_PING;
            status = RPMessage_construct(&gIpcSendMsgObject[1], &createParams);

          /* We need to "announce" to Linux client else Linux does not know a service exists on this CPU
                           * This is not mandatory to do for RTOS clients               */

            status = RPMessage_announce(CSL_CORE_ID_A53SS0_0, IPC_RPMESSAGE_ENDPT_CHRDEV_PING, IPC_RPMESSAGE_SERVICE_CHRDEV);




            char sendMsg[64] = "RPMessage INITIALIZED";

                    RPMessage_send(
                        sendMsg, strlen(sendMsg),
                        CSL_CORE_ID_A53SS0_0, 1025,
                        RPMessage_getLocalEndPt(&gIpcSendMsgObject[1]),
                        SystemP_WAIT_FOREVER);





    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    EI_APP_pvTaskMainHandle_s = OSAL_SCHED_startTask (EI_APP_mainTask,
                                                      (void*)&EI_APP_pruLogicalInstance_g,
                                                      OSAL_TASK_ePRIO_4,
                                                      EI_APP_aTaskStackMainStack_s,
                                                      EI_APP_STACK_MAIN_TASK_STACK_SIZE,
                                                      OSAL_OS_START_TASK_FLG_NONE,
                                                      "main_task");




    if (EI_APP_pvTaskMainHandle_s == NULL)
    {
        goto laError;
    }

    OSAL_startOs ();

    return (0);

//-------------------------------------------------------------------------------------------------
laError:

    return (-1);
}


/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  General stack error handler.
 *
 *  \details
 *  General stack error handler. Reporting of stack internal errors.
 *
 */
void EI_APP_stackErrorHandlerCb(
    uint32_t i32uErrorCode_p,
    uint8_t  bFatal_p,
    uint8_t  i8uNumOfPara_p,
    va_list  argptr_p)
{
    OSAL_printf("###### Stack Error: 0x%08x, %s ######\n", i32uErrorCode_p, bFatal_p == 0 ? "non fatal" : "fatal");

    if (bFatal_p)
    {
        // TODO: handle the app_cleanup return codes.
        EI_APP_cleanup();
        // TODO: Return an appropriate exit code in this case.
        exit(255);
    }

}

/*!
 *  <!-- Description: -->
 *
 *  \brief
 *  General OS error handler.
 *
 *  \details
 *  General OS error handler. Reporting of OS errors.
 *
 */
void EI_APP_osErrorHandlerCb (
    uint32_t errorCode_p,           //!< [in] Error code
    bool fatal_p,                   //!< [in] Is Error fatal
    uint8_t paraCnt_p,              //!< [in] parameter counter
    va_list argptr_p)               //!< [in] Error arguments

{
    int32_t argInd;
    uint32_t arg;

    EI_APP_globalError_g = errorCode_p;

    OSAL_printf ("\nError: 0x%8.8x, Fatal: %s", errorCode_p, fatal_p ? "yes" : "no");

//    for (argInd = 0; argInd < paraCnt_p; argInd++)
//    {   // assume all Parameters are 32bit unsigned integer
//        arg = va_arg (argptr_p, uint32_t);
//        printf (", P%d: 0x%8.8x", argInd, arg);
//    }

    if (fatal_p == true)
    {
        //while (DBG_keepLoop_g)
        //{
        //    DBG_iDebug_g++;
        //}

        exit (1);
    }
}


//*************************************************************************************************
void EI_APP_stackInit (uint8_t pruInstance_p)
{
    uint32_t err;
    EIP_SLoadParameter tParam;

    err = APP_initPhyResetGpio (); //sets gpio direction as output ?
    if (err != OSAL_NO_ERROR)
    {
        goto laError;
    }

    memset (&tParam, 0, sizeof (EIP_SLoadParameter));
    memmove (tParam.ai8uMacAddr, EI_APP_getMacAddr(), 6);

    err = EI_API_ADP_loadMac (&tParam, pruInstance_p);
    if (err)
    {
        goto laError;
    }

    OSAL_printf("+EI_API_ADP_startFirmware\r\n");
    EI_API_ADP_startFirmware();
    OSAL_printf("-EI_API_ADP_startFirmware\r\n");

    return;

//-------------------------------------------------------------------------------------------------
laError:

    printf ("\nStack Init Error: 0x%8.8x", err);
    return;
}

//*************************************************************************************************
//| Function:
//|
//! \brief
//!
//! \detailed
//!
//!
//!
//! \ingroup
//-------------------------------------------------------------------------------------------------
static uint8_t* EI_APP_getMacAddr (void)
{
#ifndef _DEBUG_USE_KUNBUS_MAC_ADDRESS
    static uint8_t mac_addr[6];
    const uint32_t mac_address_upper_16_bits = *IDK_CTRLMMR0_MAC_ID1;
    const uint32_t mac_address_lower_32_bits = *IDK_CTRLMMR0_MAC_ID0;
    mac_addr[0] = (uint8_t) (mac_address_upper_16_bits >> 8);
    mac_addr[1] = (uint8_t) (mac_address_upper_16_bits);
    mac_addr[2] = (uint8_t) (mac_address_lower_32_bits >> 24);
    mac_addr[3] = (uint8_t) (mac_address_lower_32_bits >> 16);
    mac_addr[4] = (uint8_t) (mac_address_lower_32_bits >> 8);
    mac_addr[5] = (uint8_t) (mac_address_lower_32_bits);
    // EI_APP_aMacAddr_g is directly used in lwip
    memcpy(EI_APP_aMacAddr_g, mac_addr, 6);
    return mac_addr;
#else
    return EI_APP_aMacAddr_g;
#endif
}


//*************************************************************************************************
