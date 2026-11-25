# MsgBus System Architecture & OS Design Document

**Course**: Operating Systems (25CS2104E)  
**Project**: MsgBus — High-Performance Publish/Subscribe Message Bus over Shared Memory  
**Target Environment**: Linux (Ubuntu 24.04 LTS / WSL2)  
**Standard**: C11 with POSIX.1-2008 Real-Time Extensions (`-lrt`, `-pthread`)  
**Integrated With**: ShellForge Unix Shell  

---

## 1. Executive Summary & Core Objectives

MsgBus is an asynchronous, decoupled publish/subscribe message bus engineered in C11. It enables multiple concurrent producer processes (publishers) and consumer processes (subscribers) to exchange structured messages over named topics with **zero kernel-buffer copies** and **zero busy-waiting (0.0% idle CPU usage)**.

### Architectural Hallmarks
1. **POSIX Shared-Memory Ring Buffers**: Memory-mapped circular buffers (`shm_open`, `ftruncate`, `mmap`) providing ultra-low latency inter-process communication.
2. **Process-Shared Synchronization Primitives**: Coordination via robust mutexes (`PTHREAD_MUTEX_ROBUST`, `PTHREAD_PROCESS_SHARED`), condition variables (`pthread_cond_t`), and POSIX named counting semaphores (`sem_open`, `sem_wait`, `sem_post`).
3. **Per-Subscriber Fan-Out Offset Tracking**: Independent sequence read cursors allowing all subscribers to receive each published message exactly once.
4. **Bounded Queue Policies**: Dynamic backpressure options (`POLICY_BLOCK` for guaranteed delivery vs. `POLICY_DROP_OLDEST` for real-time loss tolerance).
5. **On-Disk Durability & Late-Subscriber Replay**: Write-ahead logging (`.wal`) allowing subscribers that attach after publication to replay historical messages.
6. **Crash-Resilient Teardown Discipline**: Self-healing against subscriber abrupt death (`kill -9`, zombies), accompanied by a dedicated resource janitor (`msgbus-clean`).
7. **Native ShellForge Integration**: All broker, publisher, subscriber, cleaner, and benchmark binaries run natively under the **ShellForge** Unix Shell with job control (`&`).

---

## 2. Shared-Memory Memory Model & Struct Alignment

### 2.1 Segment Layout (`/dev/shm/msgbus_shm_<topic>`)

Every topic exists as an isolated POSIX shared-memory segment. The memory layout is strictly structured and byte-aligned:

```
+-----------------------------------------------------------------------------+
|                     msgbus_topic_shm_t (34,408 bytes)                       |
+-----------------------------------------------------------------------------+
| 0x0000 | pthread_mutex_t lock          (Process-shared robust mutex)        |
| 0x0028 | pthread_cond_t  has_data_cv   (Broadcasts data to subscribers)     |
| 0x0058 | pthread_cond_t  has_space_cv  (Signals publishers on free slot)    |
| 0x0088 | char topic_name[64]           (Null-terminated topic identifier)   |
| 0x00C8 | uint32_t capacity             (32 circular ring slots)             |
| 0x00D0 | uint64_t head_seq             (Monotonically increasing counter)   |
| 0x00D8 | uint32_t active_subscribers   (Attached subscriber count)          |
| 0x00DC | bool durable                  (Enable on-disk write-ahead log)     |
| 0x00DD | char wal_path[256]            (Path to durability log file)        |
+-----------------------------------------------------------------------------+
| 0x01E0 | msgbus_subscriber_t subscribers[16] (Subscriber Table)            |
|        | - pid_t pid                   (Process ID for liveness check)      |
|        | - uint64_t read_seq           (Current consumer cursor)            |
|        | - bool active                 (Slot allocation flag)               |
|        | - msgbus_policy_t policy      (BLOCK or DROP_OLDEST)               |
+-----------------------------------------------------------------------------+
| 0x03E0 | msgbus_msg_t ring[32]         (Circular Message Ring Buffer)       |
|        | [Slot 0 .. Slot 31]           (1,048 bytes per message slot)       |
|        | - uint64_t seq                (Sequence number)                    |
|        | - uint32_t len                (Payload length)                     |
|        | - pid_t sender_pid            (Publisher process ID)               |
|        | - uint64_t timestamp_ns       (Nanosecond CLOCK_REALTIME)          |
|        | - char payload[1024]          (Message payload payload buffer)     |
+-----------------------------------------------------------------------------+
```

### 2.2 Pointer Arithmetic & Wrap-Around Math
The ring buffer capacity is configured as a power of two ($N = 32$). Slot indexing uses fast modulo arithmetic:
$$\text{slot} = \text{seq} \pmod{\text{capacity}}$$
Monotonic 64-bit sequence numbers (`head_seq`, `read_seq`) prevent integer overflow issues over decades of continuous operation.

---

## 3. Synchronization & Zero Busy-Waiting Architecture

### 3.1 Coordination Mechanics

| Primitive | Attribute | Role in MsgBus |
| :--- | :--- | :--- |
| **`pthread_mutex_t`** | `PTHREAD_PROCESS_SHARED`, `PTHREAD_MUTEX_ROBUST` | Mutual exclusion protecting shared-memory state across arbitrary processes. Recovers via `pthread_mutex_consistent` on owner crash. |
| **`pthread_cond_t has_data_cv`** | `PTHREAD_PROCESS_SHARED` | Wakes sleeping subscribers immediately when a new message is published (`pthread_cond_broadcast`). |
| **`pthread_cond_t has_space_cv`**| `PTHREAD_PROCESS_SHARED` | Wakes blocked publishers when a slow subscriber advances its read cursor. |
| **`sem_t sem_free`** | POSIX Named Counting Semaphore | Reflects available capacity slots in the ring buffer. |
| **`sem_t sem_used`** | POSIX Named Counting Semaphore | Reflects unconsumed messages in the ring buffer. |

### 3.2 Proof of Zero Busy-Waiting
Subscribers do not poll in tight loops. When `read_seq >= head_seq`, subscribers sleep inside the Linux kernel futex via `pthread_cond_wait` / `pthread_cond_timedwait`. Idle CPU utilization is verified at **0.0%** by `tests/verify_zero_cpu.sh`.

---

## 4. Fan-Out Delivery & Bounded-Queue Policies

### 4.1 Independent Subscriber Offsets
Each subscriber occupies a slot in `shm->subscribers[0..15]`. When a message is written to slot $\text{head\_seq} \pmod{32}$, no data is copied. Each subscriber reads directly from shared memory at its personal `read_seq`.

### 4.2 Bounded-Queue Policies
When publishers outpace subscribers:
1. **`POLICY_BLOCK`**:
   The publisher evaluates all active subscribers. If any subscriber lags by $\ge \text{capacity}$ slots, the publisher blocks on `has_space_cv`. **Guarantees zero message loss.**
2. **`POLICY_DROP_OLDEST`**:
   The publisher never waits for this subscriber. When the slow subscriber awakens and detects `head_seq - read_seq > capacity`, it fast-forwards its offset:
   $$\text{read\_seq} = \text{head\_seq} - \text{capacity}$$
   The subscriber drops unread historical messages and continues with the newest available stream.

---

## 5. Crash Recovery & Teardown Discipline (Week 8)

### 5.1 Dead Subscriber Detection
If a subscriber with `POLICY_BLOCK` terminates abnormally (`kill -9`, segmentation fault, unhandled abort), future publishers would deadlock if waiting indefinitely.  
MsgBus implements **liveness probing** in `client.c`:
* Probes process existence via `kill(pid, 0)`.
* Inspects `/proc/<pid>/stat` for Zombie (`Z`) or Dead (`X`) states.
* On detecting a dead process, MsgBus automatically reclaims its subscriber slot, decrements `active_subscribers`, and broadcasts `has_space_cv` to unblock publishers.

### 5.2 Signal-Safe Unlinking
Brokers and tools install `sigaction` handlers for `SIGINT` and `SIGTERM`. Upon receiving termination signals, they gracefully call:
* `shm_unlink("/msgbus_shm_<topic>")`
* `sem_unlink("/msgbus_sem_mtx_<topic>")`
* `sem_unlink("/msgbus_sem_fre_<topic>")`
* `sem_unlink("/msgbus_sem_usd_<topic>")`

### 5.3 Janitor Utility (`msgbus-clean`)
If an entire container or host terminates abruptly without signal handling, `bin/msgbus-clean --all` sweeps `/dev/shm` and unlinks all lingering MsgBus segments and semaphores.

---

## 6. Durability & Replay Architecture (Week 9 - Distinction Goal)

Topics created with the `--durable` flag maintain an on-disk Write-Ahead Log (`/tmp/msgbus_<topic>.wal`):
1. **Append Log**: Every published message slot is sequentially appended to the WAL file.
2. **Late-Subscriber Replay**: Subscribers initiated with `--replay` first read historical messages from the WAL file starting at Sequence 0 up to `head_seq`.
3. **Seamless Hand-off**: Upon reaching EOF in the WAL file, the subscriber seamlessly transitions to the live shared-memory ring buffer.

---

## 7. Performance Benchmark: POSIX Shared Memory vs. UNIX Domain Sockets (Week 10)

The `msgbus-bench` executable conducts high-throughput stress testing comparing:
1. **POSIX Shared-Memory Ring Buffer**: Zero-copy IPC with futex notifications.
2. **UNIX Domain Sockets (`AF_UNIX`, `SOCK_STREAM`)**: Kernel-space stream IPC via `socketpair`.

### Empirical Results (15,000 Messages, 128-Byte Payloads)
```
+----------------------+------------+------------+--------------------+------------------+
| Transport Mechanism  | Messages   | Time (ms)  | Throughput (msg/s) | Avg Latency (us) |
+----------------------+------------+------------+--------------------+------------------+
| POSIX Shared Memory  | 15,000     | 46.39      | 323,357 msg/s      | 3.09 us          |
| UNIX Domain Socket   | 15,000     | 10.47      | 1,433,274 msg/s    | 0.70 us          |
+----------------------+------------+------------+--------------------+------------------+
```
*Note on Architectural Trade-off*:  
UNIX sockets batch raw contiguous byte streams within kernel socket buffers without per-message process synchronization. Shared-memory pub/sub enforces full transactional integrity (64-bit monotonic sequence tags, sender PIDs, nanosecond timestamps, robust mutex lock/unlocks, and condition-variable broadcasts) across arbitrary independent processes while eliminating kernel buffer allocations.

---

## 8. Integration with ShellForge Unix Shell

MsgBus runs directly inside **ShellForge**:
* **Background Job Control (`&`)**: ShellForge tracks the broker (`[1] PID`) and subscriber (`[2] PID`) as asynchronous jobs.
* **Redirection & Pipelines (`>`, `|`)**: ShellForge redirects subscriber outputs or pipes messages between tools.
* **Process Lifecycles**: ShellForge monitors child exit statuses and cleans up zombie processes.

---

## 9. 12-Week Syllabus & Viva Acceptance Matrix

| Syllabus Week | OS Concept | Acceptance Test | Status |
| :--- | :--- | :--- | :---: |
| **W1** | Syscall boundary · user vs kernel mode | `tests/test_w1_single_slot.c` (`shm_open`, `ftruncate`, `mmap`) | ✅ PASS |
| **W2** | C toolchain · memory model & alignment | `tests/test_w2_ring_layout.c` (Ring buffer layout & wrap-around) | ✅ PASS |
| **W3** | Lexing & parsing · topic table | `src/broker/topic_table.c` (Broker dynamic catalog) | ✅ PASS |
| **W4** | Processes · fork / exec / wait | CLI multi-process execution (`msgbus-pub`, `msgbus-sub`) | ✅ PASS |
| **W5** | Exec & PATH · client API | `lib/libmsgbus.a` modular library linking | ✅ PASS |
| **W6** | Signals · asynchronous control | `SIGINT`/`SIGTERM` handlers ensuring clean unlinking | ✅ PASS |
| **W7** | Producer / Consumer · semaphore coordination | `tests/test_w7_multiprocess.sh` & `tests/verify_zero_cpu.sh` (0.0% CPU) | ✅ PASS |
| **W8** | Memory discipline · crash recovery | `tests/test_w8_cleanup.sh` (`bin/msgbus-clean` tool) | ✅ PASS |
| **W9** | Files & persistence · durability replay | `tests/test_w9_durability.sh` (`--durable` & `--replay`) | ✅ PASS |
| **W10** | Concurrency & benchmark | `tests/test_w10_benchmark.sh` (`bin/msgbus-bench`) | ✅ PASS |
| **W11** | Deadlock-free soak test | `tests/test_w11_soak.sh` (Concurrent publishers + dead-process kill) | ✅ PASS |
| **W12** | Packaging & ShellForge integration | `tests/test_in_shellforge.sh` & `run_msgbus_demo.sh` | ✅ PASS |
