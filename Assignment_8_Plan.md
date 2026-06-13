# Assignment 8 — AESD Character Driver

## 1. Assignment Overview

Assignment 8 requires implementing a **Linux kernel character device driver** (`/dev/aesdchar`) that stores newline-terminated write commands in a circular buffer and returns them on read. It also requires modifying the existing `aesdsocket` server to write/read through `/dev/aesdchar` instead of the flat file `/var/tmp/aesdsocketdata`.

```
┌──────────────────────────────────────────────────────────────┐
│                     Two Testing Paths                        │
│                                                              │
│  Path A: Standalone Driver Test                              │
│  ┌────────────┐       ┌────────────────┐                     │
│  │ drivertest │──────►│ /dev/aesdchar  │                     │
│  │   .sh      │ echo/ │  (char driver) │                     │
│  │            │ cat   │                │                     │
│  └────────────┘       └────────────────┘                     │
│                                                              │
│  Path B: Socket → Driver Integration Test                    │
│  ┌────────────┐       ┌────────────┐       ┌──────────────┐  │
│  │ sockettest │──nc──►│ aesdsocket │──────►│ /dev/aesdchar│  │
│  │   .sh      │       │  (server)  │ open/ │ (char driver)│  │
│  └────────────┘       └────────────┘ r/w   └──────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Architecture

### 2.1 Component Diagram

```
 User Space                            Kernel Space
 ─────────                             ────────────
 ┌──────────────┐                      ┌──────────────────────────────┐
 │  aesdsocket  │   open/read/write    │     aesd char driver         │
 │  (modified)  │─────────────────────►│                              │
 │              │                      │  ┌────────────────────────┐  │
 │ Writes to    │                      │  │   aesd_dev structure   │  │
 │ /dev/aesdchar│                      │  │                        │  │
 │ instead of   │                      │  │  ┌──────────────────┐  │  │
 │ /var/tmp/... │                      │  │  │ circular_buffer  │  │  │
 └──────────────┘                      │  │  │ [0] "write1\n"   │  │  │
                                       │  │  │ [1] "write2\n"   │  │  │
 ┌──────────────┐                      │  │  │ ...              │  │  │
 │  drivertest  │   echo/cat           │  │  │ [9] "write10\n"  │  │  │
 │    .sh       │─────────────────────►│  │  └──────────────────┘  │  │
 └──────────────┘                      │  │                        │  │
                                       │  │  temp_buff (partial    │  │
                                       │  │   write accumulator)   │  │
                                       │  │                        │  │
                                       │  │  mutex_lock            │  │
                                       │  │  cdev                  │  │
                                       │  └────────────────────────┘  │
                                       └──────────────────────────────┘
```

### 2.2 Data Structures

| Structure | Purpose |
|---|---|
| `aesd_dev` | Per-device state: cdev, mutex, circular buffer, temp write buffer |
| `aesd_circular_buffer` | Fixed-size ring of 10 `aesd_buffer_entry` slots |
| `aesd_buffer_entry` | Holds `buffptr` (kmalloc'd string) and `size` |

### 2.3 Key Invariants

- Each **write command** is a newline-terminated string (e.g. `"write1\n"`)
- Writes without `\n` are **accumulated** in `temp_buff` until `\n` arrives
- The circular buffer holds at most **10** entries; the 11th overwrites the oldest
- **Reads** return the concatenation of all entries in FIFO order
- The `f_pos` offset is a **linear byte offset** across the virtual concatenation
- All buffer access is protected by a **mutex**

---

## 3. Requirements (from Assignment PDF + Test Scripts)

### 3.1 Driver Requirements

| ID | Requirement | Source |
|---|---|---|
| **R1** | Implement `aesd_open`: use `container_of(inode->i_cdev)` to set `filp->private_data` | PDF slide 13 |
| **R2** | Implement `aesd_release`: no-op (no per-open resources) | PDF slide 13 |
| **R3** | Implement `aesd_read`: find entry via `f_pos`, `copy_to_user`, return partial entry (one entry per read call) | PDF slides 14–17 |
| **R4** | Implement `aesd_write`: accumulate in `temp_buff` until `\n`, then add to circular buffer | PDF slides 18–19 |
| **R5** | When circular buffer overflows on write, **free** the `buffptr` of the evicted entry | PDF slide 6 |
| **R6** | `aesd_init_module`: initialize mutex and circular buffer | PDF slide 11 |
| **R7** | `aesd_cleanup_module`: free all dynamically allocated `buffptr` entries | PDF slide 12 |
| **R8** | Use `mutex_lock_interruptible` / `mutex_unlock` around all buffer access | PDF slide 10 |
| **R9** | Multiple partial writes without `\n` (e.g. `echo -n "wr"`, `echo -n "ite1"`, `echo ""`→`\n`) must concatenate into a single command | drivertest.sh lines 81–95 |
| **R10** | Writing 11 commands must evict command #1, so reading returns commands 2–11 | drivertest.sh lines 58–79 |

### 3.2 Socket Modification Requirements

| ID | Requirement | Source |
|---|---|---|
| **S1** | Replace `/var/tmp/aesdsocketdata` with `/dev/aesdchar` for data I/O | PDF slide 5 |
| **S2** | Timestamps may need special handling (write to device or keep separate) | PDF context |

---

## 4. Current Code Status — TODO Inventory

### 4.1 Remaining TODOs (Driver Files)

| File | Line | TODO | Status |
|---|---|---|---|
| [main.c](file:///home/karim/Desktop/Linux_assignments/assignment-1-KarimYasser275/aesd-char-driver/main.c#L123) | 123 | `TODO: handle write` | ❌ **Not implemented** |
| [main.c](file:///home/karim/Desktop/Linux_assignments/assignment-1-KarimYasser275/aesd-char-driver/main.c#L160) | 160 | `TODO: initialize the AESD specific portion of the device` | ✅ Partially done (`mutex_init` present, but circular buffer init missing) |
| [main.c](file:///home/karim/Desktop/Linux_assignments/assignment-1-KarimYasser275/aesd-char-driver/main.c#L178) | 178 | `TODO: cleanup AESD specific portions here as necessary` | ❌ **Not implemented** |
| [aesdchar.h](file:///home/karim/Desktop/Linux_assignments/assignment-1-KarimYasser275/aesd-char-driver/aesdchar.h#L31) | 31 | `TODO: Add structure(s) and locks` | ✅ Done (mutex, circular buffer, temp_buff added) |
| [aesd-circular-buffer.c](file:///home/karim/Desktop/Linux_assignments/assignment-1-KarimYasser275/aesd-char-driver/aesd-circular-buffer.c#L98) | 98 | `TODO: implement per description` | ✅ Done |

### 4.2 What's Already Implemented

| Function | Status | Notes |
|---|---|---|
| `aesd_open` | ✅ Complete | Uses `container_of`, sets `private_data` |
| `aesd_release` | ✅ Complete | Returns 0 |
| `aesd_read` | ✅ Complete | Finds entry via fpos, copies to user, updates offset |
| `aesd_write` | ❌ **Stub only** | Returns `-ENOMEM`, needs full implementation |
| `aesd_init_module` | ⚠️ Partial | `mutex_init` done, `aesd_circular_buffer_init` **missing** |
| `aesd_cleanup_module` | ❌ **Missing** | Must free all `buffptr` allocations |
| `aesd_circular_buffer_add_entry` | ✅ Complete | But doesn't return evicted pointer (see step 1) |

---

## 5. Implementation Plan — Step by Step

---

### Step 1: Update `aesd_circular_buffer_add_entry` to Return Evicted Entry

> [!IMPORTANT]
> The PDF (slide 6) says the function should return a pointer to the buffer that was removed when the circular buffer was full. This lets the caller `kfree()` the old data.

**Current signature:**
```c
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer,
                                    const struct aesd_buffer_entry *add_entry);
```

**New signature:**
```c
const char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer,
                                           const struct aesd_buffer_entry *add_entry);
```

**Logic:**
1. If `buffer->full`, save `buffer->entry[buffer->in_offs].buffptr` as `evicted_ptr`
2. Advance `out_offs` (as currently done)
3. Store the new entry, advance `in_offs`, update `full`
4. Return `evicted_ptr` (or `NULL` if not full)

**Files to modify:**
- `aesd-circular-buffer.h` — change declaration
- `aesd-circular-buffer.c` — change implementation

> [!TIP]
> **Study note — Why return the evicted pointer?**
> In kernel space, we use `kmalloc`/`kfree`. When the circular buffer overwrites an old entry, the memory for that entry's string was dynamically allocated. If we don't free it, we get a kernel memory leak. By returning the old pointer, the caller (the write function) can `kfree()` it.

---

### Step 2: Implement `aesd_write`

**File:** [main.c:118-126](file:///home/karim/Desktop/Linux_assignments/assignment-1-KarimYasser275/aesd-char-driver/main.c#L118-L126)

**Algorithm:**

```
aesd_write(filp, buf, count, f_pos):
    1. Get aesd_dev from filp->private_data
    2. Lock mutex (interruptible) → return -ERESTARTSYS on signal
    3. Reallocate temp_buff.buffptr to (temp_buff.size + count)
       - Use krealloc()
       - On failure → unlock, return -ENOMEM
    4. copy_from_user(temp_buff.buffptr + temp_buff.size, buf, count)
       - On failure → unlock, return -EFAULT
    5. temp_buff.size += count
    6. Check if '\n' exists in the received data:
       if (memchr(temp_buff.buffptr, '\n', temp_buff.size)):
           a. evicted = aesd_circular_buffer_add_entry(&cir_buff, &temp_buff)
           b. if (evicted) kfree(evicted)
           c. temp_buff.buffptr = NULL
           d. temp_buff.size = 0
    7. Unlock mutex
    8. return count  (all bytes accepted)
```

> [!TIP]
> **Study note — `copy_from_user` vs direct access:**
> User-space pointers (`buf`) cannot be dereferenced directly in kernel space. The kernel uses separate page tables. `copy_from_user()` safely copies data while checking that the user pointer is valid. It returns the number of bytes that **could not** be copied (0 = success).

> [!TIP]
> **Study note — `krealloc`:**
> Works like userspace `realloc()`. `krealloc(ptr, new_size, GFP_KERNEL)` — `GFP_KERNEL` means the allocation can sleep (which is fine since we're not in interrupt context). If `ptr` is NULL, it behaves like `kmalloc`.

> [!TIP]
> **Study note — Why ignore `f_pos` for writes?**
> The PDF (slide 18) explicitly says to ignore `f_pos` on write. Writes always either append to the current partial command or commit to the circular buffer. There's no concept of seeking to a position for writes.

---

### Step 3: Complete `aesd_init_module`

**File:** [main.c:157-162](file:///home/karim/Desktop/Linux_assignments/assignment-1-KarimYasser275/aesd-char-driver/main.c#L157-L162)

**What to add after `mutex_init`:**
```c
aesd_circular_buffer_init(&aesd_device.cir_buff);
```

The `temp_buff` is already zeroed by the `memset(&aesd_device, 0, ...)` on line 157.

> [!TIP]
> **Study note — Module init flow:**
> `aesd_init_module` is called when the module is loaded (`insmod`). It allocates a device number (`alloc_chrdev_region`), initializes our device-specific data, and registers the cdev so the kernel knows how to route file operations to our driver.

---

### Step 4: Implement `aesd_cleanup_module`

**File:** [main.c:172-181](file:///home/karim/Desktop/Linux_assignments/assignment-1-KarimYasser275/aesd-char-driver/main.c#L172-L181)

**What to add:**
```c
// Free any partial write buffer
if (aesd_device.temp_buff.buffptr) {
    kfree(aesd_device.temp_buff.buffptr);
}

// Free all circular buffer entry allocations
uint8_t index;
struct aesd_buffer_entry *entry;
AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.cir_buff, index) {
    if (entry->buffptr) {
        kfree(entry->buffptr);
    }
}
```

> [!TIP]
> **Study note — `AESD_CIRCULAR_BUFFER_FOREACH` macro:**
> Defined in `aesd-circular-buffer.h` (line 75). It iterates all 10 slots in the buffer. This is useful during cleanup because we need to free every allocated `buffptr` regardless of whether the slot is logically "active" — during module unload, we must prevent memory leaks.

> [!WARNING]
> **You must also free `temp_buff.buffptr`!** If the module is unloaded while a partial write is in progress (no `\n` received yet), there will be dangling memory.

---

### Step 5: Modify `aesdsocket` to Use `/dev/aesdchar`

**File:** [aesdsocket.c](file:///home/karim/Desktop/Linux_assignments/assignment-1-KarimYasser275/server/aesdsocket.c)

**Changes needed:**
1. Replace all `open("/var/tmp/aesdsocketdata", ...)` with `open("/dev/aesdchar", ...)`
2. The timestamp thread may need to be disabled or redirected (the char driver doesn't support arbitrary appends the same way a file does)
3. The `unlink("/var/tmp/aesdsocketdata")` in `gracefull_exit` should be removed (you don't unlink a device node)

> [!IMPORTANT]
> **Key difference from file-based approach:** `/dev/aesdchar` is a virtual device — reads return the circular buffer contents, writes add commands. There is no persistent file on disk. The socket test script (`sockettest.sh`) expects the same echo-back behavior.

---

### Step 6: Build and Test — Driver Standalone

**Build the kernel module:**
```bash
cd aesd-char-driver
make
```

**Load and test:**
```bash
sudo insmod aesd-char-driver.ko
sudo chmod 666 /dev/aesdchar       # or use aesdchar_load script
./drivertest.sh
```

**What `drivertest.sh` validates:**

| Test Case | What It Checks |
|---|---|
| Write commands 1–10, then `cat` | All 10 entries returned in FIFO order |
| Write command 11, then `cat` | Circular buffer evicts entry 1; entries 2–11 returned |
| Partial writes (`echo -n`), then `cat` | Multiple partial writes assemble into one command |

---

### Step 7: Build and Test — Socket Integration

```bash
# Start the modified aesdsocket
./aesdsocket -d

# Run the socket test
./sockettest.sh
```

**What `sockettest.sh` validates:**
- Sends strings via `nc` (netcat) and checks the echoed response
- Each response should contain all previously sent strings plus the new one

---

## 6. File Map — What To Change

| File | Action | Description |
|---|---|---|
| `aesd-char-driver/aesd-circular-buffer.h` | **MODIFY** | Change `add_entry` return type to `const char *` |
| `aesd-char-driver/aesd-circular-buffer.c` | **MODIFY** | Return evicted `buffptr` on overflow |
| `aesd-char-driver/main.c` | **MODIFY** | Implement `aesd_write`, complete `init`, implement `cleanup` |
| `server/aesdsocket.c` | **MODIFY** | Replace file path with `/dev/aesdchar`, adjust timestamp logic |

---

## 7. Key Kernel APIs Reference

| API | Header | Purpose |
|---|---|---|
| `kmalloc(size, GFP_KERNEL)` | `<linux/slab.h>` | Allocate kernel memory |
| `kfree(ptr)` | `<linux/slab.h>` | Free kernel memory |
| `krealloc(ptr, size, GFP_KERNEL)` | `<linux/slab.h>` | Resize kernel allocation |
| `copy_from_user(to, from, n)` | `<linux/uaccess.h>` | Copy from user-space buffer to kernel |
| `copy_to_user(to, from, n)` | `<linux/uaccess.h>` | Copy from kernel to user-space buffer |
| `mutex_lock_interruptible(m)` | `<linux/mutex.h>` | Lock mutex (can be interrupted by signal) |
| `mutex_unlock(m)` | `<linux/mutex.h>` | Unlock mutex |
| `container_of(ptr, type, member)` | `<linux/kernel.h>` | Get parent struct from embedded member pointer |
| `memchr(s, c, n)` | `<linux/string.h>` | Search for byte in memory region |

---

## 8. Common Pitfalls

> [!CAUTION]
> **Returning 0 from write = application hangs.** The VFS retries on 0. Always return `count` or a negative error code.

> [!WARNING]
> **Memory leak on circular buffer overflow.** When `add_entry` evicts an old entry, you must `kfree` its `buffptr`. The evicted pointer is lost forever if you don't capture the return value.

> [!WARNING]
> **Forgetting to free `temp_buff` on cleanup.** If a partial write was in progress when the module unloads, `temp_buff.buffptr` holds allocated memory.

> [!NOTE]
> **`krealloc(NULL, size, flags)` is equivalent to `kmalloc(size, flags)`.** This means you don't need to special-case the first allocation of `temp_buff.buffptr`.

> [!NOTE]
> **Read returns one entry at a time (partial read rule).** The VFS read loop (`cat`, `fread`) will call your read repeatedly until you return 0. Each call should return data from one circular buffer entry, advancing `f_pos` accordingly.
