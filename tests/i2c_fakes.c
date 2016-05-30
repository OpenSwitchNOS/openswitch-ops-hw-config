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

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include "../include/config-yaml.h"

extern int ops_cnt;

int
i2c_execute(
    YamlConfigHandle handle,
    const char *subsyst,
    const YamlDevice *dev,
    i2c_op **cmds)
{
    int i = 0;

    if ((handle != (YamlConfigHandle) NULL) &&
                        (cmds != NULL) && (dev != NULL)) {
        while (cmds[i] != NULL) {
            i++;
        }
        ops_cnt += i;

        printf("i2c_fake: %d i2c_op commands sent.\n", i);
        printf("i2c_fake: Returning success for subsystem %s.\n", subsyst);
        return 0;
    } else {
        return -1;
    }
}

int
i2c_reg_read(YamlConfigHandle handle,
             const char *subsyst,
             const i2c_bit_op *reg_op,
             uint32_t *val)
{
    if ((handle != NULL) &&
        (subsyst != NULL) && (strlen(subsyst) != 0) &&
        (reg_op != NULL) &&
        (val != NULL)) {
        *val = 0;
        printf("%s_fake: read from register processed.\n", __func__);
        printf("%s_fake: Returning success for subsystem %s.\n",
               __func__, subsyst);
        return 0;
    } else {
        return -1;
    }
}

int
i2c_reg_write(YamlConfigHandle handle,
              const char *subsyst,
              const i2c_bit_op *reg_op,
              const uint32_t val)
{
    if ((handle != NULL) &&
        (subsyst != NULL) && (strlen(subsyst) != 0) &&
        (reg_op != NULL)) {
        printf("%s_fake: read from register processed.\n", __func__);
        printf("%s_fake: Returning success for subsystem %s.\n",
               __func__, subsyst);
        return 0;
    } else {
        return -1;
    }
}

int
i2c_data_read(YamlConfigHandle handle,
              const YamlDevice *device,
              const char *subsyst,
              const size_t offset,
              const size_t len,
              void *data)
{
    if ((handle != NULL) &&
        (device != NULL) &&
        (subsyst != NULL) && (strlen(subsyst) != 0) &&
        (len != 0) &&
        (data != NULL)) {
        printf("%s_fake: read from register processed.\n", __func__);
        printf("%s_fake: Returning success for subsystem %s.\n",
               __func__, subsyst);
        return 0;
    } else {
        return -1;
    }
}

int
i2c_data_write(YamlConfigHandle handle,
               const YamlDevice *device,
               const char *subsyst,
               const size_t offset,
               const size_t len,
               void *data)
{
    if ((handle != NULL) &&
        (device != NULL) &&
        (subsyst != NULL) && (strlen(subsyst) != 0) &&
        (len != 0) &&
        (data != NULL)) {
        printf("%s_fake: read from register processed.\n", __func__);
        printf("%s_fake: Returning success for subsystem %s.\n",
               __func__, subsyst);
        return 0;
    } else {
        return -1;
    }
}

int
i2c_do_op(YamlConfigHandle handle,
          const char *subsyst,
          i2c_op *op)
{
    if ((handle != NULL) &&
        (subsyst != NULL) && (strlen(subsyst) != 0) &&
        (op != NULL)) {
        printf("%s_fake: register operation processed.\n", __func__);
        printf("%s_fake: Returning success for subsystem %s.\n",
               __func__, subsyst);
        return 0;
    } else {
        return -1;
    }
}
