/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "fu-i2c-device.h"

#include <glib.h>
#include <glib/gstdio.h>

#define LAYOUT_FLAG_NAME "FLAG"
#define LAYOUT_PARTITION_NAME "PARTITION"
#define IMG_LAYOUT_NAME "layout"
#define IMG_FLAG1_NAME "flag1.bin"
#define IMG_FLAG2_NAME "flag2.bin"
#define FIRMWARE_BLOB_FORMAT "V[0-9]+.[0-9]+.bin$"

struct FlashromArgs
{
	const gchar *spi_master;
	const gchar *layout;
	const gchar *image;
	const gchar *operation;
};

typedef struct {
	FuDevice		parent_instance;
	FuI2cDeviceKind		kind;
	gint			bus_number;
	gint			update_block_number;
	gchar*			programmer_name;
} FuI2cDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (FuI2cDevice, fu_i2c_device, FU_TYPE_DEVICE)

#define GET_PRIVATE(o) (fu_i2c_device_get_instance_private (o))

void fu_i2c_device_set_kind (FuI2cDevice *self, FuI2cDeviceKind device_kind)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	priv->kind = device_kind;
}

FuI2cDeviceKind fu_i2c_device_get_kind (FuI2cDevice *self)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	return priv->kind;
}

void fu_i2c_device_set_bus_number (FuI2cDevice *self, gint bus_number)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	priv->bus_number = bus_number;
}

gint fu_i2c_device_get_bus_number (FuI2cDevice *self)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	return priv->bus_number;
}

void fu_i2c_device_set_update_block_number (FuI2cDevice *self, gint block_number)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	priv->update_block_number = block_number;
}

gint fu_i2c_device_get_update_block_number (FuI2cDevice *self)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	return priv->update_block_number;
}

void fu_i2c_device_set_programmer_name (FuI2cDevice *self, gchar *name)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);

	g_free (priv->programmer_name);
	priv->programmer_name = g_strdup (name);
}

gchar *fu_i2c_device_get_programmer_name (FuI2cDevice *self)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	return priv->programmer_name;
}

static gboolean fu_i2c_device_validate_flashrom_args (
	const struct FlashromArgs *args,
	GError **error)
{
	if (args->spi_master == NULL ||
	    args->layout == NULL ||
	    args->image == NULL ||
	    args->operation == NULL) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "%s: all arguments under FlashromArgs has \
			     to be set",
			     __func__);
		return FALSE;
	}

	return TRUE;
}

static gboolean fu_i2c_device_run_command (const struct FlashromArgs *args,
					   GError **error)
{
	if (fu_i2c_device_validate_flashrom_args (args, error) == FALSE)
		return FALSE;

	const gchar *argv[9] = { "flashrom",
				 "-p", args->spi_master,
				 "--layout", args->layout,
				 "--image", args->image,
				 args->operation,
				 NULL };
	return fu_common_spawn_sync (argv, NULL, NULL, 0, NULL, error);
}

static gboolean fu_i2c_file_readable (const gchar *path, GError **error)
{
	gboolean result = g_access (path, R_OK);
	if (result != 0) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "%s failed to access file %s",
			     __func__,
			     path);
	}

	return (result == 0);
}

static gboolean fu_i2c_match_firmware (const gchar *target, GError **error)
{
	GRegex *regex = NULL;
	gboolean result;

	regex = g_regex_new (FIRMWARE_BLOB_FORMAT, 0, 0, error);
	if (regex == NULL)
		return FALSE;

	result = g_regex_match_all_full (regex, target, -1, 0, 0, NULL, error);
	g_regex_unref (regex);
	return result;
}

static gchar *fu_i2c_file_find_fw_path (const gchar *search_dir, GError **error)
{
	GDir *dir;
	const gchar *ent_name;
	gchar *fw_path = NULL;

	dir = g_dir_open (search_dir, 0, error);
	if (dir == NULL) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "Failed to open directory: %s",
			     search_dir);
		return NULL;
	}

	while ((ent_name = g_dir_read_name (dir)) != NULL) {
		if (fu_i2c_match_firmware (ent_name, error)) {
			if (fw_path != NULL) {
				break;
			}

			fw_path = g_build_filename (search_dir, ent_name);
		}
	}

	if (fw_path == NULL) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "None or multiple firmware blobs under %s",
			     search_dir);
	}

	g_dir_close (dir);
	return fw_path;
}

static gboolean fu_i2c_device_write_firmware (FuDevice *device,
					      FuFirmware *firmware,
					      FwupdInstallFlags flags,
					      GError **error)
{
	gint block_number;
	gint bus_number = fu_i2c_device_get_bus_number (device);
	struct FlashromArgs flash_args_write_fw;
	struct FlashromArgs flash_args_write_flg;
	const gchar *programmer_name = fu_i2c_device_get_programmer_name (device);
	g_autoptr (GBytes) archive_bytes = NULL;
	g_autofree gchar *tmp_dir_name = NULL;
	g_autofree gchar *flash_spi_master_arg = NULL;
	g_autofree gchar *flash_write_fw_image_arg = NULL;
	g_autofree gchar *flash_write_flg_image_arg = NULL;
	g_autofree gchar *partition_name = NULL;
	g_autofree gchar *layout_file_path = NULL;
	g_autofree gchar *flag1_file_path = NULL;
	g_autofree gchar *flag2_file_path = NULL;
	g_autofree gchar *firmware_file_path = NULL;
	g_autofree gchar *flag_file_name = NULL;
	gboolean ret = TRUE;
	archive_bytes = fu_firmware_get_image_default_bytes (firmware, error);
	tmp_dir_name = g_strdup_printf ("/tmp/flashrom-i2c-%d-XXXXXX", bus_number);
	if (g_mkdtemp (tmp_dir_name) == NULL) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "%s failed to create tmp dir %s",
			     __func__,
			     tmp_dir_name);
		ret = FALSE;
		goto cleanup;
	}

	ret = fu_common_extract_archive (archive_bytes, tmp_dir_name, error);
	if (ret == FALSE) {
		ret = FALSE;
		goto cleanup;
	}

	layout_file_path = g_build_filename (
		tmp_dir_name,
		IMG_LAYOUT_NAME,
		NULL);
	flag1_file_path = g_build_filename (
		tmp_dir_name,
		IMG_FLAG1_NAME,
		NULL);
	flag2_file_path = g_build_filename (
		tmp_dir_name,
		IMG_FLAG2_NAME,
		NULL);

	firmware_file_path = fu_i2c_file_find_fw_path (tmp_dir_name, error);
	if (firmware_file_path == NULL) {
		ret = FALSE;
		goto cleanup;
	}

	if (fu_i2c_file_readable (layout_file_path, error) == FALSE ||
	    fu_i2c_file_readable (flag1_file_path, error) == FALSE ||
	    fu_i2c_file_readable (flag2_file_path, error) == FALSE ||
	    fu_i2c_file_readable (firmware_file_path, error) == FALSE) {
		ret = FALSE;
		goto cleanup;
	}

	flash_spi_master_arg = g_strdup_printf ("%s:bus=%d",
		programmer_name, bus_number);
	block_number = fu_i2c_device_get_update_block_number (device);
	partition_name = g_strdup_printf (
		"%s%d", LAYOUT_PARTITION_NAME, block_number);
	if (block_number == 1)
		flag_file_name = g_strdup (flag1_file_path);
	else
		flag_file_name = g_strdup (flag2_file_path);

	flash_write_fw_image_arg = g_strdup_printf ("%s:%s",
						    partition_name,
						    firmware_file_path);
	flash_write_flg_image_arg = g_strdup_printf ("%s:%s",
						     LAYOUT_FLAG_NAME,
						     flag_file_name);

	flash_args_write_fw.spi_master = flash_spi_master_arg;
	flash_args_write_fw.layout = layout_file_path;
	flash_args_write_fw.image = flash_write_fw_image_arg;
	flash_args_write_fw.operation = "-w";
	flash_args_write_flg.spi_master = flash_spi_master_arg;
	flash_args_write_flg.layout = layout_file_path;
	flash_args_write_flg.image = flash_write_flg_image_arg;
	flash_args_write_flg.operation = "-w";

	ret = fu_i2c_device_run_command (&flash_args_write_fw, error);
	if (ret == TRUE)
		ret = fu_i2c_device_run_command (&flash_args_write_flg, error);

cleanup:
	fu_common_rmtree (tmp_dir_name, NULL);
	return ret;
}

static gboolean fu_i2c_device_setup (FuI2cDevice *self, GError **error)
{
	g_autoptr (FuI2cDeviceReader) reader = NULL;
	struct FwVersionInfo info;
	FuI2cDeviceKind kind;
	gint bus_number;
	gint block_number;
	gint block_to_update;

	kind = fu_i2c_device_get_kind (self);
	if (kind == I2C_DEVICE_PS175)
		reader = g_object_new (FU_TYPE_I2C_DEVICE_READER_LSPCON, NULL);
	else {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_INTERNAL,
			     "unsupported device kind %d",
			     kind);
	}

	bus_number = fu_i2c_device_get_bus_number (self);
	block_number = fu_i2c_device_reader_get_boot_block (reader, bus_number, error);
	if (block_number == -1)
		return FALSE;

	block_to_update = fu_i2c_device_reader_get_target_block (reader,
		block_number, error);
	if (block_to_update == -1)
		return FALSE;

	fu_i2c_device_set_update_block_number (FU_DEVICE (self), block_to_update);
	fu_i2c_device_reader_get_version (reader, bus_number, block_number, &info, error);
	fu_device_set_version_format (FU_DEVICE (self),
				      info.fmt);
	fu_device_set_version (FU_DEVICE (self), info.version);
	return TRUE;
}

static void fu_i2c_device_finalize (GObject *object)
{
	FuI2cDevice *self = FU_I2C_DEVICE (object);
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	g_free (priv->programmer_name);
}

static void fu_i2c_device_init (FuI2cDevice *self)
{
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_INTERNAL);
	fu_device_add_flag (FU_DEVICE (self),
		FWUPD_DEVICE_FLAG_USABLE_DURING_UPDATE);
}

static void fu_i2c_device_class_init (FuI2cDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FuDeviceClass *klass_device = FU_DEVICE_CLASS (klass);
	object_class->finalize = fu_i2c_device_finalize;
	klass_device->write_firmware = fu_i2c_device_write_firmware;
	klass_device->setup = fu_i2c_device_setup;
}
