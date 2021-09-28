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

#define MAIN_TASK_PRI  (configMAX_PRIORITIES-1)

#define MAIN_TASK_SIZE (16384U/sizeof(configSTACK_DEPTH_TYPE))
StackType_t gMainTaskStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));


/* The service names MUST match what linux is expecting
*/
/* This is used to run the echo test with linux kernel */
#define IPC_RPMESSAGE_SERVICE_PING        "ti.ipc4.ping-pong"
#define IPC_RPMESSAGE_ENDPT_PING          (13U)

/* This is used to run the echo test with user space kernel */
#define IPC_RPMESSAGE_SERVICE_CHRDEV      "rpmsg_chrdev"
#define IPC_RPMESSAGE_ENDPT_CHRDEV_PING   (4U)

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

/* RPMessage object used to recvice messages */
RPMessage_Object gIpcRecvMsgObject[IPC_RPMESSAGE_NUM_RECV_TASKS];

/* RPMessage object used to send messages to other non-Linux remote cores */
RPMessage_Object gIpcAckReplyMsgObject;

/* Task priority, stack, stack size and task objects, these MUST be global's */
#define IPC_RPMESSAFE_TASK_PRI         (8U)
#define IPC_RPMESSAFE_TASK_STACK_SIZE  (8*1024U)
uint8_t gIpcTaskStack[IPC_RPMESSAGE_NUM_RECV_TASKS][IPC_RPMESSAFE_TASK_STACK_SIZE] __attribute__((aligned(32)));
TaskP_Object gIpcTask[IPC_RPMESSAGE_NUM_RECV_TASKS];



StaticTask_t gMainTaskObj;
TaskHandle_t gMainTask;

void ipc_recv_task_main(void *args);
//void ipc_rpmsg_create_recv_tasks(void);
void frertos_main(void *args);


int main()
{
    /* init SOC specific modules */
    System_init();
    Board_init();

    /* This task is created at highest priority, it should create more tasks and then delete itself */
    gMainTask = xTaskCreateStatic( frertos_main,   /* Pointer to the function that implements the task. */
                                  "freertos_main", /* Text name for the task.  This is to facilitate debugging only. */
                                  MAIN_TASK_SIZE,  /* Stack depth in units of StackType_t typically uint32_t on 32b CPUs */
                                  NULL,            /* We are not using the task parameter. */
                                  MAIN_TASK_PRI,   /* task priority, 0 is lowest priority, configMAX_PRIORITIES-1 is highest */
                                  gMainTaskStack,  /* pointer to stack base */
                                  &gMainTaskObj ); /* pointer to statically allocated task object memory */
    configASSERT(gMainTask != NULL);

    /* Start the scheduler to start the tasks executing. */
    vTaskStartScheduler();

    /* The following line should never be reached because vTaskStartScheduler()
    will only return if there was not enough FreeRTOS heap memory available to
    create the Idle and (if configured) Timer tasks.  Heap management, and
    techniques for trapping heap exhaustion, are described in the book text. */
    DebugP_assertNoLog(0);

    return 0;
}

void frertos_main(void *args)
{
    int32_t status;
    //int32_t status;

    RPMessage_CreateParams createParams;
    TaskP_Params taskParams;

    //Drivers_open();
    //Board_driversOpen();




        DebugP_log("[IPC RPMSG ECHO] %s %s\r\n", __DATE__, __TIME__);

        /* This API MUST be called by applications when its ready to talk to Linux */
        status = RPMessage_waitForLinuxReady(SystemP_WAIT_FOREVER);
        DebugP_assert(status==SystemP_SUCCESS);

        RPMessage_CreateParams_init(&createParams);
                  createParams.localEndPt = IPC_RPMESSAGE_ENDPT_CHRDEV_PING;
                  status = RPMessage_construct(&gIpcRecvMsgObject[1], &createParams);
                  DebugP_assert(status==SystemP_SUCCESS);

      /* We need to "announce" to Linux client else Linux does not know a service exists on this CPU
                       * This is not mandatory to do for RTOS clients               */

        status = RPMessage_announce(CSL_CORE_ID_A53SS0_0, IPC_RPMESSAGE_ENDPT_CHRDEV_PING, IPC_RPMESSAGE_SERVICE_CHRDEV);
        DebugP_assert(status==SystemP_SUCCESS);


                  /* create message receive tasks, these tasks always run and never exit */
        //ipc_rpmsg_create_recv_tasks();

        TaskP_Params_init(&taskParams);
            taskParams.name = "RPMESSAGE_CHAR_PING";
            taskParams.stackSize = IPC_RPMESSAFE_TASK_STACK_SIZE;
            taskParams.stack = gIpcTaskStack[1];
            taskParams.priority = IPC_RPMESSAFE_TASK_PRI;
            /* we use the same task function for echo but pass the appropiate rpmsg handle to it, to echo messages */
            taskParams.args = &gIpcRecvMsgObject[1];
            taskParams.taskMain = ipc_recv_task_main;

            status = TaskP_construct(&gIpcTask[1], &taskParams);
            DebugP_assert(status == SystemP_SUCCESS);







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

    DebugP_log("[IPC RPMSG ECHO] Remote Core waiting for messages at end point %d ... !!!\r\n",
        RPMessage_getLocalEndPt(pRpmsgObj)
        );

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
        DebugP_assert(status==SystemP_SUCCESS);

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
        DebugP_assert(status==SystemP_SUCCESS);
    }
    /* This loop will never exit */
}








