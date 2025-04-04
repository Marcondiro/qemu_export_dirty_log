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

bool start_dirty_log_export(Error **errp);
void stop_dirty_log_export(Error **errp);
void loadvm_for_hotreload(Error **errp, const char *name);
void hotreload(Error **errp);
#endif
