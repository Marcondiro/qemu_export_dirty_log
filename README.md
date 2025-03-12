# QEMU with KVM dirty log export capability

## How to use this

After building (see official QEMU docs), run QEMU with the following options:

```
build/qemu-system-x86_64 \
    -accel kvm,dirty-ring-size=4096 \
    -monitor "tcp::4444,server,nowait"
```

Connect to the monitor with `nc localhost 4444` and start/stop the logging as needed with `start_dirty_log_export` and `stop_dirty_log_export`.

On stop, the dirty log will be written to `dirty_log_[TIMESTAMP]` in the current directory, each row has the format `slot gfn`.

To have more visibility on the dirty log, you can add the following tracing option to QEMU cli:

```
    -trace enable="kvm_dirty_ring_*"
```

## How it works

KVM has the ability to [track dirty memory pages](https://www.kernel.org/doc/html/latest/virt/kvm/api.html#kvm-cap-dirty-log-ring-kvm-cap-dirty-log-ring-acq-rel) in a guest VM.
The dirty log is a ring buffer that is filled by KVM and read by QEMU.

QEMU does not have a way to export the dirty log to a file, so this fork adds a new monitor command to start and stop the export of the dirty log to a file. To avoid duplicates in the file and to have good performance, the file is wtritten only on stop and the dirty log is kept in memory until then.

At the time of writing, the dirty log is used by 3 different functionalities in QEMU:
- Live migration
- Dirty rate
- Dirty limit

The export functionality does not conflict with these functionalities.
