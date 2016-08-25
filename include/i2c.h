/*
 * (c) Copyright 2015 Hewlett Packard Enterprise Development LP
 *
 *    Licensed under the Apache License, Version 2.0 (the "License"); you may
 *    not use this file except in compliance with the License. You may obtain
 *    a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *    License for the specific language governing permissions and limitations
 *    under the License.
 */

#ifndef _I2C_H_
#define _I2C_H_

#ifndef __cplusplus
#include <stdbool.h>
#endif

#define WRITE   true
#define READ    false

#define CPLD_TYPE														( 0x89 )
#define IOCTL_READ_REG                      _IOR(CPLD_TYPE,0x05,bspCpuCpldRegData_t)
#define IOCTL_WRITE_REG                     _IOW(CPLD_TYPE,0x08,bspCpuCpldRegData_t)
#define IOCTL_SFP_READ                      _IOR(CPLD_TYPE,0x0F,bspCpuCpldRegData_t)
#define IOCTL_SFP_WRITE                     _IOW(CPLD_TYPE,0x10,bspCpuCpldRegData_t)

typedef struct cpldRegData
{
    unsigned short regId;		/*register number*/
    char val;		    		/*register value*/
    unsigned char rw;			/*0:read 1:write*/
}bspCpuCpldRegData_t;

typedef struct SfpData
{
	char regId;
	int portId;
	char devAddr;
	char val[256];
	unsigned char rw;
	unsigned char len;
}SfpData_t;

typedef struct {
    bool            direction;
    char            *device;
    int             byte_count;
    bool            set_register;
    unsigned short  register_address;
    unsigned char   *data;
    bool            negative_polarity;
} i2c_op;

typedef struct {
    char            *device;
    unsigned short  register_address;
    unsigned char   register_size;      // 1, 2, or 4 byte register
    unsigned char   bit_mask;
    bool            negative_polarity;
} i2c_bit_op;

#endif
