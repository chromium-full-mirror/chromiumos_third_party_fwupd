#include "i2c_helper.h"
#include "fu-i2c-device-reader-lspcon.h"

#define REGISTER_ADDRESS	(0x94 >> 1)
#define PAGE_ADDRESS		(0x9e >> 1)
#define ACTIVE_BLOCK_ADDRESS	(0x9a >> 1)
#define MPU			0xbc
#define ROMADDR_BYTE1		0x8e
#define ROMADDR_BYTE2		0x8f

struct _FuI2cDeviceReaderLspcon
{
	GObject		parent_instance;
};

G_DEFINE_TYPE_WITH_CODE (FuI2cDeviceReaderLspcon, fu_i2c_device_reader_lspcon, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (FU_TYPE_I2C_DEVICE_READER,
						fu_i2c_device_reader_interface_init))

static gint lspcon_i2c_spi_write_data (gint fd, guint16 addr, void *buf, guint16 len, GError **error)
{
	i2c_buffer_t data;
	if (i2c_buffer_t_fill (&data, buf, len))
		return -1;

	return i2c_write (fd, addr, &data, error) == len ? 0 : -1;
}

static gint lspcon_i2c_spi_read_data (gint fd, guint16 addr, void *buf, guint16 len, GError **error)
{
	i2c_buffer_t data;
	if (i2c_buffer_t_fill (&data, buf, len))
		return -1;

	return i2c_read (fd, addr, &data, error) == len ? 0 : -1;
}

static gint lspcon_i2c_spi_write_register (gint fd, guint8 i2c_register, guint8 value, GError **error)
{
	guint8 register_data[] = { i2c_register, value };
	return lspcon_i2c_spi_write_data (fd, REGISTER_ADDRESS, register_data, 2, error);
}


static gint get_i2c_device_boot_block (gint bus_no, GError **error)
{
	gint ret = 0;
	guint16 block_id = 0;
	gint fd = i2c_open (bus_no, REGISTER_ADDRESS, 0, error);
	guint8 register_addr[] = { 0x0e };
	ret |= lspcon_i2c_spi_write_data (fd, ACTIVE_BLOCK_ADDRESS, register_addr, 1, error);
	ret |= lspcon_i2c_spi_read_data (fd, ACTIVE_BLOCK_ADDRESS, &block_id, 1, error);
	if (ret) {
		g_prefix_error (error,
				"Failed to get LSPCON boot block information.");
		block_id = -1;
	}

	i2c_close (fd);
	return block_id;
}

/* Update should happen on the non-activated block. If neither block is
 * activated, force update to block 1.
 */
static gint get_i2c_device_target_block (gint boot_block_no)
{
	if (boot_block_no == 1)
		return 2;
	return 1;
}

static void get_i2c_device_firmware_version (gint bus_no,
				      gint boot_block,
				      struct FwVersionInfo *info,
				      GError **error)
{
	gint fd = i2c_open (bus_no, REGISTER_ADDRESS, 0, error);
	guint8 index = boot_block;
	guint8 versions[3];
	gint ret = 0;

	ret |= lspcon_i2c_spi_write_register (fd, ROMADDR_BYTE1, 0x50, error);
	ret |= lspcon_i2c_spi_write_register (fd, ROMADDR_BYTE2, index, error);
	ret |= lspcon_i2c_spi_read_data (fd, PAGE_ADDRESS, versions, 3, error);
	if (ret) {
		g_prefix_error (error,
				"Failed to get LSPCON firmware version.");
	}

	i2c_close (fd);
	info->fmt = FWUPD_VERSION_FORMAT_PAIR;
	info->version = g_strdup_printf ("%d.%d", versions[0], versions[2]);
}

static void fu_i2c_device_reader_lspcon_init (FuI2cDeviceReaderLspcon *self)
{
}

static void fu_i2c_device_reader_lspcon_class_init (FuI2cDeviceReaderLspconClass *klass)
{
}

static void fu_i2c_device_reader_interface_init (FuI2cDeviceReaderInterface *iface)
{
	iface->get_boot_block = get_i2c_device_boot_block;
	iface->get_target_block = get_i2c_device_target_block;
	iface->get_version = get_i2c_device_firmware_version;
}
