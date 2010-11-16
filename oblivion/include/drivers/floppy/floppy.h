#ifndef FLOPPY
#define FLOPPY

void send_data(int d);
int get_data();
int on_command();
int busy();
int req();
void set_floppy(int flags);
void set_data_rate(int flags);
int disk_change();

#define DRIVE_ENABLE 4
#define DMA_ENABLE	8
#define MOTOR_ON 16

#endif
