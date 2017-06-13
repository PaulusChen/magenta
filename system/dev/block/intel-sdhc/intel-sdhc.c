// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/pci.h>

#include <hw/emmc.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <threads.h>
#include <unistd.h>

typedef struct intel_sdhc_device {
    mx_device_t* mxdev;

    mx_handle_t irq_handle;
    thrd_t irq_thread;

    emmc_regs_t* regs;
    uint64_t regs_size;
    mx_handle_t regs_handle;
} intel_sdhc_device_t;

static mx_protocol_device_t intel_sdhc_device_proto = {
    .version = DEVICE_OPS_VERSION,
};

static mx_status_t intel_sdhc_bind(void* ctx, mx_device_t* parent, void** cookie) {
    pci_protocol_t* pci;
    if (device_op_get_protocol(parent, MX_PROTOCOL_PCI, (void**)&pci)) return MX_ERR_NOT_SUPPORTED;

    mx_status_t status = pci->claim_device(parent);
    if (status < 0) {
        printf("intel-sdhc: error %d claiming pci device\n", status);
        return status;
    }

    // map resources and initalize the device
    intel_sdhc_device_t* dev = calloc(1, sizeof(intel_sdhc_device_t));
    if (!dev) {
        printf("intel-sdhc: out of memory\n");
        return MX_ERR_NO_MEMORY;
    }

    // map register window
    status = pci->map_resource(parent, PCI_RESOURCE_BAR_0, MX_CACHE_POLICY_UNCACHED_DEVICE,
                               (void**)&dev->regs, &dev->regs_size, &dev->regs_handle);
    if (status != MX_OK) {
        printf("intel-sdhc: error %d mapping register window\n", status);
        goto fail;
    }

    printf("intel-sdhc: version = 0x%x\n", (dev->regs->slotirqversion >> 16) & 0xff);

#if 0
    // set msi irq mode
    status = pci->set_irq_mode(dev, MX_PCIE_IRQ_MODE_MSI, 1);
    if (status < 0) {
        printf("ahci: error %d setting irq mode\n", status);
        goto fail;
    }

    // get irq handle
    status = pci->map_interrupt(dev, 0, &device->irq_handle);
    if (status != MX_OK) {
        printf("ahci: error %d getting irq handle\n", status);
        goto fail;
    }

    // start irq thread
    int ret = thrd_create_with_name(&device->irq_thread, ahci_irq_thread, device, "ahci-irq");
    if (ret != thrd_success) {
        printf("ahci: error %d in irq thread create\n", ret);
        goto fail;
    }
#endif

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "intel-sdhc",
        .ctx = dev,
        .ops = &intel_sdhc_device_proto,
        .proto_id = MX_PROTOCOL_SDHC,
    };

    status = device_add(parent, &args, &dev->mxdev);
    if (status != MX_OK) {
        goto fail;
    }

    return MX_OK;
fail:
    free(dev);
    return status;
}

static mx_driver_ops_t intel_sdhc_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = intel_sdhc_bind,
};

// clang-format off
MAGENTA_DRIVER_BEGIN(intel_sdhc, intel_sdhc_driver_ops, "magenta", "0.1", 4)
    BI_ABORT_IF(NE, BIND_PROTOCOL, MX_PROTOCOL_PCI),
    BI_ABORT_IF(NE, BIND_PCI_CLASS, 0x08),
    BI_ABORT_IF(NE, BIND_PCI_SUBCLASS, 0x05),
    BI_MATCH_IF(EQ, BIND_PCI_INTERFACE, 0x01),
MAGENTA_DRIVER_END(intel_sdhc)
