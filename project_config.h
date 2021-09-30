/*
 * configurations.h
 *
 *  Created on: Sep 28, 2021
 *      Author: Aaron.renny
 */

#ifndef CONFIGURATIONS_H_
#define CONFIGURATIONS_H_


#define MAIN_TASK_PRI  (configMAX_PRIORITIES-1)

#define MAIN_TASK_SIZE (16384U/sizeof(configSTACK_DEPTH_TYPE))

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

/* Task priority, stack, stack size and task objects, these MUST be global's */
#define IPC_RPMESSAFE_TASK_PRI         (8U)
#define IPC_RPMESSAFE_TASK_STACK_SIZE  (8*1024U)

#endif /* CONFIGURATIONS_H_ */
