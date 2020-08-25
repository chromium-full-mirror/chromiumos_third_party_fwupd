#pragma once

#include "fu-i2c-device-reader.h"

#define FU_TYPE_I2C_DEVICE_READER_LSPCON (fu_i2c_device_reader_lspcon_get_type ())

static void fu_i2c_device_reader_interface_init (FuI2cDeviceReaderInterface *iface);
G_DECLARE_FINAL_TYPE (FuI2cDeviceReaderLspcon, fu_i2c_device_reader_lspcon, FU, I2C_DEVICE_READER_LSPCON, GObject)
