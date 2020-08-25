/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once
#include <glib-object.h>
#include "fu-plugin.h"
#include "fu-i2c-device-reader.h"
#include "fu-i2c-device-reader-lspcon.h"

#define PORT_NAME "Port"
#define PROGRAMMER_NAME "Programmer"
#define DEVICE_NAME "Device"
#define DEVICE_PROTOCOL "Protocol"
#define DEVICE_VENDOR_NAME "VendorName"

#define FU_TYPE_I2C_DEVICE (fu_i2c_device_get_type ())
G_DECLARE_DERIVABLE_TYPE (FuI2cDevice, fu_i2c_device, FU,
	I2C_DEVICE, FuDevice)

typedef enum {
	I2C_DEVICE_PS175,
	I2C_DEVICE_RTD2142,
} FuI2cDeviceKind;

struct _FuI2cDeviceClass
{
	FuDeviceClass	parent_class;
};

void fu_i2c_device_set_kind	(FuI2cDevice *self,
				 FuI2cDeviceKind device_kind);
FuI2cDeviceKind	 fu_i2c_device_get_kind	(FuI2cDevice *self);
void fu_i2c_device_set_bus_number	(FuI2cDevice *self, gint bus_no);
gint fu_i2c_device_get_bus_number	(FuI2cDevice *self);
void fu_i2c_device_set_update_block_number	(FuI2cDevice *self, gint block_no);
gint fu_i2c_device_get_update_block_number	(FuI2cDevice *self);
void fu_i2c_device_set_programmer_name	(FuI2cDevice *self, gchar *name);
gchar *fu_i2c_device_get_programmer_name	(FuI2cDevice *self);
