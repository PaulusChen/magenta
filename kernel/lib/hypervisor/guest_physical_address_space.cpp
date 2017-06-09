// Copyright 2017 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <hypervisor/guest_physical_address_space.h>

#include <arch/mmu.h>
#include <kernel/vm/arch_vm_aspace.h>
#include <kernel/vm/fault.h>
#include <mxalloc/new.h>

static const uint kPfFlags = VMM_PF_FLAG_WRITE | VMM_PF_FLAG_SW_FAULT;
static const uint kApicMmuFlags = ARCH_MMU_FLAG_PERM_READ | ARCH_MMU_FLAG_PERM_WRITE;
static const uint kMmuFlags =
    ARCH_MMU_FLAG_PERM_READ | ARCH_MMU_FLAG_PERM_WRITE | ARCH_MMU_FLAG_PERM_EXECUTE;
static const size_t kAddressSpaceSize =  256ul << 30;

status_t GuestPhysicalAddressSpace::Create(mxtl::RefPtr<VmObject> guest_phys_mem,
                                           mxtl::unique_ptr<GuestPhysicalAddressSpace>* _gpas) {
    AllocChecker ac;
    mxtl::unique_ptr<GuestPhysicalAddressSpace> gpas(
        new (&ac) GuestPhysicalAddressSpace(guest_phys_mem));
    if (!ac.check())
        return ERR_NO_MEMORY;

    status_t status = gpas->Init(kAddressSpaceSize);
    if (status != NO_ERROR)
        return status;

    // TODO(abdulla): Figure out how to do this on-demand.
    status = gpas->MapRange(0, guest_phys_mem->size());
    if (status != NO_ERROR)
        return status;

    *_gpas = mxtl::move(gpas);
    return NO_ERROR;
}

GuestPhysicalAddressSpace::GuestPhysicalAddressSpace(mxtl::RefPtr<VmObject> guest_phys_mem)
    : guest_phys_mem_(guest_phys_mem) {}

GuestPhysicalAddressSpace::~GuestPhysicalAddressSpace() {
    status_t status = aspace_.Destroy();
    DEBUG_ASSERT(status == NO_ERROR);
}

status_t GuestPhysicalAddressSpace::Init(size_t size) {
    return aspace_.Init(0, size, 0);
}

static status_t map_page(ArchVmGuestAspace *aspace, vaddr_t guest_paddr, paddr_t host_paddr,
                         uint mmu_flags) {
    size_t mapped;
    status_t status = aspace->Map(guest_paddr, host_paddr, 1, mmu_flags, &mapped);
    if (status != NO_ERROR)
        return status;
    return mapped != 1 ? ERR_NO_MEMORY : NO_ERROR;
}

status_t GuestPhysicalAddressSpace::MapApicPage(vaddr_t guest_paddr, paddr_t host_paddr) {
    return map_page(&aspace_, guest_paddr, host_paddr, kApicMmuFlags);
}

status_t GuestPhysicalAddressSpace::MapRange(vaddr_t guest_paddr, size_t size) {
    auto mmu_map = [](void* context, size_t offset, size_t index, paddr_t pa) -> status_t {
        ArchVmGuestAspace* aspace = static_cast<ArchVmGuestAspace*>(context);
        return map_page(aspace, offset, pa, kMmuFlags);
    };
    return guest_phys_mem_->Lookup(guest_paddr, size, kPfFlags, mmu_map, &aspace_);
}

status_t GuestPhysicalAddressSpace::UnmapRange(vaddr_t guest_paddr, size_t size) {
    size_t num_pages = size / PAGE_SIZE;
    size_t unmapped;
    status_t status = aspace_.Unmap(guest_paddr, num_pages, &unmapped);
    if (status != NO_ERROR)
        return status;
    return unmapped != num_pages ? ERR_BAD_STATE : NO_ERROR;
}

status_t GuestPhysicalAddressSpace::GetPage(vaddr_t guest_paddr, paddr_t* host_paddr) {
    return aspace_.Query(guest_paddr, host_paddr, nullptr);
}
