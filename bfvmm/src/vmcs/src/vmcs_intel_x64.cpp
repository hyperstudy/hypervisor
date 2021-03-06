//
// Bareflank Hypervisor
//
// Copyright (C) 2015 Assured Information Security, Inc.
// Author: Rian Quinn        <quinnr@ainfosec.com>
// Author: Brendan Kerrigan  <kerriganb@ainfosec.com>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#include <gsl/gsl>

#include <debug.h>
#include <constants.h>
#include <view_as_pointer.h>
#include <vmcs/vmcs_intel_x64.h>
#include <vmcs/vmcs_intel_x64_resume.h>
#include <vmcs/vmcs_intel_x64_promote.h>
#include <memory_manager/memory_manager.h>
#include <exit_handler/exit_handler_intel_x64_support.h>

vmcs_intel_x64::vmcs_intel_x64(std::shared_ptr<intrinsics_intel_x64> intrinsics) :
    m_intrinsics(std::move(intrinsics)),
    m_vmcs_region_phys(0)
{
    if (!m_intrinsics)
        m_intrinsics = std::make_shared<intrinsics_intel_x64>();
}

void
vmcs_intel_x64::launch(const std::shared_ptr<vmcs_intel_x64_state> &host_state,
                       const std::shared_ptr<vmcs_intel_x64_state> &guest_state)
{
    this->create_vmcs_region();

    auto ___ = gsl::on_failure([&]
    { this->release_vmcs_region(); });

    this->create_exit_handler_stack();

    auto ___ = gsl::on_failure([&]
    { this->release_exit_handler_stack(); });

    this->clear();
    this->load();

    this->write_16bit_guest_state(guest_state);
    this->write_64bit_guest_state(guest_state);
    this->write_32bit_guest_state(guest_state);
    this->write_natural_guest_state(guest_state);

    this->write_16bit_control_state(host_state);
    this->write_64bit_control_state(host_state);
    this->write_32bit_control_state(host_state);
    this->write_natural_control_state(host_state);

    this->write_16bit_host_state(host_state);
    this->write_64bit_host_state(host_state);
    this->write_32bit_host_state(host_state);
    this->write_natural_host_state(host_state);

    this->pin_based_vm_execution_controls();
    this->primary_processor_based_vm_execution_controls();
    this->secondary_processor_based_vm_execution_controls();
    this->vm_exit_controls();
    this->vm_entry_controls();

    if (!m_intrinsics->vmlaunch())
    {
        auto ___ = gsl::finally([&]
        {
            this->dump_vmcs();

            this->print_execution_controls();
            this->print_pin_based_vm_execution_controls();
            this->print_primary_processor_based_vm_execution_controls();
            this->print_secondary_processor_based_vm_execution_controls();
            this->print_vm_exit_control_fields();
            this->print_vm_entry_control_fields();

            host_state->dump();
            guest_state->dump();
        });

        this->check_vmcs_control_state();
        this->check_vmcs_guest_state();
        this->check_vmcs_host_state();

        bferror << "vmlaunch failed:" << bfendl;
        bferror << "    - vm_instruction_error: "
                << this->get_vm_instruction_error() << bfendl;

        throw std::runtime_error("vmcs launch failed");
    }
}

void
vmcs_intel_x64::promote()
{
    vmcs_promote(vmread(VMCS_HOST_GS_BASE));

    throw std::runtime_error("vmcs promote failed");
}

void
vmcs_intel_x64::resume()
{
    vmcs_resume(m_state_save.get());

    throw std::runtime_error("vmcs resume failed");
}

void
vmcs_intel_x64::load()
{
    if (!m_intrinsics->vmptrld(&m_vmcs_region_phys))
        throw std::runtime_error("vmcs load failed");
}

void
vmcs_intel_x64::clear()
{
    if (!m_intrinsics->vmclear(&m_vmcs_region_phys))
        throw std::runtime_error("vmcs clear failed");
}

void
vmcs_intel_x64::create_vmcs_region()
{
    auto ___ = gsl::on_failure([&]
    { this->release_vmcs_region(); });

    m_vmcs_region = std::make_unique<uint32_t[]>(1024);
    m_vmcs_region_phys = g_mm->virtptr_to_physint(m_vmcs_region.get());

    if (m_vmcs_region_phys == 0)
        throw std::logic_error("m_vmcs_region_phys == nullptr");

    gsl::span<uint32_t> id{m_vmcs_region.get(), 1024};
    id[0] = static_cast<uint32_t>(m_intrinsics->read_msr(IA32_VMX_BASIC_MSR) & 0x7FFFFFFFF);
}

void
vmcs_intel_x64::release_vmcs_region() noexcept
{
    m_vmcs_region.reset();
    m_vmcs_region_phys = 0;
}

void
vmcs_intel_x64::create_exit_handler_stack()
{
    m_exit_handler_stack = std::make_unique<char[]>(STACK_SIZE);
}

void
vmcs_intel_x64::release_exit_handler_stack() noexcept
{
    m_exit_handler_stack.reset();
}

void
vmcs_intel_x64::write_16bit_control_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    (void) state;

    // unused: VMCS_VIRTUAL_PROCESSOR_IDENTIFIER
    // unused: VMCS_POSTED_INTERRUPT_NOTIFICATION_VECTOR
    // unused: VMCS_EPTP_INDEX
}

void
vmcs_intel_x64::write_64bit_control_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    (void) state;

    // unused: VMCS_ADDRESS_OF_IO_BITMAP_A_FULL
    // unused: VMCS_ADDRESS_OF_IO_BITMAP_B_FULL
    // unused: VMCS_ADDRESS_OF_MSR_BITMAPS_FULL
    // unused: VMCS_VM_EXIT_MSR_STORE_ADDRESS_FULL
    // unused: VMCS_VM_EXIT_MSR_LOAD_ADDRESS_FULL
    // unused: VMCS_VM_ENTRY_MSR_LOAD_ADDRESS_FULL
    // unused: VMCS_EXECUTIVE_VMCS_POINTER_FULL
    // unused: VMCS_TSC_OFFSET_FULL
    // unused: VMCS_VIRTUAL_APIC_ADDRESS_FULL
    // unused: VMCS_APIC_ACCESS_ADDRESS_FULL
    // unused: VMCS_POSTED_INTERRUPT_DESCRIPTOR_ADDRESS_FULL
    // unused: VMCS_VM_FUNCTION_CONTROLS_FULL
    // unused: VMCS_EPT_POINTER_FULL
    // unused: VMCS_EOI_EXIT_BITMAP_0_FULL
    // unused: VMCS_EOI_EXIT_BITMAP_1_FULL
    // unused: VMCS_EOI_EXIT_BITMAP_2_FULL
    // unused: VMCS_EOI_EXIT_BITMAP_3_FULL
    // unused: VMCS_EPTP_LIST_ADDRESS_FULL
    // unused: VMCS_VMREAD_BITMAP_ADDRESS_FULL
    // unused: VMCS_VMWRITE_BITMAP_ADDRESS_FULL
    // unused: VMCS_VIRTUALIZATION_EXCEPTION_INFORMATION_ADDRESS_FULL
    // unused: VMCS_XSS_EXITING_BITMAP_FULL
}

void
vmcs_intel_x64::write_32bit_control_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    (void) state;

    uint64_t lower;
    uint64_t upper;

    auto ia32_vmx_pinbased_ctls_msr =
        m_intrinsics->read_msr(IA32_VMX_TRUE_PINBASED_CTLS_MSR);
    auto ia32_vmx_procbased_ctls_msr =
        m_intrinsics->read_msr(IA32_VMX_TRUE_PROCBASED_CTLS_MSR);
    auto ia32_vmx_exit_ctls_msr =
        m_intrinsics->read_msr(IA32_VMX_TRUE_EXIT_CTLS_MSR);
    auto ia32_vmx_entry_ctls_msr =
        m_intrinsics->read_msr(IA32_VMX_TRUE_ENTRY_CTLS_MSR);

    lower = ((ia32_vmx_pinbased_ctls_msr >> 0) & 0x00000000FFFFFFFF);
    upper = ((ia32_vmx_pinbased_ctls_msr >> 32) & 0x00000000FFFFFFFF);
    vmwrite(VMCS_PIN_BASED_VM_EXECUTION_CONTROLS, lower & upper);

    lower = ((ia32_vmx_procbased_ctls_msr >> 0) & 0x00000000FFFFFFFF);
    upper = ((ia32_vmx_procbased_ctls_msr >> 32) & 0x00000000FFFFFFFF);
    vmwrite(VMCS_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, lower & upper);

    lower = ((ia32_vmx_exit_ctls_msr >> 0) & 0x00000000FFFFFFFF);
    upper = ((ia32_vmx_exit_ctls_msr >> 32) & 0x00000000FFFFFFFF);
    vmwrite(VMCS_VM_EXIT_CONTROLS, lower & upper);

    lower = ((ia32_vmx_entry_ctls_msr >> 0) & 0x00000000FFFFFFFF);
    upper = ((ia32_vmx_entry_ctls_msr >> 32) & 0x00000000FFFFFFFF);
    vmwrite(VMCS_VM_ENTRY_CONTROLS, lower & upper);

    // unused: VMCS_EXCEPTION_BITMAP
    // unused: VMCS_PAGE_FAULT_ERROR_CODE_MASK
    // unused: VMCS_PAGE_FAULT_ERROR_CODE_MATCH
    // unused: VMCS_CR3_TARGET_COUNT
    // unused: VMCS_VM_EXIT_MSR_STORE_COUNT
    // unused: VMCS_VM_EXIT_MSR_LOAD_COUNT
    // unused: VMCS_VM_ENTRY_MSR_LOAD_COUNT
    // unused: VMCS_VM_ENTRY_INTERRUPTION_INFORMATION_FIELD
    // unused: VMCS_VM_ENTRY_EXCEPTION_ERROR_CODE
    // unused: VMCS_VM_ENTRY_INSTRUCTION_LENGTH
    // unused: VMCS_TPR_THRESHOLD
    // unused: VMCS_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS
    // unused: VMCS_PLE_GAP
    // unused: VMCS_PLE_WINDOW
}

void
vmcs_intel_x64::write_natural_control_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    (void) state;

    // unused: VMCS_CR0_GUEST_HOST_MASK
    // unused: VMCS_CR4_GUEST_HOST_MASK
    // unused: VMCS_CR0_READ_SHADOW
    // unused: VMCS_CR4_READ_SHADOW
    // unused: VMCS_CR3_TARGET_VALUE_0
    // unused: VMCS_CR3_TARGET_VALUE_1
    // unused: VMCS_CR3_TARGET_VALUE_2
    // unused: VMCS_CR3_TARGET_VALUE_31
}

void
vmcs_intel_x64::write_16bit_guest_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    vmwrite(VMCS_GUEST_ES_SELECTOR, state->es());
    vmwrite(VMCS_GUEST_CS_SELECTOR, state->cs());
    vmwrite(VMCS_GUEST_SS_SELECTOR, state->ss());
    vmwrite(VMCS_GUEST_DS_SELECTOR, state->ds());
    vmwrite(VMCS_GUEST_FS_SELECTOR, state->fs());
    vmwrite(VMCS_GUEST_GS_SELECTOR, state->gs());
    vmwrite(VMCS_GUEST_LDTR_SELECTOR, state->ldtr());
    vmwrite(VMCS_GUEST_TR_SELECTOR, state->tr());

    // unused: VMCS_GUEST_INTERRUPT_STATUS
}

void
vmcs_intel_x64::write_64bit_guest_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    vmwrite(VMCS_VMCS_LINK_POINTER_FULL, 0xFFFFFFFFFFFFFFFF);
    vmwrite(VMCS_GUEST_IA32_DEBUGCTL_FULL, state->ia32_debugctl_msr());
    vmwrite(VMCS_GUEST_IA32_PAT_FULL, state->ia32_pat_msr());
    vmwrite(VMCS_GUEST_IA32_EFER_FULL, state->ia32_efer_msr());
    vmwrite(VMCS_GUEST_IA32_PERF_GLOBAL_CTRL_FULL, state->ia32_perf_global_ctrl_msr());

    // unused: VMCS_GUEST_PDPTE0_FULL
    // unused: VMCS_GUEST_PDPTE1_FULL
    // unused: VMCS_GUEST_PDPTE2_FULL
    // unused: VMCS_GUEST_PDPTE3_FULL
}

void
vmcs_intel_x64::write_32bit_guest_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    vmwrite(VMCS_GUEST_ES_LIMIT, state->es_limit());
    vmwrite(VMCS_GUEST_CS_LIMIT, state->cs_limit());
    vmwrite(VMCS_GUEST_SS_LIMIT, state->ss_limit());
    vmwrite(VMCS_GUEST_DS_LIMIT, state->ds_limit());
    vmwrite(VMCS_GUEST_FS_LIMIT, state->fs_limit());
    vmwrite(VMCS_GUEST_GS_LIMIT, state->gs_limit());
    vmwrite(VMCS_GUEST_LDTR_LIMIT, state->ldtr_limit());
    vmwrite(VMCS_GUEST_TR_LIMIT, state->tr_limit());

    vmwrite(VMCS_GUEST_GDTR_LIMIT, state->gdt_limit());
    vmwrite(VMCS_GUEST_IDTR_LIMIT, state->idt_limit());

    vmwrite(VMCS_GUEST_ES_ACCESS_RIGHTS, state->es_access_rights());
    vmwrite(VMCS_GUEST_CS_ACCESS_RIGHTS, state->cs_access_rights());
    vmwrite(VMCS_GUEST_SS_ACCESS_RIGHTS, state->ss_access_rights());
    vmwrite(VMCS_GUEST_DS_ACCESS_RIGHTS, state->ds_access_rights());
    vmwrite(VMCS_GUEST_FS_ACCESS_RIGHTS, state->fs_access_rights());
    vmwrite(VMCS_GUEST_GS_ACCESS_RIGHTS, state->gs_access_rights());
    vmwrite(VMCS_GUEST_LDTR_ACCESS_RIGHTS, state->ldtr_access_rights());
    vmwrite(VMCS_GUEST_TR_ACCESS_RIGHTS, state->tr_access_rights());

    vmwrite(VMCS_GUEST_IA32_SYSENTER_CS, state->ia32_sysenter_cs_msr());

    // unused: VMCS_GUEST_INTERRUPTIBILITY_STATE
    // unused: VMCS_GUEST_ACTIVITY_STATE
    // unused: VMCS_GUEST_SMBASE
    // unused: VMCS_VMX_PREEMPTION_TIMER_VALUE
}

void
vmcs_intel_x64::write_natural_guest_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    vmwrite(VMCS_GUEST_CR0, state->cr0());
    vmwrite(VMCS_GUEST_CR3, state->cr3());
    vmwrite(VMCS_GUEST_CR4, state->cr4());

    vmwrite(VMCS_GUEST_ES_BASE, state->es_base());
    vmwrite(VMCS_GUEST_CS_BASE, state->cs_base());
    vmwrite(VMCS_GUEST_SS_BASE, state->ss_base());
    vmwrite(VMCS_GUEST_DS_BASE, state->ds_base());
    vmwrite(VMCS_GUEST_FS_BASE, state->ia32_fs_base_msr());
    vmwrite(VMCS_GUEST_GS_BASE, state->ia32_gs_base_msr());
    vmwrite(VMCS_GUEST_LDTR_BASE, state->ldtr_base());
    vmwrite(VMCS_GUEST_TR_BASE, state->tr_base());

    vmwrite(VMCS_GUEST_GDTR_BASE, state->gdt_base());
    vmwrite(VMCS_GUEST_IDTR_BASE, state->idt_base());

    vmwrite(VMCS_GUEST_DR7, state->dr7());
    vmwrite(VMCS_GUEST_RFLAGS, state->rflags());

    vmwrite(VMCS_GUEST_IA32_SYSENTER_ESP, state->ia32_sysenter_esp_msr());
    vmwrite(VMCS_GUEST_IA32_SYSENTER_EIP, state->ia32_sysenter_eip_msr());

    // unused: VMCS_GUEST_RSP, see m_intrinsics->vmlaunch()
    // unused: VMCS_GUEST_RIP, see m_intrinsics->vmlaunch()
    // unused: VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS
}

void
vmcs_intel_x64::write_16bit_host_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    vmwrite(VMCS_HOST_ES_SELECTOR, state->es());
    vmwrite(VMCS_HOST_CS_SELECTOR, state->cs());
    vmwrite(VMCS_HOST_SS_SELECTOR, state->ss());
    vmwrite(VMCS_HOST_DS_SELECTOR, state->ds());
    vmwrite(VMCS_HOST_FS_SELECTOR, state->fs());
    vmwrite(VMCS_HOST_GS_SELECTOR, state->gs());
    vmwrite(VMCS_HOST_TR_SELECTOR, state->tr());
}

void
vmcs_intel_x64::write_64bit_host_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    vmwrite(VMCS_HOST_IA32_PAT_FULL, state->ia32_pat_msr());
    vmwrite(VMCS_HOST_IA32_EFER_FULL, state->ia32_efer_msr());
    vmwrite(VMCS_HOST_IA32_PERF_GLOBAL_CTRL_FULL, state->ia32_perf_global_ctrl_msr());
}

void
vmcs_intel_x64::write_32bit_host_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    vmwrite(VMCS_HOST_IA32_SYSENTER_CS, state->ia32_sysenter_cs_msr());
}

void
vmcs_intel_x64::write_natural_host_state(const std::shared_ptr<vmcs_intel_x64_state> &state)
{
    auto exit_handler_stack = reinterpret_cast<uintptr_t>(m_exit_handler_stack.get());

    exit_handler_stack += STACK_SIZE;
    exit_handler_stack &= 0xFFFFFFFFFFFFFFF0;

    vmwrite(VMCS_HOST_CR0, state->cr0());
    vmwrite(VMCS_HOST_CR3, state->cr3());
    vmwrite(VMCS_HOST_CR4, state->cr4());

    vmwrite(VMCS_HOST_FS_BASE, state->ia32_fs_base_msr());
    vmwrite(VMCS_HOST_GS_BASE, state->ia32_gs_base_msr());
    vmwrite(VMCS_HOST_TR_BASE, state->tr_base());

    vmwrite(VMCS_HOST_GDTR_BASE, state->gdt_base());
    vmwrite(VMCS_HOST_IDTR_BASE, state->idt_base());

    vmwrite(VMCS_HOST_IA32_SYSENTER_ESP, state->ia32_sysenter_esp_msr());
    vmwrite(VMCS_HOST_IA32_SYSENTER_EIP, state->ia32_sysenter_eip_msr());

    vmwrite(VMCS_HOST_RSP, reinterpret_cast<uintptr_t>(exit_handler_stack));
    vmwrite(VMCS_HOST_RIP, reinterpret_cast<uintptr_t>(exit_handler_entry));
}

void
vmcs_intel_x64::pin_based_vm_execution_controls()
{
    auto controls = vmread(VMCS_PIN_BASED_VM_EXECUTION_CONTROLS);

    // controls |= VM_EXEC_PIN_BASED_EXTERNAL_INTERRUPT_EXITING;
    // controls |= VM_EXEC_PIN_BASED_NMI_EXITING;
    // controls |= VM_EXEC_PIN_BASED_VIRTUAL_NMIS;
    // controls |= VM_EXEC_PIN_BASED_ACTIVATE_VMX_PREEMPTION_TIMER;
    // controls |= VM_EXEC_PIN_BASED_PROCESS_POSTED_INTERRUPTS;

    this->filter_unsupported(IA32_VMX_TRUE_PINBASED_CTLS_MSR, controls);

    vmwrite(VMCS_PIN_BASED_VM_EXECUTION_CONTROLS, controls);
}

void
vmcs_intel_x64::primary_processor_based_vm_execution_controls()
{
    auto controls = vmread(VMCS_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS);

    // controls |= VM_EXEC_P_PROC_BASED_INTERRUPT_WINDOW_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_USE_TSC_OFFSETTING;
    // controls |= VM_EXEC_P_PROC_BASED_HLT_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_INVLPG_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_MWAIT_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_RDPMC_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_RDTSC_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_CR3_LOAD_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_CR3_STORE_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_CR8_LOAD_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_CR8_STORE_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_USE_TPR_SHADOW;
    // controls |= VM_EXEC_P_PROC_BASED_NMI_WINDOW_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_MOV_DR_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_UNCONDITIONAL_IO_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_USE_IO_BITMAPS;
    // controls |= VM_EXEC_P_PROC_BASED_MONITOR_TRAP_FLAG;
    // controls |= VM_EXEC_P_PROC_BASED_USE_MSR_BITMAPS;
    // controls |= VM_EXEC_P_PROC_BASED_MONITOR_EXITING;
    // controls |= VM_EXEC_P_PROC_BASED_PAUSE_EXITING;
    controls |= VM_EXEC_P_PROC_BASED_ACTIVATE_SECONDARY_CONTROLS;

    this->filter_unsupported(IA32_VMX_TRUE_PROCBASED_CTLS_MSR, controls);

    vmwrite(VMCS_PRIMARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, controls);
}

void
vmcs_intel_x64::secondary_processor_based_vm_execution_controls()
{
    auto controls = vmread(VMCS_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS);

    // controls |= VM_EXEC_S_PROC_BASED_VIRTUALIZE_APIC_ACCESSES;
    // controls |= VM_EXEC_S_PROC_BASED_ENABLE_EPT;
    // controls |= VM_EXEC_S_PROC_BASED_DESCRIPTOR_TABLE_EXITING;
    controls |= VM_EXEC_S_PROC_BASED_ENABLE_RDTSCP;
    // controls |= VM_EXEC_S_PROC_BASED_VIRTUALIZE_X2APIC_MODE;
    // controls |= VM_EXEC_S_PROC_BASED_ENABLE_VPID;
    // controls |= VM_EXEC_S_PROC_BASED_WBINVD_EXITING;
    // controls |= VM_EXEC_S_PROC_BASED_UNRESTRICTED_GUEST;
    // controls |= VM_EXEC_S_PROC_BASED_APIC_REGISTER_VIRTUALIZATION;
    // controls |= VM_EXEC_S_PROC_BASED_VIRTUAL_INTERRUPT_DELIVERY;
    // controls |= VM_EXEC_S_PROC_BASED_PAUSE_LOOP_EXITING;
    // controls |= VM_EXEC_S_PROC_BASED_RDRAND_EXITING;
    controls |= VM_EXEC_S_PROC_BASED_ENABLE_INVPCID;
    // controls |= VM_EXEC_S_PROC_BASED_ENABLE_VM_FUNCTIONS;
    // controls |= VM_EXEC_S_PROC_BASED_VMCS_SHADOWING;
    // controls |= VM_EXEC_S_PROC_BASED_RDSEED_EXITING;
    // controls |= VM_EXEC_S_PROC_BASED_EPT_VIOLATION_VE;
    controls |= VM_EXEC_S_PROC_BASED_ENABLE_XSAVES_XRSTORS;

    this->filter_unsupported(IA32_VMX_PROCBASED_CTLS2_MSR, controls);

    vmwrite(VMCS_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, controls);
}

void
vmcs_intel_x64::vm_exit_controls()
{
    auto controls = vmread(VMCS_VM_EXIT_CONTROLS);

    controls |= VM_EXIT_CONTROL_SAVE_DEBUG_CONTROLS;
    controls |= VM_EXIT_CONTROL_HOST_ADDRESS_SPACE_SIZE;
    controls |= VM_EXIT_CONTROL_LOAD_IA32_PERF_GLOBAL_CTRL;
    controls |= VM_EXIT_CONTROL_ACKNOWLEDGE_INTERRUPT_ON_EXIT;
    controls |= VM_EXIT_CONTROL_SAVE_IA32_PAT;
    controls |= VM_EXIT_CONTROL_LOAD_IA32_PAT;
    controls |= VM_EXIT_CONTROL_SAVE_IA32_EFER;
    controls |= VM_EXIT_CONTROL_LOAD_IA32_EFER;
    // controls |= VM_EXIT_CONTROL_SAVE_VMX_PREEMPTION_TIMER_VALUE;

    this->filter_unsupported(IA32_VMX_TRUE_EXIT_CTLS_MSR, controls);

    vmwrite(VMCS_VM_EXIT_CONTROLS, controls);
}

void
vmcs_intel_x64::vm_entry_controls()
{
    auto controls = vmread(VMCS_VM_ENTRY_CONTROLS);

    controls |= VM_ENTRY_CONTROL_LOAD_DEBUG_CONTROLS;
    controls |= VM_ENTRY_CONTROL_IA_32E_MODE_GUEST;
    // controls |= VM_ENTRY_CONTROL_ENTRY_TO_SMM;
    // controls |= VM_ENTRY_CONTROL_DEACTIVATE_DUAL_MONITOR_TREATMENT;
    controls |= VM_ENTRY_CONTROL_LOAD_IA32_PERF_GLOBAL_CTRL;
    controls |= VM_ENTRY_CONTROL_LOAD_IA32_PAT;
    controls |= VM_ENTRY_CONTROL_LOAD_IA32_EFER;

    this->filter_unsupported(IA32_VMX_TRUE_ENTRY_CTLS_MSR, controls);

    vmwrite(VMCS_VM_ENTRY_CONTROLS, controls);
}

uint64_t
vmcs_intel_x64::vmread(uint64_t field) const
{
    uint64_t value = 0;

    if (!m_intrinsics->vmread(field, &value))
    {
        bferror << "vmcs_intel_x64::vmread failed:" << bfendl;
        bferror << "    - field: " << view_as_pointer(field) << bfendl;

        throw std::runtime_error("vmread failed");
    }

    return value;
}

void
vmcs_intel_x64::vmwrite(uint64_t field, uint64_t value)
{
    if (!m_intrinsics->vmwrite(field, value))
    {
        bferror << "vmcs_intel_x64::vmwrite failed:" << bfendl;
        bferror << "    - field: " << view_as_pointer(field) << bfendl;
        bferror << "    - value: " << view_as_pointer(value) << bfendl;

        throw std::runtime_error("vmwrite failed");
    }
}

void
vmcs_intel_x64::filter_unsupported(uint32_t msr, uint64_t &ctrl)
{
    auto allowed = m_intrinsics->read_msr(msr);
    auto allowed0 = ((allowed >> 00) & 0x00000000FFFFFFFF);
    auto allowed1 = ((allowed >> 32) & 0x00000000FFFFFFFF);

    if ((allowed0 & ctrl) != allowed0)
    {
        bfdebug << "vmcs ctrl field mis-configured for msr allowed 0: " << view_as_pointer(msr) << bfendl;
        bfdebug << "    - allowed0: " << view_as_pointer(allowed0) << bfendl;
        bfdebug << "    - old ctrl: " << view_as_pointer(ctrl) << bfendl;

        ctrl |= allowed0;

        bfdebug << "    - new ctrl: " << view_as_pointer(ctrl) << bfendl;
    }

    if ((ctrl & ~allowed1) != 0)
    {
        bfdebug << "vmcs ctrl field mis-configured for msr allowed 1: " << view_as_pointer(msr) << bfendl;
        bfdebug << "    - allowed1: " << view_as_pointer(allowed1) << bfendl;
        bfdebug << "    - old ctrl: " << view_as_pointer(ctrl) << bfendl;

        ctrl &= allowed1;

        bfdebug << "    - new ctrl: " << view_as_pointer(ctrl) << bfendl;
    }
}
