/*
 * Private includes for Digital Persona UareU fingerprint reader library
 * 
 *    Copyright (C) 2006 Daniel Drake <dsd@gentoo.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __DPFP_PRIVATE_H__
#define __DPFP_PRIVATE_H__

#include <stdio.h>
#include <stdint.h>

#ifdef __DARWIN_NULL
/* Darwin does endianness slightly differently */
#include <machine/endian.h>
#define __BYTE_ORDER BYTE_ORDER
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#else /* Linux */
#include <endian.h>
#endif

#include <openssl/aes.h>

extern AES_KEY aeskey;

struct dpfp_dev_entry {
	uint16_t vid;
	uint16_t pid;
	int type;
	uint8_t flags;
	const char *name;
};

#define DEV_HAS_EDGE_LIGHT	(1 << 0)

enum {
	DEV_TYPE_URU4000 = 0,
	DEV_TYPE_URU4000B = 1,
	DEV_TYPE_URU4000Bg2 = 2,
};

struct dpfp_dev_type {
	uint32_t firmware_start;
	uint32_t fw_enc_offset;
};

struct dpfp_dev {
	struct usb_dev_handle *handle;
	const struct dpfp_dev_entry *dev_entry;
};

enum {
	DBG_INFO,
	DBG_WARN,
	DBG_ERR,
};

/* URB timeouts */
#define CTRL_TIMEOUT 1000

/* To be implemented later */
#define dbg(lvl, msg) printf("%s: " msg "\n", __func__)
#define dbgf(lvl, fmt, args...) printf("%s: " fmt "\n", __func__, ## args)

/* Endpoint addresses */
#define EP_INTR	0x81
#define EP_DATA 0x82

/* USB Request types */
#define USB_IN	0xc0
#define USB_OUT	0x40

/* USB Request field */
#define USB_RQ	0x04

/* hwstat write register bits */
#define HWSTAT_ACTIVE		0x01
#define HWSTAT_SCANPWR_OFF	0x80

/* Value field of control messages */
#define HWSTAT_CONTROL		0x07
#define EDGE_LIGHT_CONTROL	0x20
#define MODE_CONTROL		0x4e
#define CHALLENGE_CONTROL	0x33
#define RESPONSE_CONTROL	0x34

#define AUTH_CHALLENGE		0x2010
#define AUTH_RESPONSE		0x2000

#define DATABLK1_RQSIZE		0x10000
#define DATABLK2_RQSIZE		0xb340

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define be16_to_cpu(x) (((x & 0xff) << 8) | (x >> 8))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define be16_to_cpu(x) (x)
#else
#error "Unrecognized endianness"
#endif

#define TV_TO_DOUBLE(tv) (tv.tv_sec + (tv.tv_usec / 1000000.0))

#endif

