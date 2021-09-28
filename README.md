# project
A trial project
A sample code taken from TI MCU+SDK echo_linux_example.
This basically enables communication between Linux core(A53) and FreeRTOS core(R5F). A message is send from the linux core which is then echoed back to the R5F core.
This code has been simplified to include only the above functionality. The original example code provided by TI will differ.

This code can be run from the linux core by typing :
echo stop > /sys/class/remoteproc/remoteproc0/state
echo start > /sys/class/remoteproc/remoteproc0/state
 and then
rpmsg_char_simple [-r <rproc_id>] [-n <num_msgs>] [-d <rpmsg_dev_name] [-p <remote_endpt]
