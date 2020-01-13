/**
 * Bao, a Lightweight Static Partitioning Hypervisor
 *
 * Copyright (c) Bao Project (www.bao-project.org), 2019-
 *
 * Authors:
 *      Jose Martins <jose.martins@bao-project.org>
 *
 * Bao is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation, with a special exception exempting guest code from such
 * license. See the COPYING file in the top-level directory for details.
 *
 */

#include <arch/aborts.h>
#include <arch/sysregs.h>
#include <cpu.h>
#include <vm.h>
#include <emul.h>
#include <arch/psci.h>

typedef void (*abort_handler_t)(uint32_t, uint64_t, uint64_t);

void aborts_data_lower(uint32_t iss, uint64_t far, uint64_t il)
{
    if (!(iss & ESR_ISS_DA_ISV_BIT) || (iss & ESR_ISS_DA_FnV_BIT)) {
        ERROR("no information to handle data abort (0x%x)", far);
    }

    uint32_t DSFC =
        bit_extract(iss, ESR_ISS_DA_DSFC_OFF, ESR_ISS_DA_DSFC_LEN) & (0xf << 2);

    if (DSFC != ESR_ISS_DA_DSFC_TRNSLT) {
        ERROR("data abort is not translation fault - cant deal with it");
    }

    uint64_t addr = far;
    emul_handler_t handler = vm_get_emul(cpu.vcpu->vm, addr);
    if (handler != NULL) {
        emul_access_t emul;
        emul.addr = addr;
        emul.width =
            (1 << bit_extract(iss, ESR_ISS_DA_SAS_OFF, ESR_ISS_DA_SAS_LEN));
        emul.write = iss & ESR_ISS_DA_WnR_BIT ? true : false;
        emul.reg = bit_extract(iss, ESR_ISS_DA_SRT_OFF, ESR_ISS_DA_SRT_LEN);
        emul.reg_width =
            4 + (4 * bit_extract(iss, ESR_ISS_DA_SF_OFF, ESR_ISS_DA_SF_LEN));
        emul.sign_ext =
            bit_extract(iss, ESR_ISS_DA_SSE_OFF, ESR_ISS_DA_SSE_LEN);

        // TODO: check if the access is aligned. If not, inject an exception in
        // the vm

        if (handler(&emul)) {
            uint64_t pc_step = 2 + (2 * il);
            cpu.vcpu->regs->elr_el2 += pc_step;
        } else {
            ERROR("data abort emulation failed");
        }
    } else {
        ERROR("no emulation handler for abort(0x%x at 0x%x)", far,
              cpu.vcpu->regs->elr_el2);
    }
}

void smc64_handler(uint32_t iss, uint64_t far, uint64_t il)
{
    uint64_t smc_fid = cpu.vcpu->regs->x[0];
    uint64_t x1 = cpu.vcpu->regs->x[1];
    uint64_t x2 = cpu.vcpu->regs->x[2];
    uint64_t x3 = cpu.vcpu->regs->x[3];

    int64_t ret = -1;

    if (is_psci_fid(smc_fid)) {
        ret = psci_smc_handler(smc_fid, x1, x2, x3);
    } else {
        INFO("unknown smc_fid 0x%lx", smc_fid);
    }

    vcpu_writereg(cpu.vcpu, 0, ret);
    uint64_t pc_step = 2 + (2 * il);
    cpu.vcpu->regs->elr_el2 += pc_step;
}

abort_handler_t abort_handlers[64] = {[ESR_EC_DALEL] = aborts_data_lower,
                                      [ESR_EC_SMC64] = smc64_handler};

void aborts_sync_handler()
{
    uint32_t esr = 0;
    uint64_t far = 0;
    uint64_t hpfar = 0;
    uint64_t ipa_fault_addr = 0;
    MRS(esr, ESR_EL2);
    MRS(far, FAR_EL2);
    MRS(hpfar, HPFAR_EL2);

    ipa_fault_addr = (far & 0xFFF) | (hpfar << 8);

    uint32_t ec = bit_extract(esr, ESR_EC_OFF, ESR_EC_LEN);
    uint32_t il = bit_extract(esr, ESR_IL_OFF, ESR_IL_LEN);
    uint32_t iss = bit_extract(esr, ESR_ISS_OFF, ESR_ISS_LEN);

    abort_handler_t handler = abort_handlers[ec];
    if (handler)
        handler(iss, ipa_fault_addr, il);
    else
        ERROR("no handler for abort ec = 0x%x", ec);  // unknown guest exception
}
