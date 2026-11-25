# MsgBus — Publish/Subscribe Message Bus (Weeks 1 to 7)

An inter-process communication (IPC) publish/subscribe bus written in **C11** for Linux. Decoupled publisher and subscriber processes exchange messages over named topics using **POSIX shared-memory ring buffers** (`shm_open`, `mmap`, `ftruncate`) coordinated by **POSIX semaphores and process-shared condition variables** (`sem_open`, `sem_wait`, `sem_post`, `pthread_cond_t`) with **zero busy-waiting**.

---

## 📚 12-Week Syllabus Alignment (Completed up to Week 7)

| Week | OS Course Concept | MsgBus Implementation in this Codebase |
| :--- | :--- | :--- |
| **W1** | **Syscall boundary · User vs. Kernel Mode** | `shm_open`, `ftruncate`, `mmap`, `munmap`, `shm_unlink`. Validated in `tests/test_w1_single_slot.c`. |
| **W2** | **C Toolchain · Memory Model & Alignment** | Topic ring-buffer layout in shared memory (`struct msgbus_topic_shm_t`), struct alignment, monotonic sequence IDs. Validated in `tests/test_w2_ring_layout.c`. |
| **W3** | **Lexing & Parsing · Topic Table** | Message descriptors (`msgbus_msg_t`), timestamps, PID tracking, and the Broker daemon's topic catalog (`src/broker/topic_table.c`). |
| **W4** | **Processes · Fork / Exec / Scheduling** | Multi-process model: independent Broker daemon, CLI Publisher (`msgbus-pub`), and CLI Subscriber (`msgbus-sub`). Subscriber registry table inside shared memory. |
| **W5** | **Exec & PATH · Client Library API** | Modular client library `libmsgbus.a` providing `msgbus_connect`, `msgbus_publish`, `msgbus_subscribe`, `msgbus_recv`, and `msgbus_disconnect`. |
| **W6** | **Signals · Asynchronous Teardown** | `sigaction` handlers for `SIGINT` / `SIGTERM` ensuring clean teardown and unlinking of all `/dev/shm/msgbus_*` and `/dev/shm/sem.msgbus_*` files even on abrupt termination. |
| **W7** | **Producer / Consumer · Semaphore Coordination** | **The Core Milestone**: POSIX counting semaphores (`sem_free`, `sem_used`) & process-shared condition variables (`has_data_cv`, `has_space_cv`). Bounded-queue policies (`POLICY_BLOCK`, `POLICY_DROP_OLDEST`) with **0% idle CPU usage** (proven by `tests/verify_zero_cpu.sh`). |

---

## 🏗️ Architecture & Shared-Memory Layout

```
                  +----------------------------------------------+
                  |         MsgBus Broker Daemon (PID: 100)      |
                  |  - Creates & monitors topics                 |
                  |  - Signal-safe teardown (SIGINT / SIGTERM)   |
                  +----------------------------------------------+
                                         |
                       Creates & manages /dev/shm/msgbus_shm_<topic>
                                         v
   +-----------------------------------------------------------------------------+
   |                     POSIX Shared Memory Segment (mmap)                      |
   |                                                                             |
   |  [Sync Primitives]  pthread_mutex_t (PTHREAD_MUTEX_ROBUST, PROCESS_SHARED)   |
   |                     pthread_cond_t  has_data_cv (Wakes waiting subscribers) |
   |                     pthread_cond_t  has_space_cv (Wakes blocked publishers) |
   |  [Named Sems]       /msgbus_sem_mtx_*, /msgbus_sem_fre_*, /msgbus_sem_usd_* |
   |  [Metadata]         topic_name, capacity (32), head_seq (monotonically +1)  |
   |  [Sub Table]        subscribers[0..15] -> { pid, read_seq, active, policy } |
   |  [Ring Buffer]      slots[0..31]       -> { seq, len, pid, timestamp, payload } |
   +-----------------------------------------------------------------------------+
             ^                                           |
             | writes messages                           | reads at sub->read_seq
   +-----------------------+                    +-----------------------+
   | Publisher (msgbus-pub)|                    | Subscriber(msgbus-sub)|
   | (PID: 201, 202, ...)  |                    | (PID: 301, 302, ...)  |
   +-----------------------+                    +-----------------------+
```

---

## 🐧 How to Save and Run in Ubuntu

### Option A: Using Ubuntu on Windows via WSL (Your Current Setup)

Your Windows folder is directly mounted inside Ubuntu at `/mnt/c/`:

1. **Open your Ubuntu Terminal** (or run `wsl` in PowerShell).
2. **Navigate to the project directory**:
   ```bash
   cd /mnt/c/Users/asolo/.gemini/antigravity/scratch/msgbus
   ```
3. **Build everything**:
   ```bash
   make clean
   make all
   ```
4. **Run the automated acceptance test suite**:
   ```bash
   make test
   ```

---

### Option B: Copying to a Native Ubuntu Machine / VM

If you want to copy this project to a separate Ubuntu laptop or VM:

1. **Zip or copy the `msgbus` folder** to your Ubuntu machine:
   ```bash
   tar -czvf msgbus.tar.gz msgbus/
   ```
2. **On your Ubuntu machine, install the build tools** (if not already installed):
   ```bash
   sudo apt update
   sudo apt install -y build-essential gcc make
   ```
3. **Extract and compile**:
   ```bash
   tar -xzvf msgbus.tar.gz
   cd msgbus
   make all
   ```

---

## 🚀 Step-by-Step Interactive Demo (3 Separate Terminals)

Open **3 separate Ubuntu terminals** to test the system in action:

### Terminal 1: Start the Broker Daemon
```bash
./bin/msgbus-broker sports
```
*Output:*
```text
=====================================================
      MsgBus Broker Daemon (Milestone 1 to 7)
 Decoupled Pub/Sub over POSIX Shared-Memory Rings
=====================================================
[broker] Initialized topic: sports
[broker] Broker running (PID: 1234). Press Ctrl+C to terminate cleanly.
```

---

### Terminal 2: Start the Subscriber (Zero Busy-Waiting)
```bash
./bin/msgbus-sub sports --policy block
```
*Output:*
```text
[sub:1235] Connecting to topic 'sports' (policy: BLOCK)...
[sub:1235] Subscribed successfully. Waiting for messages (zero busy-waiting)...
```
*(Notice: The subscriber is sleeping on the semaphore/condvar. CPU usage is exactly 0.0%)*

---

### Terminal 3: Publish Messages
```bash
# Publish 5 messages with 200ms delay between them
./bin/msgbus-pub sports "Goal scored!" -n 5 -d 200
```
*Output in Terminal 3:*
```text
[pub:1236] Connecting to topic 'sports'...
[pub:1236] Published: "Goal scored! #1"
[pub:1236] Published: "Goal scored! #2"
[pub:1236] Published: "Goal scored! #3"
[pub:1236] Published: "Goal scored! #4"
[pub:1236] Published: "Goal scored! #5"
```

*Output in Terminal 2 (Subscriber instantly receives every message):*
```text
[sub:1235] Received [Seq #0 | From PID 1236]: "Goal scored! #1"
[sub:1235] Received [Seq #1 | From PID 1236]: "Goal scored! #2"
[sub:1235] Received [Seq #2 | From PID 1236]: "Goal scored! #3"
[sub:1235] Received [Seq #3 | From PID 1236]: "Goal scored! #4"
[sub:1235] Received [Seq #4 | From PID 1236]: "Goal scored! #5"
```

---

## 🔍 Verifying OS Acceptance Criteria

### 1. Verify Zero Busy-Waiting (0.0% CPU)
Run while a subscriber is waiting:
```bash
bash tests/verify_zero_cpu.sh
```
*Result:*
```text
Subscriber (PID 385) CPU usage while idle: 0.0%
Zero busy-waiting verified successfully (CPU < 1%).
```

### 2. Verify Signal-Safe Cleanup & No Orphaned SHM
Press `Ctrl+C` in Terminal 1 (Broker). The broker catches `SIGINT` and executes:
```text
[broker] Caught shutdown signal! Unlinking all shared memory & semaphores...
[broker] Cleaning up topic: sports
[broker] Teardown complete. Exiting.
```
Confirm no files are leaked in `/dev/shm`:
```bash
ls -la /dev/shm/msgbus_*
```
*(No files remain!)*
