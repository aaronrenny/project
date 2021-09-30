#include "aaron.h"


#include <stdlib.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_board_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/TaskP.h>
#include <drivers/ipc_notify.h>
#include <drivers/ipc_rpmsg.h>
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"

/* RPMessage object used to recvice messages */
RPMessage_Object gIpcRecvMsgObject[IPC_RPMESSAGE_NUM_RECV_TASKS];
uint8_t gIpcTaskStack[IPC_RPMESSAGE_NUM_RECV_TASKS][IPC_RPMESSAFE_TASK_STACK_SIZE] __attribute__((aligned(32)));


TaskP_Object gIpcTask[IPC_RPMESSAGE_NUM_RECV_TASKS];

void ipc_recv_task_main(void *args);

void frertos_main(void *args)
{
    int32_t status;

    RPMessage_CreateParams createParams;
    TaskP_Params taskParams;

        /* This API MUST be called by applications when its ready to talk to Linux */
        status = RPMessage_waitForLinuxReady(SystemP_WAIT_FOREVER);
        DebugP_assert(status==SystemP_SUCCESS);

        RPMessage_CreateParams_init(&createParams);
        createParams.localEndPt = IPC_RPMESSAGE_ENDPT_CHRDEV_PING;
        status = RPMessage_construct(&gIpcRecvMsgObject[1], &createParams);

      /* We need to "announce" to Linux client else Linux does not know a service exists on this CPU
                       * This is not mandatory to do for RTOS clients               */

        status = RPMessage_announce(CSL_CORE_ID_A53SS0_0, IPC_RPMESSAGE_ENDPT_CHRDEV_PING, IPC_RPMESSAGE_SERVICE_CHRDEV);

        TaskP_Params_init(&taskParams);
        taskParams.name = "RPMESSAGE_CHAR_PING";
        taskParams.stackSize = IPC_RPMESSAFE_TASK_STACK_SIZE;
        taskParams.stack = gIpcTaskStack[1];
        taskParams.priority = IPC_RPMESSAFE_TASK_PRI;
        /* we use the same task function for echo but pass the appropiate rpmsg handle to it, to echo messages */
        taskParams.args = &gIpcRecvMsgObject[1];
        taskParams.taskMain = ipc_recv_task_main;

        status = TaskP_construct(&gIpcTask[1], &taskParams);

        /* wait for all non-Linux cores to be ready, this ensure that when we send messages below
         * they wont be lost due to rpmsg end point not created at remote core
         */
        IpcNotify_syncAll(SystemP_WAIT_FOREVER);

    vTaskDelete(NULL);
}



void ipc_recv_task_main(void *args)
{
    int32_t status;
    char recvMsg[IPC_RPMESSAGE_MAX_MSG_SIZE+1]; /* +1 for NULL char in worst case */
    char sendingMsg[IPC_RPMESSAGE_MAX_MSG_SIZE+1]="HI,IT WORKS";
    uint16_t recvMsgSize, remoteCoreId, remoteCoreEndPt;
    RPMessage_Object *pRpmsgObj = (RPMessage_Object *)args;

    /* wait for messages forever in a loop */
    while(1)
    {
        /* set 'recvMsgSize' to size of recv buffer,
        * after return `recvMsgSize` contains actual size of valid data in recv buffer
        */
        recvMsgSize = IPC_RPMESSAGE_MAX_MSG_SIZE;
        status = RPMessage_recv(pRpmsgObj,
            recvMsg, &recvMsgSize,
            &remoteCoreId, &remoteCoreEndPt,
            SystemP_WAIT_FOREVER);


        /* echo the same message string as reply */
        #if 0 /* not logging this so that this does not add to the latency of message exchange */
        recvMsg[recvMsgSize] = 0; /* add a NULL char at the end of message */
        DebugP_log("%s\r\n", recvMsg);
        #endif

        /* send ack to sender CPU at the sender end point */
        status = RPMessage_send(
            sendingMsg, recvMsgSize,
            remoteCoreId, remoteCoreEndPt,
            RPMessage_getLocalEndPt(pRpmsgObj),
            SystemP_WAIT_FOREVER);

    }
    /* This loop will never exit */
}
