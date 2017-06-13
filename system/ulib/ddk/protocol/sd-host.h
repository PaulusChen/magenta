// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/driver.h>
#include <magenta/compiler.h>
#include <magenta/types.h>

__BEGIN_CDECLS;

/**
 * protocols/sd-host.h - SD Host protocol definitions
 */

typedef struct sd_host_protocol {
    // below 2 functions should be replaced with a generic busdev mechanism
    mx_handle_t (*get_interrupt)(mx_device_t* dev);
    mx_status_t (*get_mmio)(mx_device_t* dev, void** out);
    uint32_t (*get_base_clock)(mx_device_t* dev);
} sd_host_protocol_t;

__END_CDECLS;
