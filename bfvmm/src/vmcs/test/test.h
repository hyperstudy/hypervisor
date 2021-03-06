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

#ifndef TEST_H
#define TEST_H

#include <unittest.h>
#include <vector>
#include <functional>
#include <memory>
#include <vmcs/vmcs_intel_x64.h>
#include <memory_manager/memory_manager.h>

#define run_vmcs_test(cfg, ...) run_vmcs_test_with_args(gsl::cstring_span<>(__PRETTY_FUNCTION__), __LINE__, cfg, __VA_ARGS__)

struct control_flow_path
{
    std::function<void()> setup;
    std::shared_ptr<std::exception> exception;
    bool throws_exception;
};

extern std::map<uint32_t, uint64_t> g_msrs;
extern std::map<uint64_t, uint64_t> g_vmcs_fields;
extern uint8_t span[0x81];
extern bool g_virt_to_phys_return_nullptr;
extern bool g_phys_to_virt_return_nullptr;

void setup_mock(MockRepository &mocks, memory_manager *mm, intrinsics_intel_x64 *in);
void enable_proc_ctl(uint64_t control);
void enable_proc_ctl2(uint64_t control);
void enable_pin_ctl(uint64_t control);
void enable_exit_ctl(uint64_t control);
void enable_entry_ctl(uint64_t control);
void disable_proc_ctl(uint64_t control);
void disable_proc_ctl2(uint64_t control);
void disable_pin_ctl(uint64_t control);
void disable_exit_ctl(uint64_t control);
void disable_entry_ctl(uint64_t control);
uint64_t read_msr(uint32_t msr);
bool vmread(uint64_t field, uint64_t *val);
uint32_t cpuid_eax(uint32_t val);
uintptr_t virtptr_to_physint(void *ptr);
void *physint_to_virtptr(uintptr_t phys);

class vmcs_ut : public unittest
{
public:

    vmcs_ut();
    ~vmcs_ut() override = default;

protected:

    bool init() override;
    bool fini() override;
    bool list() override;

    template <typename R, typename ...Args> void
    run_vmcs_test_with_args(gsl::cstring_span<> fut, int line,
                            const std::vector<struct control_flow_path> &cfg,
                            R(vmcs_intel_x64::*mf)(Args...), Args &&... args)
    {
        for (const auto &path : cfg)
        {
            MockRepository mocks;
            auto mm = mocks.Mock<memory_manager>();
            auto in = bfn::mock_shared<intrinsics_intel_x64>(mocks);

            setup_mock(mocks, mm, in.get());
            path.setup();

            RUN_UNITTEST_WITH_MOCKS(mocks, [&]
            {
                vmcs_intel_x64 vmcs(in);
                auto func = std::bind(std::forward<decltype(mf)>(mf), &vmcs, std::forward<Args>(args)...);

                if (path.throws_exception)
                    this->expect_exception_with_args(std::forward<decltype(func)>(func), path.exception, fut, line);
                else
                    this->expect_no_exception_with_args(std::forward<decltype(func)>(func), fut, line);
            });
        }
    }

private:

    void test_constructor_null_intrinsics();
    void test_launch_success();
    void test_launch_vmlaunch_failure();
    void test_launch_create_vmcs_region_failure();
    void test_launch_create_exit_handler_stack_failure();
    void test_launch_clear_failure();
    void test_launch_load_failure();
    void test_promote_failure();
    void test_resume_failure();
    void test_vmread_failure();
    void test_vmwrite_failure();

    void test_check_control_pin_based_ctls_reserved_properly_set();
    void test_check_control_proc_based_ctls_reserved_properly_set();
    void test_check_control_proc_based_ctls2_reserved_properly_set();
    void test_check_control_cr3_count_less_than_4();
    void test_check_control_io_bitmap_address_bits();
    void test_check_control_msr_bitmap_address_bits();
    void test_check_control_tpr_shadow_and_virtual_apic();
    void test_check_control_nmi_exiting_and_virtual_nmi();
    void test_check_control_virtual_nmi_and_nmi_window();
    void test_check_control_virtual_apic_address_bits();
    void test_check_control_x2apic_mode_and_virtual_apic_access();
    void test_check_control_virtual_interrupt_and_external_interrupt();
    void test_check_control_process_posted_interrupt_checks();
    void test_check_control_vpid_checks();
    void test_check_control_enable_ept_checks();
    void test_check_control_enable_pml_checks();
    void test_check_control_unrestricted_guests();
    void test_check_control_enable_vm_functions();
    void test_check_control_enable_vmcs_shadowing();
    void test_check_control_enable_ept_violation_checks();
    void test_check_control_vm_exit_ctls_reserved_properly_set();
    void test_check_control_activate_and_save_preemption_timer_must_be_0();
    void test_check_control_exit_msr_store_address();
    void test_check_control_exit_msr_load_address();
    void test_check_control_vm_entry_ctls_reserved_properly_set();
    void test_check_control_event_injection_type_vector_checks();
    void test_check_control_event_injection_delivery_ec_checks();
    void test_check_control_event_injection_reserved_bits_checks();
    void test_check_control_event_injection_ec_checks();
    void test_check_control_event_injection_instr_length_checks();
    void test_check_control_entry_msr_load_address();

    void test_check_host_cr0_for_unsupported_bits();
    void test_check_host_cr4_for_unsupported_bits();
    void test_check_host_cr3_for_unsupported_bits();
    void test_check_host_ia32_sysenter_esp_canonical_address();
    void test_check_host_ia32_sysenter_eip_canonical_address();
    void test_check_host_verify_load_ia32_perf_global_ctrl();
    void test_check_host_verify_load_ia32_pat();
    void test_check_host_verify_load_ia32_efer();
    void test_check_host_es_selector_rpl_ti_equal_zero();
    void test_check_host_cs_selector_rpl_ti_equal_zero();
    void test_check_host_ss_selector_rpl_ti_equal_zero();
    void test_check_host_ds_selector_rpl_ti_equal_zero();
    void test_check_host_fs_selector_rpl_ti_equal_zero();
    void test_check_host_gs_selector_rpl_ti_equal_zero();
    void test_check_host_tr_selector_rpl_ti_equal_zero();
    void test_check_host_cs_not_equal_zero();
    void test_check_host_tr_not_equal_zero();
    void test_check_host_ss_not_equal_zero();
    void test_check_host_fs_canonical_base_address();
    void test_check_host_gs_canonical_base_address();
    void test_check_host_gdtr_canonical_base_address();
    void test_check_host_idtr_canonical_base_address();
    void test_check_host_tr_canonical_base_address();
    void test_check_host_if_outside_ia32e_mode();
    void test_check_host_vmcs_host_address_space_size_is_set();
    void test_check_host_host_address_space_disabled();
    void test_check_host_host_address_space_enabled();
};

#endif
