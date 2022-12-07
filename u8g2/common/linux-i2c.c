#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <u8x8.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFSIZ_I2C 32

char filename[255];
uint8_t data[BUFSIZ_I2C]; // just to be sure
int idx = 0;
// almost certainly the wrong place for this state!

typedef struct {
    int file;
} LinuxI2cPrivate_t;

uint8_t
u8x8_byte_linux_i2c(u8x8_t *u8x8,
		    uint8_t msg,
		    uint8_t arg_int,
		    void *arg_ptr)
{
	switch(msg){
	case U8X8_MSG_BYTE_SEND:
		//fprintf(stderr, "-- %d bytes:\n", arg_int);
		for(int i = 0; i < arg_int && idx < BUFSIZ_I2C; i++, idx++){
			data[idx] = *(uint8_t *)(arg_ptr+i);
			//fprintf(stderr, "    %d  %d:  %0x\n", i, idx, data[idx]);
		}
		break;
	case U8X8_MSG_BYTE_INIT:
	{
                //TODO: ensure we free resources if this is a repeated init
                //TOOD: add cleanup
                LinuxI2cPrivate_t* ptr = calloc(1, sizeof(LinuxI2cPrivate_t));
                if(!ptr) {
                    fprintf(stderr, "Cannot allocate memory for LinuxI2cPrivate_t\n");
                    return 1;
                }
                u8x8->private_state = ptr;
		int addr = u8x8_GetI2CAddress(u8x8);
		// ths open/setup? it seems to be a one-time setup
		snprintf(filename, 19, "/dev/i2c-%d", u8x8_GetI2CBus(u8x8));
		int file = open(filename, O_RDWR);
		if (file < 0) {
			fprintf(stderr, "can't open i2c\n");
			return(errno); 
		}
                ptr->file = file;
		fprintf(stderr, "opened i2c file %d\n", file);
		if (ioctl(file, I2C_SLAVE, addr) < 0) { // u8x8_GetI2CAddress(u8x8)
			fprintf(stderr, "can't set addr %0x\n", addr);
			return(errno);
		}
		fprintf(stderr, "set i2c addr %0x\n", addr);
		break;
	}
	case U8X8_MSG_BYTE_SET_DC:
		/* ignored for i2c */
		//fprintf(stderr, "++ set dc?\n");
		break;
	case U8X8_MSG_BYTE_START_TRANSFER:
		//fprintf(stderr, "++ start transfer, resetting buffers\n");
		memset(data, 0, BUFSIZ_I2C);
		idx = 0;
		break;
	case U8X8_MSG_BYTE_END_TRANSFER:
		//fprintf(stderr, "++ end transfer, sending cmd %0x %0x count %d\n", data[0], data[1], idx);
		// NB! note the extre _i2c_ in there! leave that out and you are screwed
		errno = 0;
               LinuxI2cPrivate_t* ptr = u8x8->private_state;
		if (write(ptr->file, data, idx) != idx) {
		//if (i2c_smbus_write_i2c_block_data(file, data[0], idx - 1, &data[1]) < 0) {
			fprintf(stderr, "can't write cmd %0x: %s\n", data[0], strerror(errno));
			return(errno); 
		}
		break;
	default:
		fprintf(stderr, "unknown msg type %d\n", msg);
		return 1;
	}
	return 0;
}


uint8_t
u8x8_linux_i2c_delay(u8x8_t *u8x8,
		     uint8_t msg,
		     uint8_t arg_int,
		     void *arg_ptr)
{
	struct timespec req;
	struct timespec rem;
	int ret;

	req.tv_sec = 0;
	
	switch(msg) {
	case U8X8_MSG_DELAY_NANO:  // delay arg_int * 1 nano second
		req.tv_nsec = arg_int;
		break;
	case U8X8_MSG_DELAY_100NANO:       // delay arg_int * 100 nano seconds
		req.tv_nsec = arg_int * 100;
		break;
	case U8X8_MSG_DELAY_10MICRO: // delay arg_int * 10 micro seconds
		req.tv_nsec = arg_int * 10000;
		break;
	case U8X8_MSG_DELAY_MILLI:  // delay arg_int * 1 milli second
		req.tv_nsec = arg_int * 1000000;
		break;
	default:
		return 0;
	}

	while((ret = nanosleep(&req, &rem)) && errno == EINTR){
		struct timespec tmp = req;
		req = rem;
		rem = tmp;
	}
	if (ret) {
		perror("nanosleep");
		fprintf(stderr, "can't sleep\n");
		return(errno);
	}
	
	return 1;
}
