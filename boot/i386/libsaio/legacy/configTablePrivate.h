/* 	Copyright (c) 1993 NeXT Computer, Inc.  All rights reserved. 
 *
 * configTablePrivate.h - private defintions for configTable mechanism.
 *
 * HISTORY
 * 28-Jan-93    Doug Mitchell at NeXT
 *      Created.
 */

/*
 * Max size fo config data array, in bytes.
 */
#define IO_CONFIG_DATA_SIZE		4096

/*
 * Location of driver and system config table bundles.
 */ 
#define IO_CONFIG_DIR 	"/usr/Devices/"

/*
 * File names and extensions.
 */
#define IO_BUNDLE_EXTENSION		".config"
#define IO_TABLE_EXTENSION		".table"
#define IO_DEFAULT_TABLE_FILENAME	"Default.table"
#define IO_INSPECTOR_FILENAME		"Inspector.nib"
#define IO_SYSTEM_CONFIG_FILE 	"/usr/Devices/System.config/Instance0.table"
#define IO_SYSTEM_CONFIG_DIR 	"/usr/Devices/System.config/"
#define IO_BINARY_EXTENSION		"_reloc"

