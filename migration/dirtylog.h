/*
 * Dirty log export common functions
 *
 * Copyright (c) 2025 Marco Cavenati
 *
 * Authors:
 *  Marco Cavenati <cavenati.marco+qemu@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef QEMU_MIGRATION_DIRTYLOG_H
#define QEMU_MIGRATION_DIRTYLOG_H

#include "qemu/osdep.h"

extern GHashTable* dirty_log_hash_set;

/* Similar to kvm_dirty_gfn, but without flags */
struct dirty_gfn {
	__u32 slot;
	__u64 offset;
};

#endif
