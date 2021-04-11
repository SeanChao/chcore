# Userspace Processes and Exceptions

## Resource management for processes

Physical resources are abstracted into **capabilities**. Each capability corresponds to some physical resource.

A process who owns a list of resources has a list of capabilities. `slot_table` is the data structure to save the list.

## Creating User Process

```text
process_create_root
├── process_create
│   ├── process_init
│   ├── alloc_slot_id
│   ├── vmspace_init
│   └── obj_alloc, cap_alloc (for vmspace)
└── thread_create_main
    ├── pmo_init
    ├── vmspace_map_range
    ├── prepare_env
    └── thread_init
```

In this paticular lab, `ramdisk_read_file` is called to load a user program into pointer `binary`.

Then, `process_create` is called to create a process. It allocates a `TYPE_PROCESS` resource, a slot id. After that, `vmspace` is allocated for the process.

In `thread_create_main`, `pmo_init` initializes user stack and `vmspace_map_range` maps the memory space.
Then a thread is allocated, pc is set to the entry point of the program (returned by `load_binary`).
stack is set to `stack_base + stack_size`. `prepare_env` initializes the thread environment, including `argc`, `argv`, `envp`.

Finally, `thread_init` initializes the thread and adds it to the process.

## System Call

System calls are sync exceptions triggerred by the user process.
In `sync_el0_64`, a system-call has `esr_el1 >> #ESR_EL1_EC_SHIFT == #ESR_EL1_EC_SVC_64`, so it jumps to `el0_syscall`.
