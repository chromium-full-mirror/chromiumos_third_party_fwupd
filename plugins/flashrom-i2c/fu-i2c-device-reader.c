#include "fu-i2c-device-reader.h"

G_DEFINE_INTERFACE (FuI2cDeviceReader, fu_i2c_device_reader, G_TYPE_OBJECT)

static void fu_i2c_device_reader_default_init (FuI2cDeviceReaderInterface *iface)
{
}

gint fu_i2c_device_reader_get_boot_block (FuI2cDeviceReader *self,
					  gint bus_no,
					  GError **error)
{
	FuI2cDeviceReaderInterface *reader_interface;

	g_return_val_if_fail (error == NULL || *error == NULL, -1);

	reader_interface = FU_I2C_DEVICE_READER_GET_IFACE (self);
	if (reader_interface->get_boot_block == NULL) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_NOT_SUPPORTED,
				     "not supported");
		return -1;
	}

	return reader_interface->get_boot_block (bus_no, error);
}

gint fu_i2c_device_reader_get_target_block (FuI2cDeviceReader *self,
					    gint boot_block_no,
					    GError **error)
{
	FuI2cDeviceReaderInterface *reader_interface;

	g_return_val_if_fail (error == NULL || *error == NULL, -1);

	reader_interface = FU_I2C_DEVICE_READER_GET_IFACE (self);
	if (reader_interface->get_target_block == NULL) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_NOT_SUPPORTED,
				     "not supported");
		return -1;
	}

	return reader_interface->get_target_block (boot_block_no);
}

void fu_i2c_device_reader_get_version (FuI2cDeviceReader *self,
				       gint bus_no,
				       gint block,
				       struct FwVersionInfo *info,
				       GError **error)
{
	FuI2cDeviceReaderInterface *reader_interface;

	g_return_if_fail (error == NULL || *error == NULL);

	reader_interface = FU_I2C_DEVICE_READER_GET_IFACE (self);
	if (reader_interface->get_version == NULL) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_NOT_SUPPORTED,
				     "not supported");
		return;
	}
	reader_interface->get_version (bus_no, block, info, error);
}
