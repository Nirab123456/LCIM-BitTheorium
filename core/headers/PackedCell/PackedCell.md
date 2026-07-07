
left-to-right bit diagram:

`32BitFamily`

bit 63                                                        bit 0
  ↓                                                            ↓
+----------------+----------------+--------------------------------+
|    meta16      |    clock16     |            value32             |
| bits 63..48    | bits 47..32    |          bits 31..0            |
+----------------+----------------+--------------------------------+

HIGH bits                                                   LOW bits

`48BitFamily`:

bit 63                                                        bit 0
  ↓                                                            ↓
+----------------+------------------------------------------------+
|    meta16      |                  raw48                         |
| bits 63..48    |                bits 47..0                      |
+----------------+------------------------------------------------+

HIGH bits                                              LOW bits

---

`GenericAccessContract`:

| Value | Name                      | CAS?                                     | Meaning                                                                                                   |
| ----: | ------------------------- | ---------------------------------------- | --------------------------------------------------------------------------------------------------------- |
|   `0` | `RAW_PRIVATE`             | No                                       | Caller owns range/cell; init/shutdown/private APC segment.                                                |
|   `1` | `ATOMIC_SNAPSHOT`         | No CAS                                   | Atomic load/store whole 64-bit cell. Multiple writers are allowed only if last-writer-wins is acceptable. |
|   `2` | `CLAIMED_GUARDED`         | Yes, but only `IDLE/PUBLISHED → CLAIMED` | Exclusive mutation. After claim, writer may raw-store companion cells, then publish with release store.   |
|   `3` | `LAST_WRITIER_WIN_CAS_RMW`                 | Yes, CAS loop or fetch-op                | For counters, cursors, epochs, clocks, version increments, occupancy deltas. No `CLAIMED` state needed.   |


---

## The final rule

```text
PackedMode = physical shape.
GenericAccessContract = access policy for VALUE32/VALUE48.
Model32Subclass/Model48Subclass = structural interpretation for MODEL32/MODEL48.
LocalityPolicy = runtime state.
OwnershipPolicy = mutation authority.
```


## `RAW_PRIVATE`

No CAS. No atomic load/store needed.

Use when:

```text
fabric initialization
fabric shutdown
slot already CLAIMED by this thread
APC owns this private region
cold immutable table data
single-thread test path
```

Never use if another thread or device can concurrently read/write the same cell.

## `ATOMIC_SNAPSHOT`

No CAS.

Use when you only need coherent 64-bit visibility:

```text
read table state
read published handle
read clock snapshot
read immutable directory after initialization
publish simple value with release store
observe value with acquire load
```

This is cheaper than CAS. Use this for most read-mostly cells.

## `CLAIMED_GUARDED`

CAS required, but only for the claim transition.

Use for exclusive mutation:

```text
payload publish
payload consume
slot allocation
hash bucket insertion
relation record allocation
work queue cell claim
ready queue cell claim
```

The pattern is:

```text
atomic load
CAS IDLE/PUBLISHED → CLAIMED
raw write companion fields
release store final PUBLISHED or IDLE
```

This is main APC/NSTFC mutation protocol.

## `LAST_WRITIER_WIN_CAS_RMW`

CAS loop or atomic fetch operation.

Use for:

```text
global epoch
fabric clock
CAS failure count
live slot count
ready queue cursor
work queue cursor
producer cursor
consumer cursor
occupancy delta
version increment
```

This is where CAS loops are justified. Do **not** use `CLAIMED_GUARDED` for every counter; it adds an unnecessary state transition.

---

# 2. Which generic VALUE cells use CAS?

For `VALUE32` / `VALUE48`:

| Cell kind                                | Recommended contract                                                   | CAS?                  |
| ---------------------------------------- | ---------------------------------------------------------------------- | --------------------- |
| immutable directory begin/end            | `RAW_PRIVATE` during init, then `ATOMIC_SNAPSHOT` or raw after freeze  | No                    |
| APC normal float32 payload               | `CLAIMED_GUARDED` if multi-producer/multi-consumer                     | Yes, claim only       |
| APC private float32 payload              | `RAW_PRIVATE`                                                          | No                    |
| branch id / logical id after publication | `ATOMIC_SNAPSHOT`                                                      | No                    |
| slot state                               | `CLAIMED_GUARDED`                                                      | Yes                   |
| hash key                                 | `CLAIMED_GUARDED`                                                      | Yes                   |
| hash value handle                        | `RAW_PRIVATE` while key claimed; `ATOMIC_SNAPSHOT` after key published | Usually no            |
| cursor                                   | `LAST_WRITIER_WIN_CAS_RMW`                                                              | Yes loop or fetch-add |
| counter                                  | `LAST_WRITIER_WIN_CAS_RMW`                                                              | Yes loop or fetch-add |
| epoch                                    | `LAST_WRITIER_WIN_CAS_RMW`                                                              | Yes                   |
| simple last-writer-wins config           | `ATOMIC_SNAPSHOT`                                                      | No                    |
| tensor value compacted for device        | `RAW_PRIVATE` in device view, then scatter through fabric protocol     | Usually no direct CAS |

This is how to minimize CAS without losing safety.

---

# 3. Model subclasses: safest rule

For `MODEL32` / `MODEL48`, the subclass defines structure, not automatically CAS.

| Mode/subclass                                           | Meaning                      | Read policy                                                        | Write policy                                                    |
| ------------------------------------------------------- | ---------------------------- | ------------------------------------------------------------------ | --------------------------------------------------------------- |
| `MODEL32::SELF_CLASS`                                   | structured 32-bit model cell | depends on owner/locality                                          | depends on contract embedded by class usage                     |
| `MODEL32::LOW_OF_PAIRED_VERSIONED_CELL`                 | low half of pair             | paired atomic snapshot/version check                               | staged update, usually claim parent/control                     |
| `MODEL32::HIGH_OF_PAIRED_VERSIONED_CELL`                | high half of pair            | paired atomic snapshot/version check                               | staged update, usually claim parent/control                     |
| `MODEL32::SUBDIVISION_NO_CLOCK16_32BIT_META_1x8PLUS2x4` | handle/meta subdivision      | atomic snapshot for handle, claimed guarded if hash key/slot state | depends on use                                                  |
| `MODEL48::PURE_TIMER_48`                                | 48-bit clock/timer           | atomic snapshot                                                    | atomic store or RMW if incrementing                             |
| `MODEL48::SUBDIVISION16x3_INTERNAL_CELL_MODEL`          | packed occupancy/counters    | atomic snapshot for read                                           | `LAST_WRITIER_WIN_CAS_RMW` CAS loop                                              |
| `MODEL48::FOUR_SUBDIVISION_2x16_AND_2x8`                | packed multi-field model     | atomic snapshot/versioned validation                               | usually `CLAIMED_GUARDED` or `LAST_WRITIER_WIN_CAS_RMW` depending on mutation    |

---

# 4. Weak CAS or strong CAS?

## Use `compare_exchange_weak` inside retry loops

Use weak CAS when you are already looping:

```text
claim payload slot
claim hash bucket
increment counter by CAS loop
update cursor by CAS loop
Robin Hood insertion
occupancy update
producer/consumer cursor advance
```

## Use `compare_exchange_strong` for one-shot decisions

Use strong CAS for **one attempt** and failure means “somebody else got it”:

```text
try mark split-in-flight once
try mark initialization-in-progress
try transition fabric initialized state
try claim singleton global compaction flag
debug/assertion-sensitive one-shot claim
```
Strong CAS avoids spurious failure, so it is better for “attempt once” logic. 
Cppreference states weak CAS may fail spuriously; strong does not have that permission in the same way. ([en.cppreference.com][1])

---

# 5. Memory order policy

| Operation                                    | Memory order                                 |
| -------------------------------------------- | -------------------------------------------- |
| observe published cell                       | `acquire`                                    |
| publish final cell                           | `release`                                    |
| claim cell by CAS                            | success `acq_rel`, failure `acquire`         |
| pure counter increment not publishing data   | `relaxed` or `acq_rel` if tied to visibility |
| release ownership / publish companion fields | `release`                                    |
| read after seeing published state            | `acquire`                                    |
| debug counters only                          | `relaxed`                                    |

Do **not** use release everywhere. Use release only when you are publishing previous writes. 
`memory_order_relaxed` is appropriate for counters that only need atomicity and no synchronization; cppreference gives counters such as reference counters as typical relaxed-use examples. ([en.cppreference.com][2])

For This Project:

```text
CAS IDLE → CLAIMED:
    success = acq_rel
    failure = acquire

raw write companion fields:
    only while owner holds CLAIMED

store final PUBLISHED:
    release

reader load PUBLISHED:
    acquire
```
---

# 6. The policy for your four `PackedMode`

| PackedMode | Subclass field means    | CAS policy                                            |
| ---------- | ----------------------- | ----------------------------------------------------- |
| `VALUE32`  | `GenericAccessContract` | only if subclass is `CLAIMED_GUARDED` or `LAST_WRITIER_WIN_CAS_RMW`    |
| `VALUE48`  | `GenericAccessContract` | only if subclass is `CLAIMED_GUARDED` or `LAST_WRITIER_WIN_CAS_RMW`    |
| `MODEL32`  | `Model32Subclass`       | determined by model + cell role                       |
| `MODEL48`  | `Model48Subclass`       | determined by model + cell role                       |

So for generic cells:

```text
VALUE32 + RAW_PRIVATE      = no atomic, no CAS
VALUE32 + ATOMIC_SNAPSHOT  = atomic load/store, no CAS
VALUE32 + CLAIMED_GUARDED  = CAS only claim/release protocol
VALUE32 + LAST_WRITIER_WIN_CAS_RMW       = CAS loop/fetch operation

VALUE48 + RAW_PRIVATE      = no atomic, no CAS
VALUE48 + ATOMIC_SNAPSHOT  = atomic load/store, no CAS
VALUE48 + CLAIMED_GUARDED  = CAS only claim/release protocol
VALUE48 + LAST_WRITIER_WIN_CAS_RMW       = CAS loop/fetch operation
```

---

# 7. What this means for NSTFC

For NSTFC :

| NSTFC structure                | Contract                                                          |
| ------------------------------ | ----------------------------------------------------------------- |
| table directory cells          | `VALUE48/VALUE32 + RAW_PRIVATE` during init, immutable after init |
| slot `STATE`                   | `MODEL32 handle + CLAIMED_GUARDED`                                |
| slot `BEGIN/END`               | `VALUE48 + RAW_PRIVATE` while slot claimed, then immutable        |
| branch/logical/shared hash key | `MODEL32 handle/meta + CLAIMED_GUARDED`                           |
| hash value handle              | `MODEL32 handle + ATOMIC_SNAPSHOT` after key published            |
| ready/work queue record        | `VALUE32/VALUE48 + CLAIMED_GUARDED`                               |
| queue cursors                  | `VALUE48 + LAST_WRITIER_WIN_CAS_RMW`                                            |
| fabric counters                | `VALUE48 + LAST_WRITIER_WIN_CAS_RMW`                                            |
| relation record state          | `MODEL32 + CLAIMED_GUARDED`                                       |
| relation fields                | raw while record claimed, snapshot after publication              |
| device view descriptor         | claimed guarded while building, snapshot after publication        |
| occupancy approximation        | `MODEL32` pair or `MODEL48` packed model, `LAST_WRITIER_WIN_CAS_RMW` on update  |

This matches the earlier NSTFC fix-guide direction: raw slab ownership, `atomic_ref` only for hot state transitions, slot directory authority, hash table cells, and no external `TableCache_`. 

---

[1]: https://en.cppreference.com/cpp/atomic/atomic/compare_exchange "compare_exchange_weak, std::atomic<T>:: ..."
[2]: https://en.cppreference.com/cpp/atomic/memory_order "std::memory_order"
[3]: https://en.cppreference.com/cpp/atomic/atomic_ref "std::atomic_ref"
