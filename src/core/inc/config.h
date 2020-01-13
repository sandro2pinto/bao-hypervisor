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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <bao.h>
#include <platform.h>

extern uint64_t _config_end, _images_end;

#define VM_CONFIG_HEADER_SIZE ((size_t)&_config_end)
#define VM_CONFIG_SIZE ((size_t)&_images_end)

#define VM_IMAGE_OFFSET(vm_name) ((uint64_t)&_##vm_name##_vm_beg)
#define VM_IMAGE_SIZE(vm_name) ((size_t)&_##vm_name##_vm_size)

#define VM_IMAGE(vm_name, image)                                          \
    extern uint64_t _##vm_name##_vm_size;                                 \
    extern uint64_t _##vm_name##_vm_beg;                                  \
    asm(".section .vm_image_" #vm_name                                    \
        ", \"a\"\n\t"                                                     \
        ".global _" #vm_name                                              \
        "_vm_beg\n\t"                                                     \
        "_" #vm_name                                                      \
        "_vm_beg:\n\t"                                                    \
        ".incbin \"" #image                                               \
        "\"\n\t"                                                          \
        "_" #vm_name                                                      \
        "_vm_end:\n\t"                                                    \
        ".global _" #vm_name                                              \
        "_vm_size\n\t"                                                    \
        ".set _" #vm_name "_vm_size,  (_" #vm_name "_vm_end - _" #vm_name \
        "_vm_beg)\n\t");

#define VM_CONFIG_HEADER                           \
    .fdt_header =                                  \
        {                                          \
            .magic = 0xedfe0dd0,                   \
            .totalsize = 0x28000000,               \
            .version = 0x11000000,                 \
            .last_comp_version = 0x2000000,        \
    },                                             \
    .vmconfig_header_size = VM_CONFIG_HEADER_SIZE, \
    .vmconfig_size = VM_CONFIG_SIZE,

typedef struct {
    struct {
        /* Image load address in VM's address space */
        uint64_t base_addr;
        /* Image load address in hyp address space */
        uint64_t load_addr;
        /* Image size */
        size_t size;
    } image;

    /* Entry point address in VM's address space */
    uint64_t entry;
    /**
     * A bitmap signaling the preferred physical cpus assigned to the VM.
     * If this value is each mutual exclusive for all the VMs, this field
     * allows to direcly assign specific physical cpus to the VM.
     */
    uint64_t cpu_affinity;

    /**
     * A bitmap for the assigned colors of the VM. This value is truncated
     * depending on the number of available colors calculated at runtime
     */
    uint64_t colors;

    /**
     * A description of the virtual platform available to the guest, i.e.,
     * the virtual machine itself.
     */

    struct platform_desc platform;

} vm_config_t;

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

extern struct vm_config {
    /**
     *  Faking the fdt header allows to boot using u-boot mechanisms passing
     * this configuration as the dtb.
     */
    struct fdt_header fdt_header;

    /* The of this struct aligned to page size */
    size_t vmconfig_header_size;
    /* The size of the full configuration binary, including VM images */
    size_t vmconfig_size;

    /* Hypervisor colors */
    uint64_t hyp_colors;

    /* The number of VMs specified by this configuration */
    size_t vmlist_size;

    /* Array list with VM configuration */
    vm_config_t vmlist[];

} vm_config __attribute__((section(".config")));

void config_adjust_to_va(struct vm_config *config, uint64_t phys);

#endif /* __CONFIG_H__ */
