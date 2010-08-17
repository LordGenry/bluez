/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010  Nokia Corporation
 *  Copyright (C) 2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <arpa/inet.h>

#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <glib.h>

#include "sdpd.h"
#include "log.h"
#include "attrib-server.h"

#include "att.h"
#include "example.h"

#define ATT_PSM 27

/* FIXME: Not defined by SIG? UUID128? */
#define OPCODES_SUPPORTED_UUID          0xA001
#define BATTERY_STATE_SVC_UUID		0xA002
#define BATTERY_STATE_UUID		0xA003
#define THERM_HUMIDITY_SVC_UUID		0xA004
#define MANUFACTURER_SVC_UUID		0xA005
#define TEMPERATURE_UUID		0xA006
#define FMT_CELSIUS_UUID		0xA007
#define FMT_OUTSIDE_UUID		0xA008
#define RELATIVE_HUMIDITY_UUID		0xA009
#define FMT_PERCENT_UUID		0xA00A
#define BLUETOOTH_SIG_UUID		0xA00B
#define MANUFACTURER_NAME_UUID		0xA00C
#define MANUFACTURER_SERIAL_UUID	0xA00D
#define VENDOR_SPECIFIC_SVC_UUID	0xA00E
#define VENDOR_SPECIFIC_TYPE_UUID	0xA00F

static uint32_t handle = 0;

static sdp_record_t *server_record_new(void)
{
	sdp_list_t *svclass_id, *apseq, *proto[2], *profiles, *root, *aproto;
	uuid_t root_uuid, proto_uuid, gatt_uuid, l2cap;
	sdp_profile_desc_t profile;
	sdp_record_t *record;
	sdp_data_t *psm, *sh, *eh;
	uint16_t lp = ATT_PSM, start = 0x0001, end = 0x000f;

	record = sdp_record_alloc();
	if (record == NULL)
		return NULL;

	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root = sdp_list_append(NULL, &root_uuid);
	sdp_set_browse_groups(record, root);
	sdp_list_free(root, NULL);

	sdp_uuid16_create(&gatt_uuid, GENERIC_ATTRIB_SVCLASS_ID);
	svclass_id = sdp_list_append(NULL, &gatt_uuid);
	sdp_set_service_classes(record, svclass_id);
	sdp_list_free(svclass_id, NULL);

	sdp_uuid16_create(&profile.uuid, GENERIC_ATTRIB_PROFILE_ID);
	profile.version = 0x0100;
	profiles = sdp_list_append(NULL, &profile);
	sdp_set_profile_descs(record, profiles);
	sdp_list_free(profiles, NULL);

	sdp_uuid16_create(&l2cap, L2CAP_UUID);
	proto[0] = sdp_list_append(NULL, &l2cap);
	psm = sdp_data_alloc(SDP_UINT16, &lp);
	proto[0] = sdp_list_append(proto[0], psm);
	apseq = sdp_list_append(NULL, proto[0]);

	sdp_uuid16_create(&proto_uuid, ATT_UUID);
	proto[1] = sdp_list_append(NULL, &proto_uuid);
	sh = sdp_data_alloc(SDP_UINT16, &start);
	proto[1] = sdp_list_append(proto[1], sh);
	eh = sdp_data_alloc(SDP_UINT16, &end);
	proto[1] = sdp_list_append(proto[1], eh);
	apseq = sdp_list_append(apseq, proto[1]);

	aproto = sdp_list_append(NULL, apseq);
	sdp_set_access_protos(record, aproto);

	sdp_set_info_attr(record, "Generic Attribute Profile", "BlueZ", NULL);

	sdp_set_url_attr(record, "http://www.bluez.org/",
			"http://www.bluez.org/", "http://www.bluez.org/");

	sdp_set_service_id(record, gatt_uuid);

	sdp_data_free(psm);
	sdp_data_free(sh);
	sdp_data_free(eh);
	sdp_list_free(proto[0], NULL);
	sdp_list_free(proto[1], NULL);
	sdp_list_free(apseq, NULL);
	sdp_list_free(aproto, NULL);

	return record;
}

static gboolean change_humidity(gpointer user_data)
{
	static uint8_t humidity =  0x28;
	uuid_t uuid;
	uint8_t atval[1];

	/*
	 * Thermometer: relative humidity value. Humidity is
	 * being increased every 10 seconds.
	 */
	atval[0] = humidity++;
	sdp_uuid16_create(&uuid, RELATIVE_HUMIDITY_UUID);
	attrib_db_update(0x0212, &uuid, atval, 1);

	return TRUE;
}

static int register_attributes(void)
{
	const char *devname = "Example Device";
	const char *desc_out_temp = "Outside Temperature";
	const char *desc_out_hum = "Outside Relative Humidity";
	const char *manufacturer_name1 = "ACME Temperature Sensor";
	const char *manufacturer_name2 = "ACME Weighing Scales";
	const char *serial1 = "237495-3282-A";
	const char *serial2 = "11267-2327A00239";
	uint8_t atval[256];
	uuid_t uuid;
	int len;
	uint16_t u16;

	/* GAP service: primary service definition */
	sdp_uuid16_create(&uuid, GATT_PRIM_SVC_UUID);
	u16 = htons(GENERIC_ACCESS_PROFILE_ID);
	atval[0] = u16 >> 8;
	atval[1] = u16;
	attrib_db_add(0x0001, &uuid, atval, 2);

	/* GAP service: device name characteristic */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(GATT_CHARAC_DEVICE_NAME);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x06;
	atval[2] = 0x00;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0004, &uuid, atval, 5);

	/* GAP service: device name attribute */
	sdp_uuid16_create(&uuid, GATT_CHARAC_DEVICE_NAME);
	len = strlen(devname);
	strncpy((char *) atval, devname, len);
	attrib_db_add(0x0006, &uuid, atval, len);

	/* GATT service: primary service definition */
	sdp_uuid16_create(&uuid, GATT_PRIM_SVC_UUID);
	u16 = htons(GENERIC_ATTRIB_PROFILE_ID);
	atval[0] = u16 >> 8;
	atval[1] = u16;
	attrib_db_add(0x0010, &uuid, atval, 2);

	/* GATT service: attributes opcodes characteristic */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(OPCODES_SUPPORTED_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x12;
	atval[2] = 0x00;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0011, &uuid, atval, 5);

	/* GATT service: attribute opcodes supported */
	sdp_uuid16_create(&uuid, OPCODES_SUPPORTED_UUID);
	atval[0] = 0xFF;
	atval[1] = 0x01;
	attrib_db_add(0x0012, &uuid, atval, 2);

	/* Battery state service: primary service definition */
	sdp_uuid16_create(&uuid, GATT_PRIM_SVC_UUID);
	u16 = htons(BATTERY_STATE_SVC_UUID);
	atval[0] = u16 >> 8;
	atval[1] = u16;
	attrib_db_add(0x0100, &uuid, atval, 2);

	/* Battery: battery state characteristic */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(BATTERY_STATE_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x10;
	atval[2] = 0x01;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0106, &uuid, atval, 5);

	/* Battery: battery state attribute */
	sdp_uuid16_create(&uuid, BATTERY_STATE_UUID);
	u16 = htons(BATTERY_STATE_UUID);
	atval[0] = 0x04;
	attrib_db_add(0x0110, &uuid, atval, 1);

	/* Thermometer: primary service definition */
	sdp_uuid16_create(&uuid, GATT_PRIM_SVC_UUID);
	u16 = htons(THERM_HUMIDITY_SVC_UUID);
	atval[0] = u16 >> 8;
	atval[1] = u16;
	attrib_db_add(0x0200, &uuid, atval, 2);

	/* Thermometer: Include */
	sdp_uuid16_create(&uuid, GATT_INCLUDE_UUID);
	u16 = htons(MANUFACTURER_SVC_UUID);
	atval[0] = 0x00;
	atval[1] = 0x05;
	atval[2] = 0x04;
	atval[3] = 0x05;
	atval[4] = u16 >> 8;
	atval[5] = u16;
	attrib_db_add(0x0201, &uuid, atval, 6);

	/* Thermometer: Include */
	atval[0] = 0x50;
	atval[1] = 0x05;
	atval[2] = 0x68;
	atval[3] = 0x05;
	attrib_db_add(0x0202, &uuid, atval, 4);

	/* Thermometer: temperature characteristic */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(TEMPERATURE_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x04;
	atval[2] = 0x02;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0203, &uuid, atval, 5);

	/* Thermometer: temperature characteristic value */
	sdp_uuid16_create(&uuid, TEMPERATURE_UUID);
	atval[0] = 0x8A;
	atval[1] = 0x02;
	attrib_db_add(0x0204, &uuid, atval, 2);

	/* Thermometer: temperature characteristic format */
	sdp_uuid16_create(&uuid, GATT_CHARAC_FMT_UUID);
	u16 = htons(FMT_CELSIUS_UUID);
	atval[0] = 0x0E;
	atval[1] = 0xFE;
	atval[2] = u16 >> 8;
	atval[3] = u16;
	atval[4] = 0x01;
	u16 = htons(FMT_OUTSIDE_UUID);
	atval[5] = u16 >> 8;
	atval[6] = u16;
	attrib_db_add(0x0205, &uuid, atval, 7);

	/* Thermometer: characteristic user description */
	sdp_uuid16_create(&uuid, GATT_CHARAC_USER_DESC_UUID);
	len = strlen(desc_out_temp);
	strncpy((char *) atval, desc_out_temp, len);
	attrib_db_add(0x0206, &uuid, atval, len);

	/* Thermometer: relative humidity characteristic */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(RELATIVE_HUMIDITY_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x12;
	atval[2] = 0x02;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0210, &uuid, atval, 5);

	/* Thermometer: relative humidity value */
	sdp_uuid16_create(&uuid, RELATIVE_HUMIDITY_UUID);
	atval[0] = 0x27;
	attrib_db_add(0x0212, &uuid, atval, 1);

	g_timeout_add_seconds(10, change_humidity, NULL);

	/* Thermometer: relative humidity characteristic format */
	sdp_uuid16_create(&uuid, GATT_CHARAC_FMT_UUID);
	u16 = htons(FMT_PERCENT_UUID);
	atval[0] = 0x04;
	atval[1] = 0x00;
	atval[2] = u16 >> 8;
	atval[3] = u16;
	u16 = htons(BLUETOOTH_SIG_UUID);
	atval[4] = u16 >> 8;
	atval[5] = u16;
	u16 = htons(FMT_OUTSIDE_UUID);
	atval[6] = u16 >> 8;
	atval[7] = u16;
	attrib_db_add(0x0213, &uuid, atval, 8);

	/* Thermometer: characteristic user description */
	sdp_uuid16_create(&uuid, GATT_CHARAC_USER_DESC_UUID);
	len = strlen(desc_out_hum);
	strncpy((char *) atval, desc_out_hum, len);
	attrib_db_add(0x0214, &uuid, atval, len);

	/* Secondary Service: Manufacturer Service */
	sdp_uuid16_create(&uuid, GATT_SND_SVC_UUID);
	u16 = htons(MANUFACTURER_SVC_UUID);
	atval[0] = u16 >> 8;
	atval[1] = u16;
	attrib_db_add(0x0500, &uuid, atval, 2);

	/* Manufacturer name characteristic definition */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(MANUFACTURER_NAME_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x02;
	atval[2] = 0x05;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0501, &uuid, atval, 5);

	/* Manufacturer name characteristic value */
	sdp_uuid16_create(&uuid, MANUFACTURER_NAME_UUID);
	len = strlen(manufacturer_name1);
	strncpy((char *) atval, manufacturer_name1, len);
	attrib_db_add(0x0502, &uuid, atval, len);

	/* Manufacturer serial number characteristic */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(MANUFACTURER_SERIAL_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x04;
	atval[2] = 0x05;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0503, &uuid, atval, 5);

	/* Manufacturer serial number characteristic value */
	sdp_uuid16_create(&uuid, MANUFACTURER_SERIAL_UUID);
	len = strlen(serial1);
	strncpy((char *) atval, serial1, len);
	attrib_db_add(0x0504, &uuid, atval, len);

	/* Secondary Service: Manufacturer Service */
	sdp_uuid16_create(&uuid, GATT_SND_SVC_UUID);
	u16 = htons(MANUFACTURER_SVC_UUID);
	atval[0] = u16 >> 8;
	atval[1] = u16;
	attrib_db_add(0x0505, &uuid, atval, 2);

	/* Manufacturer name characteristic definition */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(MANUFACTURER_NAME_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x07;
	atval[2] = 0x05;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0506, &uuid, atval, 5);

	/* Secondary Service: Vendor Specific Service */
	sdp_uuid16_create(&uuid, GATT_SND_SVC_UUID);
	u16 = htons(VENDOR_SPECIFIC_SVC_UUID);
	atval[0] = u16 >> 8;
	atval[1] = u16;
	attrib_db_add(0x0550, &uuid, atval, 2);

	/* Vendor Specific Type characteristic definition */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(VENDOR_SPECIFIC_TYPE_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x68;
	atval[2] = 0x05;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0560, &uuid, atval, 5);

	/* Vendor Specific Type characteristic value */
	sdp_uuid16_create(&uuid, VENDOR_SPECIFIC_TYPE_UUID);
	atval[0] = 0x56;
	atval[1] = 0x65;
	atval[2] = 0x6E;
	atval[3] = 0x64;
	atval[4] = 0x6F;
	atval[5] = 0x72;
	attrib_db_add(0x0568, &uuid, atval, 6);

	/* Manufacturer name attribute */
	sdp_uuid16_create(&uuid, MANUFACTURER_NAME_UUID);
	len = strlen(manufacturer_name2);
	strncpy((char *) atval, manufacturer_name2, len);
	attrib_db_add(0x0507, &uuid, atval, len);

	/* Characteristic: serial number */
	sdp_uuid16_create(&uuid, GATT_CHARAC_UUID);
	u16 = htons(MANUFACTURER_SERIAL_UUID);
	atval[0] = ATT_CHAR_PROPER_READ;
	atval[1] = 0x09;
	atval[2] = 0x05;
	atval[3] = u16 >> 8;
	atval[4] = u16;
	attrib_db_add(0x0508, &uuid, atval, 5);

	/* Serial number characteristic value */
	sdp_uuid16_create(&uuid, MANUFACTURER_SERIAL_UUID);
	len = strlen(serial2);
	strncpy((char *) atval, serial2, len);
	attrib_db_add(0x0509, &uuid, atval, len);

	return 0;
}

int server_example_init(void)
{
	sdp_record_t *record;

	/*
	 * FIXME: Add BR/EDR service record and attributes into the GATT
	 * database. BlueZ gatt server will be automatically enabled if
	 * any plugin registers at least one primary service.
	 */

	record = server_record_new();
	if (record == NULL) {
		error("Unable to create GATT service record");
		return -1;
	}

	if (add_record_to_server(BDADDR_ANY, record) < 0) {
		error("Failed to register GATT service record");
		sdp_record_free(record);
		return -1;
	}

	handle = record->handle;

	return register_attributes();
}

void server_example_exit(void)
{
	if (handle)
		remove_record_from_server(handle);
}
