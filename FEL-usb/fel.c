#include <libusb.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <endian.h>
#include <string.h>
#include <stdio.h>

struct  aw_usb_request {
	char signature[8];
	uint32_t length;
	uint32_t unknown1;	/* 0x0c000000 */
	uint16_t request;
	uint32_t length2;	/* Same as length */
	char	pad[10];
}  __attribute__((packed));

static const int AW_USB_READ = 0x11;
static const int AW_USB_WRITE = 0x12;

static const int AW_USB_FEL_BULK_EP_OUT=0x01;
static const int AW_USB_FEL_BULK_EP_IN=0x82;

void usb_bulk_send(libusb_device_handle *usb, int ep, const void *data, int length)
{
	int rc, sent;
	while (length > 0) {
		rc = libusb_bulk_transfer(usb, ep, (void *)data, length, &sent, 1000);
		if (rc != 0) {
			perror("usb send");
			exit(2);
		}
		length -= sent;
		data += sent;
	}
}

void usb_bulk_recv(libusb_device_handle *usb, int ep, void *data, int length)
{
	int rc, recv;
	while (length > 0) {
		rc = libusb_bulk_transfer(usb, ep, data, length, &recv, 1000);
		if (rc != 0) {
			perror("usb recv");
			exit(2);
		}
		length -= recv;
		data += recv;
	}
}

void aw_send_usb_request(libusb_device_handle *usb, int type, int length)
{
	struct aw_usb_request req;
	memset(&req, 0, sizeof(req));
	strcpy(req.signature, "AWUC");
	req.length = req.length2 = htole32(length);
	req.request = htole16(type);
	req.unknown1 = htole32(0x0c000000);
	usb_bulk_send(usb, AW_USB_FEL_BULK_EP_OUT, &req, sizeof(req));
}

void aw_read_usb_response(libusb_device_handle *usb)
{
	char buf[13];
	usb_bulk_recv(usb, AW_USB_FEL_BULK_EP_IN, &buf, sizeof(buf));
	assert(strcmp(buf, "AWUS") == 0);
}

void aw_usb_write(libusb_device_handle *usb, const void *data, size_t len)
{
	aw_send_usb_request(usb, AW_USB_WRITE, len);
	usb_bulk_send(usb, AW_USB_FEL_BULK_EP_OUT, data, len);
	aw_read_usb_response(usb);
}

void aw_usb_read(libusb_device_handle *usb, const void *data, size_t len)
{
	aw_send_usb_request(usb, AW_USB_READ, len);
	usb_bulk_send(usb, AW_USB_FEL_BULK_EP_IN, data, len);
	aw_read_usb_response(usb);
}

struct aw_fel_request {
	uint32_t request;
	uint32_t address;
	uint32_t length;
	uint32_t pad;
};

static const int AW_FEL_VERSION = 0x001;
static const int AW_FEL_1_WRITE = 0x101;
static const int AW_FEL_1_CALL  = 0x102;
static const int AW_FEL_1_READ  = 0x103;

void aw_send_fel_request(libusb_device_handle *usb, int type, uint32_t addr, uint32_t length)
{
	struct aw_fel_request req;
	req.request = htole32(type);
	req.address = htole32(addr);
	req.length = htole32(length);
	aw_usb_write(usb, &req, sizeof(req));
}

void aw_read_fel_status(libusb_device_handle *usb)
{
	char buf[8];
	aw_usb_read(usb, &buf, sizeof(buf));
}

void aw_fel_get_version(libusb_device_handle *usb)
{
	struct aw_fel_version {
		char signature[8];
		uint16_t unknown1;
		uint16_t unknown2;
		uint32_t unknown3;
		uint16_t protocol;
		uint16_t unknown4;
		uint16_t unknown5;
		uint16_t unknown6;
		uint16_t unknown7;
		uint16_t unknown8;
		uint16_t unknown9;
		uint16_t unknown10;
	} __attribute__((packed)) buf;

	aw_send_fel_request(usb, AW_FEL_VERSION, 0, 0);
	aw_usb_read(usb, &buf, sizeof(buf));
	aw_read_fel_status(usb);

	printf("%.8s %04x %04x %08x ver=%04x %04x %04x %04x %04x %04x %04x %04x\n", buf.signature, tobuf.unknown1, buf.unknown2, buf.unknown3, buf.protocol, buf.unknown4, buf.unknown5, buf.unknown6, buf.unknown7, buf.unknown8, buf.unknown9, buf.unknown10);
}

int main(int argc, char **argv)
{
	int rc;
	libusb_device_handle *handle = NULL;
	rc = libusb_init(NULL);
	assert(rc == 0);

	handle = libusb_open_device_with_vid_pid(NULL, 0x1f3a, 0xefe8);
	if (!handle) {
		perror("A10 USB FEL device not found!");
		exit(1);
	}
	rc = libusb_claim_interface(handle, 0);
	assert(rc == 0);

	aw_fel_get_version(handle);

	return 0;
}
