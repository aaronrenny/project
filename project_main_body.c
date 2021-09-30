#include"aaron.h"


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



StackType_t gMainTaskStack[MAIN_TASK_SIZE] __attribute__((aligned(32)));


StaticTask_t gMainTaskObj;
TaskHandle_t gMainTask;

void  frertos_main(void *args);
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



    return 0;
}
