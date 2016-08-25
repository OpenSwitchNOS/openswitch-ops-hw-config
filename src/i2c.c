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

#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <endian.h>

#include <linux/i2c-dev-user.h>

#include "config-yaml.h"

static int
count_ops(i2c_op **ops)
{
    int count = 0;

    while (ops[count] != NULL) {
        count++;
    }

    return(count);
}

static int
count_post(YamlConfigHandle *handle, const char *subsyst, const YamlDevice *dev)
{
    int count = 0;

    if (dev == NULL || dev->post == NULL) {
        return(0);
    }

    count += count_ops(dev->post);

    if (count != 0) {
        dev = yaml_find_device(handle, subsyst, dev->post[0]->device);
        count += count_post(handle, subsyst, dev);
    }

    return(count);
}

static int
count_pre(YamlConfigHandle *handle, const char *subsyst, const YamlDevice *dev)
{
    int count = 0;

    if (dev == NULL || dev->pre == NULL) {
        return(0);
    }

    count += count_ops(dev->pre);

    if (count != 0) {
        dev = yaml_find_device(handle, subsyst, dev->pre[0]->device);
        count += count_pre(handle, subsyst, dev);
    }

    return(count);
}

static int
add_post(
    YamlConfigHandle *handle,
    const char *subsyst,
    const YamlDevice *dev,
    i2c_op **cmds,
    int idx)
{
    int i;

    const YamlDevice *post_dev;

    if (dev->post == NULL || dev->post[0] == NULL) {
        return(idx);
    }

    post_dev = yaml_find_device(handle, subsyst, dev->post[0]->device);

    for (i = 0; dev->post[i] != NULL; i++) {
        cmds[idx] = dev->post[i];
        idx++;
    }

    idx = add_post(handle, subsyst, post_dev, cmds, idx);

    return(idx);
}

static int
add_pre(
    YamlConfigHandle *handle,
    const char *subsyst,
    const YamlDevice *dev,
    i2c_op **cmds,
    int idx)
{
    int i;

    const YamlDevice *pre_dev;

    if (dev->pre == NULL || dev->pre[0] == NULL) {
        return(idx);
    }

    pre_dev = yaml_find_device(handle, subsyst, dev->pre[0]->device);

    idx = add_pre(handle, subsyst, pre_dev, cmds, idx);

    for (i = 0; dev->pre[i] != NULL; i++) {
        cmds[idx] = dev->pre[i];
        idx++;
    }

    return(idx);
}

static int
add_cmds(i2c_op **ops, i2c_op **cmds, int idx)
{
    int i;

    for (i = 0; ops[i] != NULL; i++) {
        cmds[idx] = ops[i];
        idx++;
    }

    return(idx);
}

static int
i2c_do_smbus_io(int fd, i2c_op *cmd, char *dev_type)
{
    int rc;
    uint32_t data;

    if (cmd->direction) {
        /* write */
        if (1 == cmd->byte_count) {
            data = (long)cmd->data[0];
            rc = i2c_smbus_write_byte_data(fd, (unsigned char)cmd->register_address, data);
        } else if (2 == cmd->byte_count) {
            data = (long)(*(unsigned short *)cmd->data);
            rc = i2c_smbus_write_word_data(fd, (unsigned char)cmd->register_address, data);
        } else {
            // NOT IMPLEMENTED
            return EINVAL;
        }
        if (rc < 0)
            rc = -errno;
    } else {
        /* read */
        if (1 == cmd->byte_count) {
            data = i2c_smbus_read_byte_data(fd, (unsigned char)cmd->register_address);
            if (data < 0)
                rc = -errno;
            else {
                cmd->data[0] = (unsigned char)data;
                rc = 0;
            }
        } else if (2 == cmd->byte_count) {
            data = i2c_smbus_read_word_data(fd, (unsigned char)cmd->register_address);
            if (data < 0)
                rc = -errno;
            else {
                *(unsigned short *)cmd->data = (unsigned short)data;
                rc = 0;
            }

        } else if (strcmp(dev_type, "eeprom")==0) {
            size_t remaining = cmd->byte_count;
            rc = 0;
            while (remaining != 0) {
                unsigned char *buffer;
                size_t count = remaining;
                size_t offset = (cmd->byte_count - remaining);

                rc = i2c_smbus_write_byte_data(fd, ((cmd->register_address + offset) >> 8) & 0x0ff, (cmd->register_address + offset) & 0x0ff);
                if (rc < 0) {
                    rc = -errno;
                    continue;
                }

                if (count > 1) {
                    count = 1;
                }

                buffer = cmd->data + offset;

                data = i2c_smbus_read_byte(fd);

                if (data < 0) {
                    rc = -errno;
                    break;
                }

                *buffer = (unsigned char)data;

                remaining -= count;
            }

        } else {
            size_t remaining = cmd->byte_count;
            rc = 0;
            while (remaining != 0) {
                unsigned char *buffer;
                size_t count = remaining;
                size_t offset = (cmd->byte_count - remaining);

                if (count > 1) {
                  count = 1;
                }
                buffer = cmd->data + offset;
                data = i2c_smbus_read_byte_data(fd,
                                                (unsigned char)cmd->register_address + offset);
                if (data < 0) {
                    rc = -errno;
                    break;
                }
                *buffer = (unsigned char)data;
                remaining -= count;
            }
        }
    }

    return rc;
}

static int
i2c_execute_i2c(
    YamlConfigHandle handle,
    const char *subsyst,
    const YamlDevice *dev,
    i2c_op **cmds)
{
    i2c_op **all_cmds;
    unsigned int count = 0;
    unsigned int idx = 0;
    char *bus_name;
    int fd = -1;
    unsigned int i;
    int rc;
    int final_rc;
    struct i2c_msg *msgbuf;
    struct i2c_rdwr_ioctl_data msgioctl;
    char *dev_name;
    char *dev_type;

    if (dev == NULL || handle == NULL) {
        return EINVAL;
    }

    if (cmds == NULL || cmds[0] == NULL) {
        return EINVAL;
    }

    dev_type = dev->dev_type;

    // OPS_TODO: need better i2c_op array functions to avoid
    // having to count operations before accumulating them.
    count += count_pre(handle, subsyst, dev);
    count += count_ops(cmds);
    count += count_post(handle, subsyst, dev);

    all_cmds = (i2c_op **)calloc(sizeof(i2c_op *), count);

    // OPS_TODO: need to look at bus to see if it crosses a subsystem boundary
    // and jump to the other subsystem (recursively) to pick up any pre and
    // post operations that may be required.
    idx = add_pre(handle, subsyst, dev, all_cmds, idx);
    idx = add_cmds(cmds, all_cmds, idx);
    idx = add_post(handle, subsyst, dev, all_cmds, idx);

    // verify that all operations are to the same bus
    // OPS_TODO: bus may change as we cross the boundary between subsystems

    dev = yaml_find_device(handle, subsyst, all_cmds[0]->device);

    if (dev == NULL) {
        free(all_cmds);
        return EINVAL;
    }

    bus_name = dev->bus;

    for (idx = 0; idx < count; idx++) {
        dev = yaml_find_device(handle, subsyst, all_cmds[idx]->device);
        if (strcmp(dev->bus, bus_name) != 0) {
            free(all_cmds);
            return EINVAL;
        }
    }

    const YamlBus *bus = yaml_find_bus(handle, subsyst, bus_name);

    if (bus == NULL) {
        free(all_cmds);
        return EINVAL;
    }

    dev_name = bus->devname;

    fd = open(dev_name, O_RDWR);

    if (fd < 0) {
        rc = errno;
        free(all_cmds);
        return rc;
    }

    rc = 0;
    final_rc = 0;


    flock(fd, LOCK_EX);

    if (!bus->smbus) {
        msgbuf = (struct i2c_msg *)calloc(sizeof(struct i2c_msg), count);

        msgioctl.nmsgs = count;
        msgioctl.msgs = msgbuf;

        for (i = 0; i < count; i++) {
            dev = yaml_find_device(handle, subsyst, all_cmds[i]->device);
            msgbuf[i].flags = all_cmds[i]->direction ? 0 : I2C_M_RD;
            msgbuf[i].len = all_cmds[i]->byte_count;
            msgbuf[i].addr = dev->address;
            msgbuf[i].buf = (char *)all_cmds[i]->data;
            idx++;
        }

        do {
            rc = ioctl(fd, I2C_RDWR, &msgioctl);
        } while (rc < 0 && EINTR == errno);

        if (rc < 0) {
            rc = errno;
            final_rc = rc;
        }

        free(msgbuf);
    } else {

        for (idx = 0; idx < count; idx++) {
            i2c_op *cmd = all_cmds[idx];
            dev = yaml_find_device(handle, subsyst, cmd->device);

            rc = ioctl(fd, I2C_SLAVE, (long)dev->address);

            if (rc < 0) {
                rc = errno;
                final_rc = rc;
                continue;
            }

            rc = i2c_do_smbus_io(fd, cmd, dev_type);
            if (rc < 0) {
                final_rc = rc;
                continue;
            }
        }
    }
    flock(fd, LOCK_UN);

    free(all_cmds);

    close(fd);

    return final_rc;
}

static int
i2c_execute_9541(
    YamlConfigHandle handle,
    const char *subsyst,
    const YamlDevice *dev,
    i2c_op **cmds)
{
    char *bus_name;
    char *dev_name;
    int data_rd;
    int data_wr;
    int fd = -1;
    int rc;
    i2c_op *cmd = cmds[0];

    bus_name = dev->bus;
    const YamlBus *bus = yaml_find_bus(handle, subsyst, bus_name);
    if (bus == NULL) {
        return EINVAL;
    }

    dev_name = bus->devname;
    fd = open(dev_name, O_RDWR);
    if (fd < 0) {
        rc = errno;
        return rc;
    }

    rc = ioctl(fd, I2C_SLAVE, (long)dev->address);
    data_rd = i2c_smbus_read_byte_data(fd, (unsigned char)cmd->register_address);

    switch(data_rd & 0x0f)
    {
        case 0x09:
        case 0x0c:
        case 0x0d:
            data_wr = 0x00;
            break;

        case 0x0A:
        case 0x0E:
        case 0x0F:
            data_wr = 0x01;
            break;

        case 0x00:
        case 0x01:
        case 0x05:
            data_wr = 0x04;
            break;

        case 0x02:
        case 0x03:
        case 0x06:
            data_wr = 0x05;
            break;
        default:
            data_wr = 0xff;
            break;
    }

    if (data_wr == 0xff)
        return 0;

    rc = ioctl(fd, I2C_SLAVE, (long)dev->address);
    rc = i2c_smbus_write_byte_data(fd, (unsigned char)cmd->register_address, data_wr);

    close(fd);
    return rc;
}

static int
i2c_execute_cpld(
    YamlConfigHandle handle,
    const char *subsyst,
    const YamlDevice *dev,
    i2c_op **cmds)
{
    char *bus_name;
    char *bus_dev_name;
    char *dev_dev_name;
    int fd = -1;
    int rc;
    bspCpuCpldRegData_t reg_ls;
    SfpData_t           sfp_ls;
    i2c_op *cmd = cmds[0];

    bus_name = dev->bus;
    const YamlBus *bus = yaml_find_bus(handle, subsyst, bus_name);
    if (bus == NULL) {
        return EINVAL;
    }

    dev_dev_name = dev->name;
    bus_dev_name = bus->devname;
    fd = open(bus_dev_name, O_RDWR);
    if (fd < 0) {
        rc = errno;
        return rc;
    }

    if (strcmp(dev_dev_name, "cpld_io")==0)
    {
       if (cmd->direction)
       {
          reg_ls.regId = cmd->register_address;
          reg_ls.val   = *(char *)cmd->data;
          reg_ls.rw    = 1;
          rc           = ioctl(fd, IOCTL_WRITE_REG, (char *)&reg_ls);
       }
       else
       {
          reg_ls.regId = cmd->register_address;
          reg_ls.val   = 0;
          reg_ls.rw    = 0;
          rc           = ioctl(fd, IOCTL_READ_REG, (char *)&reg_ls);
          *cmd->data   = *(unsigned char *)&reg_ls.val;
       }
    }
    else
    {
       if (cmd->direction)
       {
          sfp_ls.portId  = cmd->data[1];
          sfp_ls.devAddr = (dev->address<<1);
          sfp_ls.regId   = *(char *)&cmd->register_address;
          sfp_ls.rw      = 1;
          sfp_ls.len     = cmd->byte_count;

          memset(sfp_ls.val, 0, sizeof(sfp_ls.val));
          for (int i=0; i<cmd->byte_count; i++) sfp_ls.val[i] = *(char *)(cmd->data+i);
          rc             = ioctl(fd, IOCTL_SFP_WRITE, (char *)&sfp_ls);
       }
       else
       {
          sfp_ls.portId  = cmd->data[1];
          sfp_ls.devAddr = (dev->address<<1);
          sfp_ls.regId   = *(char *)&cmd->register_address;
          sfp_ls.rw      = 0;
          sfp_ls.len     = cmd->byte_count;

          memset(sfp_ls.val, 0, sizeof(sfp_ls.val));
          rc             = ioctl(fd, IOCTL_SFP_READ, (char *)&sfp_ls);
          for (int i=0; i<cmd->byte_count; i++) *(cmd->data+i) = *(unsigned char *)&sfp_ls.val[i];
        }
    }

    close(fd);
    return rc;
}

static int
i2c_execute(
    YamlConfigHandle handle,
    const char *subsyst,
    const YamlDevice *dev,
    i2c_op **cmds)
{
    char *bus_name;
    char *dev_type;
    bus_name = dev->bus;
    dev_type = dev->dev_type;

    if (strcmp(bus_name, "cpldev_1")==0)
      {
        return i2c_execute_cpld(handle, subsyst, dev, cmds);
      }
    else if (strcmp(dev_type, "pca9541")==0)
      {
        return i2c_execute_9541(handle, subsyst, dev, cmds);
      }
    else
      {
        return i2c_execute_i2c(handle, subsyst, dev, cmds);
      }
}

int
i2c_do_op(YamlConfigHandle handle,
          const char *subsyst,
          i2c_op *op)
{
    const YamlDevice *device;
    i2c_op *cmds[2];

    device = yaml_find_device(handle, subsyst, op->device);
    if (device == NULL) {
        return EINVAL;
    }
    cmds[0] = op;
    cmds[1] = NULL;

    return i2c_execute(handle, subsyst, device, cmds);
}

static int
i2c_reg_io(YamlConfigHandle handle,
           const char *subsyst,
           const bool direction,
           const i2c_bit_op *reg_op,
           uint32_t *val)
{
    int rc;
    uint8_t byte;
    uint16_t word;
    uint32_t dword;
    i2c_op op = {0};

    if (direction == WRITE) {
        uint32_t pre_data;

        /* Apply the polarity and mask */
        dword = *val;
        if (reg_op->negative_polarity)
            dword = ~dword;
        dword &= (uint32_t)reg_op->bit_mask;

        /* If partial register write, must do read/modify/write */
        if (((reg_op->register_size == 1) && (reg_op->bit_mask != 0xffu)) ||
            ((reg_op->register_size == 2) && (reg_op->bit_mask != 0xffffu)) ||
            ((reg_op->register_size == 4) && (reg_op->bit_mask != 0xffffffffu))) {
            i2c_bit_op read_reg_op = *reg_op;

            /* We want to read the entire register */
            read_reg_op.bit_mask = 0xffffffffu >> ((4 - reg_op->register_size) * 8);
            rc = i2c_reg_io(handle, subsyst, READ, &read_reg_op, &pre_data);
            if (rc)
                return rc;
            pre_data &= ~(uint32_t)reg_op->bit_mask;
            dword |= pre_data;
        }
    }

    op.direction        = direction;
    op.device           = reg_op->device;
    op.register_address = reg_op->register_address;
    op.byte_count       = reg_op->register_size;

    switch(reg_op->register_size) {
    case 1:
        if (direction == WRITE)
            byte = (unsigned char)dword;
        op.data = (uint8_t *)&byte;
        break;
    case 2:
        if (direction == WRITE)
          word = htobe16((uint16_t)dword);
        op.data = (uint8_t *)&word;
        break;
    case 4:
        if (direction == WRITE)
          dword = htobe32(dword);
        op.data = (uint8_t *)&dword;
        break;
    default:
        return EINVAL;
    }
    rc = i2c_do_op(handle, subsyst, &op);

    if ((rc >= 0) && (direction == READ)) {

        switch(reg_op->register_size) {
        case 1:
            dword = (uint32_t)byte;
            break;
        case 2:
            /* byte-swapping already done... */
            dword = (uint32_t)word;
            break;
        case 4:
            dword = be32toh(dword);
            break;
        default:
            return EINVAL;
        }

        /* Apply the polarity and mask */
        if (reg_op->negative_polarity)
            dword = ~dword;
        dword &= (uint32_t)reg_op->bit_mask;
        *val = dword;
    }

    return rc;
}

int
i2c_reg_read(YamlConfigHandle handle,
             const char *subsyst,
             const i2c_bit_op *reg_op,
             uint32_t *val)
{
    return i2c_reg_io(handle, subsyst, READ, reg_op, val);
}

int
i2c_reg_write(YamlConfigHandle handle,
              const char *subsyst,
              const i2c_bit_op *reg_op,
              const uint32_t val)
{
    uint32_t reg_data = val;

    return i2c_reg_io(handle, subsyst, WRITE, reg_op, &reg_data);
}

int
i2c_data_read(YamlConfigHandle handle,
              const YamlDevice *device,
              const char *subsyst,
              const size_t offset,
              const size_t len,
              void *data)
{
    i2c_op op = {0};

    op.direction = READ;
    op.device = device->name;
    op.register_address = offset;
    op.byte_count = len;
    op.data = data;

    return i2c_do_op(handle, subsyst, &op);
}

int
i2c_data_write(YamlConfigHandle handle,
               const YamlDevice *device,
               const char *subsyst,
               const size_t offset,
               const size_t len,
               void *data)
{
    i2c_op op = {0};

    op.direction = WRITE;
    op.device = device->name;
    op.register_address = offset;
    op.byte_count = len;
    op.data = data;

    return i2c_do_op(handle, subsyst, &op);
}
