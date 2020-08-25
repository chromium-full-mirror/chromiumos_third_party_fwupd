/*
 * This file is part of the flashrom project.
 *
 * Copyright (C) 2020 The Chromium OS Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <stdlib.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "i2c_helper.h"

#define I2C_DEV_PREFIX	"/dev/i2c-"
#define I2C_MAX_BUS	255

gint i2c_close (gint fd)
{
	return fd == -1 ? 0 : close (fd);
}

gint i2c_open (gint bus, guint16 addr, gint force, GError **error)
{
	gint ret = -1;
	gint fd = -1;

	/* Maximum i2c bus number is 255(3 char), +1 for null terminated string. */
	gint path_len = strlen (I2C_DEV_PREFIX) + 4;
	gint request = force ? I2C_SLAVE_FORCE : I2C_SLAVE;

	if (bus < 0 || bus > I2C_MAX_BUS) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Invalid I2C bus %d.",
			     bus);
		return ret;
	}

	gchar *dev = calloc (1, path_len);
	if (dev == NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Unable to allocate space for device name of len %d: %s.",
			     path_len, strerror (errno));
		goto linux_i2c_open_err;
	}

	ret = snprintf (dev, path_len, "%s%d", I2C_DEV_PREFIX, bus);
	if (ret < 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Unable to join bus number to device name: %s.",
			     strerror (errno));
		goto linux_i2c_open_err;
	}

	fd = open (dev, O_RDWR);
	if (fd < 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Unable to open I2C device %s: %s.",
			     dev, strerror (errno));
		ret = fd;
		goto linux_i2c_open_err;
	}

	ret = ioctl (fd, request, addr);
	if (ret < 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Unable to set I2C slave address to 0x%02x: %s.\n",
			     addr, strerror (errno));
		i2c_close (fd);
		goto linux_i2c_open_err;
	}

linux_i2c_open_err:
	if (dev)
		free (dev);

	return ret ? ret : fd;
}

gint i2c_read (gint fd, guint16 addr, i2c_buffer_t *buf, GError **error)
{
	if (buf->len == 0)
		return 0;

	gint ret = ioctl (fd, I2C_SLAVE, addr);
	if (ret < 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Unable to set I2C slave address to 0x%02x: %s.\n",
			     addr, strerror (errno));
		return ret;
	}

	ret = read (fd, buf->buf, buf->len);
	if (ret < 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "I2C read failed on slave address 0x%02x: %s",
			     addr, strerror (errno));
	}

	return ret;
}

gint i2c_write (gint fd, guint16 addr, const i2c_buffer_t *buf, GError **error)
{
	if (buf->len == 0)
		return 0;

	gint ret = ioctl (fd, I2C_SLAVE, addr);
	if (ret < 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Unable to set I2C slave address to 0x%02x: %s.\n",
			     addr, strerror (errno));
		return ret;
	}

	ret = write (fd, buf->buf, buf->len);
	if (ret < 0) {
                g_set_error (error,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "I2C write failed on slave address 0x%02x: %s.\n",
                             addr, strerror (errno));
        }

	return ret;
}
