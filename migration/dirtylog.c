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
#include "ram.h"
#include "migration/snapshot.h"
#include "system/runstate.h"

/* CPU generation id */
static unsigned int gen_id;

/* Dirtied pages between start_dirty_log_export and stop */
GHashTable *dirty_log_hash_set = NULL;

static void serialize_entry(gpointer key, gpointer value, gpointer user_data)
{
    FILE *file = user_data;
    u_int64_t *paddr = value;

    assert(file != NULL);
    assert(paddr != NULL);

    fprintf(file, "0x%016lx\n", *paddr);
}

static bool serialize_dirty_log_hash_set(Error **errp)
{
    FILE *file;
    char file_name[64] = {0};

    if (!dirty_log_hash_set)
    {
        error_setg(errp, "dirty_log_hash_set is NULL");
        return false;
    }

    snprintf(file_name, sizeof(file_name), "dirty_log_%li", time(NULL));
    file = fopen(file_name, "w");
    if (!file)
    {
        error_setg(errp, "Failed to open dirty_log file");
        return false;
    }

    g_hash_table_foreach(dirty_log_hash_set, serialize_entry, file);

    fclose(file);
    return true;
}

bool start_dirty_log_export(Error **errp)
{
    bool ret;
    /*
     * dirty_log_export only works when kvm dirty ring is enabled.
     */
    if (!kvm_dirty_ring_enabled())
    {
        error_setg(errp, "dirty ring is not enabled! run Qemu with -accel kvm,dirty-ring-size=4096");
        return false;
    }

    if (dirty_log_hash_set)
    {
        g_hash_table_remove_all(dirty_log_hash_set);
    }
    else
    {
        dirty_log_hash_set = g_hash_table_new_full(g_int64_hash, g_int64_equal, g_free, g_free);
    }

    ret = memory_global_dirty_log_start(GLOBAL_DIRTY_TO_HASHMAP, errp);
    if (!ret)
    {
        g_hash_table_destroy(dirty_log_hash_set);
        dirty_log_hash_set = NULL;
        return false;
    }

    WITH_QEMU_LOCK_GUARD(&qemu_cpu_list_lock)
    {
        gen_id = cpu_list_generation_id_get();
    }
    
    return true;
}

void stop_dirty_log_export(Error **errp)
{
    if (!(global_dirty_tracking & GLOBAL_DIRTY_TO_HASHMAP))
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
    memory_global_dirty_log_stop(GLOBAL_DIRTY_TO_HASHMAP);
}

void loadvm_for_hotreload(Error **errp, const char *name)
{
    RunState saved_state = runstate_get();

    vm_stop(RUN_STATE_RESTORE_VM);

    if (load_snapshot(name, NULL, false, NULL, errp))
    {
        if (start_dirty_log_export(errp))
        {
            global_hotreload = GLOBAL_HOTRELOAD_PREPARE;
            size_t len = strlen(name) + 1;
            hotreload_snapshot = malloc(len);
            strncpy(hotreload_snapshot, name, len);
        }
        load_snapshot_resume(saved_state);
    }
}

void hotreload(Error **errp)
{
    if (!hotreload_snapshot || global_hotreload != GLOBAL_HOTRELOAD_PREPARE)
    {
        error_setg(errp, "Hotreload not set up. Use loadvm_for_hotreload before this.");
        return;
    }

    RunState saved_state = runstate_get();

    global_hotreload = GLOBAL_HOTRELOAD_LOADVM;

    vm_stop(RUN_STATE_RESTORE_VM);
    stop_dirty_log_export(errp);
    if (*errp)
    {
        // fallback to normal reload in case stop_dirty_log_export fails
        global_hotreload = GLOBAL_HOTRELOAD_OFF;
    }

    if (load_snapshot(hotreload_snapshot, NULL, false, NULL, errp))
    {
        start_dirty_log_export(errp);
        load_snapshot_resume(saved_state);
    }
    else
    {
        free(hotreload_snapshot);
        hotreload_snapshot = NULL;
    }

    global_hotreload = GLOBAL_HOTRELOAD_PREPARE;
}

void hmp_start_dirty_log_export(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;

    start_dirty_log_export(&err);

    if (err)
    {
        hmp_handle_error(mon, err);
        return;
    }

    monitor_printf(mon, "Starting dirty log export\n");
}

void hmp_stop_dirty_log_export(Monitor *mon, const QDict *qdict)
{
    Error *err = NULL;

    stop_dirty_log_export(&err);

    if (!err)
    {
        serialize_dirty_log_hash_set(&err);
    }

    if (err)
    {
        hmp_handle_error(mon, err);
        return;
    }

    monitor_printf(mon, "Stopping dirty log export\n");
}
