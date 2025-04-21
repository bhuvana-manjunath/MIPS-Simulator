# MIPS Architecture Simulation Suite

Developed as part of the ECE-GY 6913 Computer Architecture course at New York University.

This repository contains a collection of five simulators that model key components of a modern MIPS-based processor architecture. Each simulator corresponds to a core architectural concept, including pipelining, caching, virtual memory, branch prediction, and dynamic instruction scheduling. Together, they provide a comprehensive environment for studying and analyzing the behavior of processor subsystems under realistic workloads and timing constraints.

The simulators are written in C++ with compatibility for `g++ 11.3.0` and `GNU Make`and follow a cycle-accurate execution model. All inputs and outputs are file-based. Each project is self-contained, but collectively they form a unified toolchain for architectural experimentation.

## 1. MIPS 5-Stage Pipeline Simulator

This simulator models a cycle-accurate 5-stage pipelined MIPS processor. It supports a reduced instruction set and captures the architectural and pipeline state at each cycle of execution. The simulator provides insights into instruction-level parallelism, data hazards, and control flow behavior within a pipelined architecture.

### Pipeline Overview

The processor consists of the following five classic MIPS pipeline stages:

IF → ID → EX → MEM → WB

Each stage is connected by pipeline registers (flip-flops), and instruction execution is modeled cycle-by-cycle, with hazard detection and resolution mechanisms implemented to preserve correctness.

### Supported Instruction Set

The simulator implements a subset of the MIPS ISA, including both R-type and I-type instructions, as well as a custom halt instruction. The supported instructions and their encodings are listed below:

| Instruction | Format  | Opcode (Hex) | Func (Hex) |
|-------------|---------|--------------|------------|
| `addu`      | R-Type  | 00           | 21         |
| `subu`      | R-Type  | 00           | 23         |
| `lw`        | I-Type  | 23           | –          |
| `sw`        | I-Type  | 2B           | –          |
| `bne`       | I-Type  | 05           | –          |
| `halt`      | J-Type  | 3F           | –          |

All instructions, with the exception of `halt`, are standard MIPS operations. The `bne` instruction is used in place of `beq` to simplify loop implementation. A `halt` instruction is used to terminate simulation and flush the pipeline.

### Pipeline Stage Descriptions

- **IF (Instruction Fetch)**: Fetches instruction from instruction memory and updates the program counter (PC).
- **ID (Instruction Decode / Register Fetch)**: Decodes the instruction, generates control signals, reads from the register file, resolves branches, and computes effective addresses.
- **EX (Execute)**: Performs arithmetic and logical operations via a simple ALU.
- **MEM (Memory Access)**: Performs memory read/write operations for `lw` and `sw`.
- **WB (Write Back)**: Writes results to the register file if applicable.

### Hazard Management

- **Data Hazards (RAW)**: Resolved through data forwarding. When forwarding alone is insufficient, stalling is applied in conjunction with forwarding.
- **Control Hazards (Branch)**: The simulator assumes a static branch prediction strategy (always not taken). Branch resolution occurs in the ID stage. If a branch is taken, the instruction fetched speculatively in the IF stage is squashed using the `nop` signal.

### Input Format

- `imem.txt`: Initializes instruction memory. Each line represents a byte in binary, with instructions stored in big-endian format (most significant byte first).
- `dmem.txt`: Initializes data memory in the same format as `imem.txt`.

### Output Files

- `results/RFresult.txt`: Contains the contents of the register file after every cycle.
- `results/dmemresult.txt`: Captures the final state of the data memory after program termination.
- `results/stateresult.txt`: Records the pipeline register (flip-flop) state at every cycle.

### Running the Simulator

To compile and run the simulator:

```bash
make
./MIPS_pipeline
```

Ensure all required files (`imem.txt`, `dmem.txt`) are present in the same directory as the executable. Results are stored in the `results/` directory.

## 2. Two-Level Cache Simulator

This simulator models a two-level (L1 and L2) exclusive cache hierarchy with configurable parameters, including block size, associativity, and cache size. It supports read and write memory accesses and implements realistic cache behavior including hits, misses, evictions, and memory writes. All operations follow a cycle-accurate simulation of cache access and state transition logic.

### Cache Architecture

The cache hierarchy consists of:

- **L1 Cache**: Typically smaller and faster, configured independently.
- **L2 Cache**: Larger, slower cache that exclusively holds blocks not in L1.
  
Both caches are block-based and set-associative. The hierarchy is **exclusive**, meaning a block exists in either L1 or L2, but never in both simultaneously.

Each cache is implemented as an array of sets, where each set contains multiple **ways**. Blocks are indexed by set and tagged for identification. Eviction is handled per-set using a round-robin policy.

### Cache Organization

- **Block Size**: Must be a non-negative power of 2 (e.g., 4, 8).
- **Associativity**: Direct-mapped if 1-way, fully associative if 0-way, and set-associative otherwise.
- **Replacement Policy**: Each set uses a **round-robin** counter to determine the eviction candidate when full.

### Memory Access Semantics

- **Read Operations**:
  - **L1 Hit**: Immediate access, no further action.
  - **L1 Miss / L2 Hit**: Block is moved from L2 to L1. If L1 is full, a block is evicted to L2. If L2 is also full, an eviction may trigger a write to main memory (if dirty).
  - **L1 Miss / L2 Miss**: Block is fetched from main memory to L1. May trigger evictions at both levels.
  
- **Write Operations**:
  - **Write-Back, Write-No-Allocate**:
    - **L1 Hit**: Set dirty bit in L1.
    - **L1 Miss / L2 Hit**: Set dirty bit in L2.
    - **L1 Miss / L2 Miss**: Write occurs directly to main memory (no cache update).

### Input Files

- `cacheconfig.txt`: Defines cache structure for both L1 and L2. Format:
  ```
  L1:
  <block_size>
  <associativity>
  <size_in_KiB>
  L2:
  <block_size>
  <associativity>
  <size_in_KiB>
  ```

  Example:
  ```
  L1:
  8
  1
  16
  L2:
  8
  4
  32
  ```

- `trace.txt`: Sequence of memory accesses. Each line contains:
  ```
  <AccessType> <MemoryAddress>
  ```
  where:
  - `<AccessType>` is `R` (read) or `W` (write)
  - `<MemoryAddress>` is a 32-bit unsigned hex value (e.g., `0xABCDEF12`)

### Output File

- `trace.txt.out`: Line-by-line record of each memory access, describing the events that occurred in the caches and main memory. Each line contains three event codes:
  ```
  <L1_event_code> <L2_event_code> <Main_memory_event_code>
  ```

#### Event Codes

| Code | Description                  |
|------|------------------------------|
| 0    | No Access                    |
| 1    | Read Hit                     |
| 2    | Read Miss                    |
| 3    | Write Hit                    |
| 4    | Write Miss                   |
| 5    | No Write to Main Memory      |
| 6    | Write to Main Memory         |

**Example**:  
A write miss in both L1 and L2, causing a direct write to main memory, results in the output line:  
```
4 4 6
```

### Running the Simulator

To compile and run the simulator:

```bash
make
./cachesimulator cacheconfig.txt trace.txt
```

Ensure all required files (`cacheconfig.txt`, `trace.txt`) are present in the same directory as the executable. The simulator will produce `trace.txt.out` as output.

### Implementation Notes

- Block movement and eviction follow exclusive policy: a block evicted from L1 is placed into L2, and vice versa.
- Round-robin eviction uses a per-set counter to determine the way to evict.
- Dirty bits are respected during evictions from L2: if dirty, data is written to main memory; otherwise, it is discarded.
- All address parsing, set indexing, and tag matching are done using bitwise operations based on the configured block size and associativity.

## 3. Two-Level Page Table Simulator

This simulator implements a two-level page table mechanism for virtual-to-physical address translation. It models a 14-bit virtual address space and a 12-bit physical address space using a hierarchical paging scheme with byte-addressable memory. The design accurately simulates page table traversal, validity checking, and memory lookup for a virtual memory subsystem.

### Paging Scheme Overview

- **Virtual Address Size**: 14 bits  
- **Physical Address Size**: 12 bits  
- **Page Size**: 64 bytes  
- **Page Table Entry (PTE) Size**: 4 bytes  
- **Memory Model**: Byte-addressable  
- **Translation Granularity**: 2-level page table

### Virtual Address Format

Each 14-bit virtual address is divided as follows:

```
[13–10]  [9–6]   [5–0]
  OPT     IPT    Offset
```

- **OPT** (Outer Page Table Index): 4 bits  
- **IPT** (Inner Page Table Index): 4 bits  
- **Offset** (Page Offset): 6 bits  

The **Page Table Base Register (PTBR)** holds the physical base address of the outer page table.

### Page Table Entry (PTE) Formats

- **Outer PTE** (4 bytes):
  - **Bit 0**: Valid bit  
  - **Bits 1–19**: Unused (set to 0)  
  - **Bits 20–31**: Physical address of the inner page table base

- **Inner PTE** (4 bytes):
  - **Bit 0**: Valid bit  
  - **Bits 1–25**: Unused (set to 0)  
  - **Bits 26–31**: Frame number (top 6 bits of physical address)

Physical memory is initialized with predefined values. Each page table fits into a 64-byte page.

### Translation Process

1. Read the **PTBR** (Page Table Base Register) to locate the base of the outer page table.
2. Use the **OPT** field to index into the outer table (OPT × 4).
3. Check the outer PTE’s **valid bit**:
   - If 0: output `outer_valid = 0`, skip translation.
4. Extract the inner table base address from the outer PTE.
5. Use the **IPT** field to index into the inner table (IPT × 4).
6. Check the inner PTE’s **valid bit**:
   - If 0: output `inner_valid = 0`, skip translation.
7. If both levels are valid:
   - Combine the 6-bit frame number with the 6-bit offset to compute the final 12-bit physical address.
   - Read 4 consecutive bytes from memory at this physical address to retrieve a 32-bit memory value.

### Input Files

- `pt_initialize.txt`: Initializes physical memory.
- `PTBR.txt`: Contains the value of the Page Table Base Register.
- `pt_requests.txt`: List of virtual addresses to be translated (one per line).

### Output File

- `pt_results.txt`: Contains the result of each address translation in the format:
  ```
  <outer_valid>, <inner_valid>, 0x<PhysicalAddr>, 0x<MemoryValue>
  ```

If a page table entry is invalid, the output for address and value is:
```
0x000, 0x00000000
```

### Running the Simulator

To compile and run the simulator:

```bash
make
./PageTable pt_requests.txt PTBR.txt
```

Ensure the following files are present in the same directory:
- `pt_initialize.txt`
- `PTBR.txt`
- `pt_requests.txt`

The simulator will produce `pt_results.txt` as output.

### Example Output

```
1, 1, 0x3F2, 0xADDE1234
0, 0, 0x000, 0x00000000
1, 0, 0x000, 0x00000000
```

Each line corresponds to a single translation request, reflecting the validity of outer and inner page table entries and the outcome of the translation.

### Implementation Notes
- Both levels of page tables are indexed using 4-bit values, with each entry being 4 bytes.
- Indexing is done by multiplying the index by 4 (due to byte addressability).
- All unused PTE bits must be set to 0, as required by the lab specification.
- The memory lookup is performed on 32-bit aligned addresses derived from valid physical addresses.

## 4. Two-Level Branch Predictor Simulator

This simulator implements a **two-level global branch predictor** using a combination of a global Branch History Table (BHT) and a Pattern History Table (PHT) with 2-bit saturating counters. It predicts the outcome of conditional branch instructions and updates its internal state based on correctness of predictions. The design closely follows real-world dynamic prediction schemes and enables exploration of predictor configuration impact.

### Predictor Architecture

The branch predictor uses the following components:

- **Branch History Table (BHT)**:  
  A global table with \(2^h\) entries, each entry storing a `w`-bit branch history register (BHR). All entries are initialized to `0`.

- **Pattern History Table (PHT)**:  
  A table with \(2^m\) entries. Each entry stores a 2-bit **saturating counter** initialized to `10` (Weak Taken). The 2-bit states evolve based on the finite state machine shown below:

  | Binary | State             | Prediction   |
  |--------|-------------------|--------------|
  | 00     | Strong Not Taken  | Not Taken (0)|
  | 01     | Weak Not Taken    | Not Taken (0)|
  | 10     | Weak Taken        | Taken (1)    |
  | 11     | Strong Taken      | Taken (1)    |

### Prediction Process

For each branch in the trace:

1. **Index the BHT** using `h` bits from the PC, starting from the 3rd least significant bit (LSB). This gives a `w`-bit history value.
2. **Construct the PHT index** by:
   - Extracting the upper `(m - w)` bits from the PC (also starting from the 3rd LSB)
   - Concatenating those bits with the `w`-bit history value from the BHT
3. **Predict** the outcome using the corresponding 2-bit counter from the PHT:
   - If counter is `00` or `01`, predict **Not Taken (0)**
   - If counter is `10` or `11`, predict **Taken (1)**
4. **Compare** the prediction to the actual branch outcome:
   - If correct: retain or strengthen the current state
   - If incorrect: shift toward the opposite prediction side (taken ↔ not taken)
5. **Update the BHT**:
   - Left shift the current BHT entry and append the actual outcome bit (0 or 1)

### Parameters (from `config.txt`)

- `m`: Number of bits used to index into the PHT
- `h`: Number of bits extracted from PC to index the BHT
- `w`: Length of the history register stored at each BHT entry

### Input Files

- `config.txt` — Contains the three predictor parameters:
  ```
  <m>
  <h>
  <w>
  ```

- `trace.txt` — Each line represents a branch instruction:
  ```
  <PC (hex)> <BranchOutcome (0 or 1)>
  ```

### Output File

- `trace.txt.out`: One line per prediction:
  ```
  0  // Not Taken
  1  // Taken
  ```

Each line corresponds to the simulator’s prediction for the respective branch in the trace file.

### Running the Simulator

To compile and run the simulator:

```bash
make
./branchsimulator.out config.txt trace.txt
```

Ensure all required files (`config.txt`, `trace.txt`) are present in the same directory as the executable. The simulator will produce `trace.txt.out` as output.

### Example Workflow

Given:
- `m = 7`, `h = 3`, `w = 3`
- BHT contains \(2^3 = 8\) entries, each with a 3-bit branch history
- PHT contains \(2^7 = 128\) 2-bit counters

Each prediction uses `3` bits from the PC to index the BHT, retrieves a 3-bit history, and forms a 7-bit PHT index by combining `(m - w) = 4` more bits from the PC with the history.

### Implementation Notes

- All bit extractions are done starting from the 3rd LSB of the PC.
- PHT and BHT are initialized to their respective default states.
- Concatenation for PHT indexing places PC bits in the upper bits and BHT bits in the lower bits.

## 5. Tomasulo’s Algorithm Simulator

This simulator implements **Tomasulo’s algorithm**, a hardware-based dynamic scheduling technique for out-of-order (OOO) instruction execution. It models register renaming, reservation stations, and Common Data Bus (CDB) result broadcasting to handle **RAW, WAR, and WAW hazards** and enable parallelism across multiple functional units.

### Simulation Overview

Tomasulo’s algorithm enables:
- Dynamic instruction issue and execution
- Data hazard resolution without compiler intervention
- Speculative parallelism while maintaining correctness

The simulator includes reservation stations for each instruction type (ADD/SUB, MULT/DIV, LOAD, STORE), a **Register Result Status table** to track operand readiness, and a **CDB** to broadcast results to dependent instructions.

### Features

- Simulates out-of-order instruction issue, execution, and writeback  
- Handles **RAW**, **WAR**, and **WAW** hazards  
- Uses **2-cycle latency** for ADD, SUB, LOAD, STORE  
- Uses **10-cycle latency** for MULT, and **40-cycle latency** for DIV  
- Models **result forwarding via CDB**  
- Records **per-instruction timeline** and **register status** every 5 cycles  

### Components

#### 1. **Reservation Stations (RS)**
Each RS holds:
- Operation (`Op`)
- Operand values (`Vj`, `Vk`) or their sources (`Qj`, `Qk`)
- Countdown to track execution cycles
- Busy status and issue cycle

#### 2. **Common Data Bus (CDB)**
- Only one instruction may write to the CDB per cycle
- Writes to registers and updates waiting RS entries

#### 3. **Register Result Status Table**
Tracks:
- The RS producing each register’s value
- A valid bit indicating whether the data is ready

#### 4. **Functional Units**
Each RS entry is assumed to have a dedicated functional unit. Multiple instructions of the same type may execute in parallel, but only one can broadcast on the CDB per cycle.

### Execution Model

1. **Issue**  
   - Finds a free RS of the correct type  
   - Operands are read if available, or RS pointers (`Qj`, `Qk`) are set  
   - RS assigned to the destination register (if applicable)

2. **Execute**  
   - Countdown starts only when both operands are ready (`Qj` and `Qk` are empty)  
   - Execution duration depends on instruction latency

3. **Write Result**  
   - Result is written to the CDB and broadcasted to:
     - All registers that depend on the issuing RS
     - All RS entries waiting on this result

4. **Register Update**  
   - The Register Result Status is cleared if a register receives its final value

### Instruction Latencies

| Instruction | Latency (cycles) |
|-------------|------------------|
| ADD, SUB    | 2                |
| LOAD, STORE | 2                |
| MULT        | 10               |
| DIV         | 40               |


### Input Files

- `config.txt`: Configuration for simulator hardware  
  ```
  <RS_Load_Size>
  <RS_Store_Size>
  <RS_Add_Size>
  <RS_Mult_Size>
  <Num_FP_Registers>
  ```
  Example:
  ```
  3
  3
  3
  3
  12
  ```

- `trace.txt`: Instruction trace file  
  Each instruction is formatted as:  
  ```
  <OP> <Dest> <Src1> <Src2>
  ```
  Example:
  ```
  LOAD F6 34 0
  LOAD F2 45 0
  MULT F0 F2 F6
  SUB  F8 F6 F2
  DIV  F10 F0 F6
  ADD  F6 F8 F2
  ```

### Output File

- `trace.out.txt` — Contains:
  - **Register status** every 5 cycles  
  - **Instruction timeline**: issue cycle, execution complete, and write result cycle  

Example:
```
Cycle 5:
F0: Mult0, dataRdy: N,
F1: , dataRdy: N,
F2: Load1, dataRdy: Y,
...

Instruction Status:
Instr0: Issued: 1, Completed: 3, Write Result: 4,
Instr1: Issued: 2, Completed: 4, Write Result: 5,
Instr2: Issued: 3, Completed: 15, Write Result: 16,
...
```

### Running the Simulator

To compile and run the simulator:

```bash
make
./tomasulosimulator.out config.txt trace.txt
```

Ensure `trace.txt`, `config.txt`, and the executable are in the same directory. The simulator will output `trace.out.txt`.

### Implementation Notes

- **STORE** has a "write result" stage but does not update any register.
- Only **one** result may be written to the CDB per cycle. If multiple instructions complete, the instruction **issued earliest** writes first.
- Memory hazards are not modeled; LOAD and STORE always complete after 2 cycles.

All expected output files are included in the `expected_results/` directory located within each respective simulator folder.
