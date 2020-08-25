#pragma once

#include "i2c_helper.h"
#include "fu-device.h"

#include <glib-object.h>

#define FU_TYPE_I2C_DEVICE_READER		( \
	fu_i2c_device_reader_get_type ())
#define FU_I2C_DEVICE_READER_GET_IFACE(obj)	( \
	G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
	FU_TYPE_I2C_DEVICE_READER, FuI2cDeviceReaderInterface))


G_DECLARE_INTERFACE (FuI2cDeviceReader, fu_i2c_device_reader, FU,
	I2cDeviceReader, GObject)

struct FwVersionInfo {
	FwupdVersionFormat fmt;
	gchar *version;
};

struct _FuI2cDeviceReaderInterface
{
	GTypeInterface		parent_iface;
	gint			(*get_boot_block)	(gint bus_no,
							 GError **error);
	gint			(*get_target_block)	(gint boot_block_no);
	void			(*get_version)		(gint bus_no,
							 gint block,
							 struct FwVersionInfo *info,
							 GError **error);
};

gint fu_i2c_device_reader_get_boot_block (FuI2cDeviceReader *self,
					  gint bus_no,
					  GError **error);

gint fu_i2c_device_reader_get_target_block (FuI2cDeviceReader *self,
					    gint boot_block_no,
					    GError **error);

void fu_i2c_device_reader_get_version (FuI2cDeviceReader *self,
				       gint bus_no,
				       gint block,
				       struct FwVersionInfo *info,
				       GError **error);
