/*
 * This file is part of the KNOT Project
 *
 * Copyright (c) 2020, CESAR. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <ell/ell.h>

#include "settings.h"
#include "device.h"

static void signal_handler(uint32_t signo, void *user_data)
{
	switch (signo) {
	case SIGINT:
	case SIGTERM:
		l_info("Terminate");
		l_main_quit();
		break;
	}
}

static int detach_daemon(void)
{
	if (daemon(0, 0))
		return -errno;

	return 0;
}

static void set_device_settings(struct device_settings *conf_files,
				struct settings *settings)
{
	conf_files->credentials_path = l_strdup(settings->credentials_path);
	conf_files->device_path = l_strdup(settings->device_path);
	conf_files->rabbitmq_path = l_strdup(settings->rabbitmq_path);
}

static void free_device_settings(struct device_settings *conf_files)
{
	l_free(conf_files->credentials_path);
	l_free(conf_files->device_path);
	l_free(conf_files->rabbitmq_path);
	l_free(conf_files);
}

int main(int argc, char *argv[])
{
	struct settings *settings;
	struct device_settings *conf_files;
	int err;

	settings = settings_load(argc, argv);
	if (settings == NULL)
		return EXIT_FAILURE;

	if (settings->help) {
		settings_free(settings);
		return EXIT_SUCCESS;
	}

	if (!l_main_init()) {
		settings_free(settings);
		return EXIT_FAILURE;
	}

	l_log_set_stderr();

	conf_files = l_new(struct device_settings, 1);
	set_device_settings(conf_files, settings);

	err = device_start(conf_files);
	if (err) {
		l_error("Failed to start the device: %s (%d). Exiting...",
			strerror(-err), -err);
		l_main_exit();
		settings_free(settings);
		free_device_settings(conf_files);
		return EXIT_FAILURE;
	}
	free_device_settings(conf_files);

	if (settings->detach) {
		err = detach_daemon();
		if (err) {
			l_error("Failed to detach. %s (%d). Exiting...",
				strerror(-err), -err);
			settings_free(settings);
			l_main_exit();
			return EXIT_FAILURE;
		}
	}

	settings_free(settings);

	l_main_run_with_signal(signal_handler, NULL);

	device_destroy();

	l_main_exit();

	return EXIT_SUCCESS;
}
