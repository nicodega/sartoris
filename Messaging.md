# Messages #

The kernel provides an asynchronous inter-task message system through the `send_msg`, `get_msg` and `get_msg_count` system calls. Messages have a fixed size defined by the implementation (currently 4-bytes), and the kernel guarantees that messages arrive in FIFO order, but each message queue has a maximum capacity which is also implementation-defined.

Messages can be Sent to Ports. Each task is allowed to open ports numbered from 0 to a maximum value which is implementation defined.

When a message is sent using the function `send_msg(int to_task, int port, void *msg)`, the contents of the message pointed by `msg` are copied to the queue for port number `port` of task `to_task`.

The receiving of messages is done in an analogous fashion using the function `get_msg(int port, void *msg, int *id)`, which removes the fist message in the supplied Port's queue and copies it's contents to the address `msg`. The variable `id` is used to return the id of the sender task. Both functions return 0 on success.

The function `get_msg_count(int port)` returns the amount of messages waiting in the queue of the supplied port.

Before a port can be used to receive messages, it must be opened using the `open_port` System Call, setting it's protection mode (see below) and the numerically higher privilege level that can send messages to this port.

When a port is closed using the `close_port` system call, the associated message queue is flushed and the port becomes available to be used again. The amount of simultaneous ports a task can open at any given time is defined by the constant `MAX_TSK_OPEN_PORTS` located in the header file `/include/sartoris/message.h`.

# Port Permissions #

In order to provide a secure messaging system, Sartoris defines the following port protection modes:

  * **`PERM_REQ`**: When a port's protection mode is set to `PERM_REQ`, a task can send messages to this port if it has been authorized through the `set_port_perm` system call, and it's privilege level is numerically lower or equal than the port's privilege level.

  * **`PRIV_LEVEL_ONLY`**: The privilege level of the task sending the message must be numerically lower or equal than the port's privilege level.

  * **`DISABLED`**: If a task wishes to deny access to a port, this is the protection mode it ought to have.

The protection mode for a port can be modified using the `set_port_mode` system call. Permissions can be granted (or removed) to individual tasks using the `set_port_perm` system call.

Creation and deletion of ports is done with the `create_port`, `delete_port` and `delete_task_ports` functions.