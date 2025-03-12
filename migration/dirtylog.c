/*
 * Dirty log export implement code
 *
 * Copyright (c) 2025 Marco Cavenati
 *
 * Authors:
 *  Marco Cavenati <cavenati.marco+qemu@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "dirtylog.h"
#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "monitor/hmp.h"
#include "monitor/monitor.h"
#include "system/kvm.h"
#include "exec/memory.h"

/* CPU generation id */
static unsigned int gen_id;

/* Dirtied pages between start_dirty_log_export and stop */
GHashTable* dirty_log_hash_set = NULL;

static void serialize_entry(gpointer key, gpointer value, gpointer user_data)
{
    FILE *file = user_data;
    struct dirty_gfn *gfn = key;
    
    assert(file != NULL);
    assert(gfn != NULL);

    fprintf(file, "0x%08x 0x%016llx\n", gfn->slot, gfn->offset);
}

static bool serialize_dirty_log_hash_set(Error **errp)
{
    FILE *file;
    char file_name[64] = {0};
    
    if (!dirty_log_hash_set) {
        error_setg(errp, "dirty_log_hash_set is NULL");
        return false;
    }

    snprintf(file_name, sizeof(file_name), "dirty_log_%li", time(NULL));
    file = fopen(file_name, "w");
    if (!file) {
        error_setg(errp, "Failed to open dirty_log file");
        return false;
    }

    g_hash_table_foreach(dirty_log_hash_set, serialize_entry, file);

    fclose(file);
    return true;
}

static guint dirty_gfn_hash(gconstpointer key)
{
    const struct dirty_gfn *gfn = key;
    const guint offset_hash = g_int64_hash(&gfn->offset);
    const guint slot_hash = g_int_hash(&gfn->slot);
    return offset_hash ^ slot_hash;
}

static gboolean entry_equal(gconstpointer a, gconstpointer b) {
    const struct dirty_gfn  *gfn_a = a;
    const struct dirty_gfn  *gfn_b = b;
    return (gfn_a->offset == gfn_b->offset) && (gfn_a->slot == gfn_b->slot);
}

static void start_dirty_log_export(Error **errp)
{
    bool ret;

    if (global_dirty_tracking & GLOBAL_DIRTY_EXPORT)
    {
        error_setg(errp, "Dirty tracking export is already running!");
        return;
    }

    /*
     * dirty_log_export only works when kvm dirty ring is enabled.
     */
    if (!kvm_dirty_ring_enabled())
    {
        error_setg(errp, "dirty ring is not enabled! run Qemu with -accel kvm,dirty-ring-size=4096");
        return;
    }

    dirty_log_hash_set = g_hash_table_new_full(dirty_gfn_hash, entry_equal, g_free, NULL);
    ret = memory_global_dirty_log_start(GLOBAL_DIRTY_EXPORT, errp);
    if (!ret) {
        g_hash_table_destroy(dirty_log_hash_set);
        dirty_log_hash_set = NULL;
        return;
    }

    WITH_QEMU_LOCK_GUARD(&qemu_cpu_list_lock)
    {
        gen_id = cpu_list_generation_id_get();
    }
}

static void stop_dirty_log_export(Error **errp)
{
    if (!(global_dirty_tracking & GLOBAL_DIRTY_EXPORT))
    {
        error_setg(errp, "Dirty tracking export is not running!");
        return;
    }

    WITH_QEMU_LOCK_GUARD(&qemu_cpu_list_lock)
    {
        if (gen_id != cpu_list_generation_id_get())
        {
            error_setg(errp, "The cpus changed while tracking, this is not handled");
        }
    }

    memory_global_dirty_log_sync(false);
    memory_global_dirty_log_stop(GLOBAL_DIRTY_EXPORT);

    serialize_dirty_log_hash_set(errp);
    g_hash_table_destroy(dirty_log_hash_set);
    dirty_log_hash_set = NULL;
}


void hmp_start_dirty_log_export(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;

    start_dirty_log_export(&err);
    
    if (err) {
        hmp_handle_error(mon, err);
        return;
    }

    monitor_printf(mon, "Starting dirty log export\n");
}

void hmp_stop_dirty_log_export(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;

    stop_dirty_log_export(&err);
    
    if (err) {
        hmp_handle_error(mon, err);
        return;
    }

    monitor_printf(mon, "Stopping dirty log export\n");
}
