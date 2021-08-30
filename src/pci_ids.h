/*
 * Copyright © 2020 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including
 * the next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef SRC_PCI_IDS_H_
#define SRC_PCI_IDS_H_

#include <stddef.h>
#include <stdint.h>

struct pci_ids {
	int32_t fd; // -1 if file access failed
	uint32_t size;
	void *addr;
};

// Sixteen byte struct is passed in registers. Avoids calling malloc/free.
struct pci_ids pci_ids_create(void);
void pci_ids_destroy(struct pci_ids pacc);

// Writes to buf. Returns buf. Does not fail.
char *pci_ids_lookup(struct pci_ids pacc, char *buf, size_t buf_size,
		     uint16_t VendorId, uint16_t DeviceId);

#endif // SRC_PCI_IDS_H_
