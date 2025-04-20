/*
--------------------------------------------------------
               MIPS 5-Stage Pipeline Simulator
--------------------------------------------------------

This program simulates a simplified MIPS processor using the classic
5-stage pipeline architecture:

    IF  - Instruction Fetch  
    ID  - Instruction Decode / Register Fetch  
    EX  - Execute 
    MEM - Memory Access  
    WB  - Write Back
  
The simulator models execution cycle by cycle and supports a reduced
MIPS instruction set: addu, subu, lw, sw, bne, and halt.

It implements forwarding and stalling to handle RAW hazards, and uses
a static "not taken" branch prediction strategy to resolve control hazards.
Branches are resolved in the ID stage, and incorrectly fetched instructions
are squashed using the nop bit.

Instruction memory (imem.txt) and data memory (dmem.txt) are initialized
in big-endian format, and results are written to RFresult.txt and 
dmemresult.txt. The simulator halts cleanly upon encountering the custom
halt instruction, ensuring all architectural state changes are complete.

Useful for understanding pipelined processor behavior, hazard resolution,
and instruction-level parallelism in CPU design.
*/

#include <iostream>
#include <string>
#include <vector>
#include <bitset>
#include <fstream>
using namespace std;

// Simulated memory size in words.
// In a real MIPS system, memory is 2^32 bytes.
// Here, we limit it to 1000 entries for simplicity.
#define MemSize 1000


// Takes an N-bit immediate value (e.g., 16-bit) and sign-extends it to 32 bits.
template<size_t N>
bitset<32> sign_extend(bitset<N> imm)
{
    return imm.to_ulong() | 
           (bitset<32>(bitset<16>(-imm[imm.size() - 1]).to_ulong() << 16)).to_ulong();
}

/*
 * Pipeline Register Structures
 * ----------------------------------
 * Represent the state and control signals passed between pipeline stages.
 * The `nop` flag indicates whether the stage is currently executing a valid instruction.
 */

// IF (Instruction Fetch) Stage
struct IFStruct {
    bitset<32> PC = bitset<32>(0); // Current Program Counter
    bool nop;                      // True if this stage is a bubble
};

// ID (Instruction Decode / Register Fetch) Stage
struct IDStruct {
    bitset<32> Instr = bitset<32>(0); // Raw instruction fetched from memory
    bool nop;
};

// EX (Execute) Stage
struct EXStruct {
    bitset<32> Read_data1 = bitset<32>(0); // Register operand 1
    bitset<32> Read_data2 = bitset<32>(0); // Register operand 2
    bitset<16> Imm = bitset<16>(0);        // 16-bit immediate value
    bitset<5> Rs = bitset<5>(0);           // Source register 1
    bitset<5> Rt = bitset<5>(0);           // Source register 2
    bitset<5> Wrt_reg_addr = bitset<5>(0); // Destination register
    bool is_I_type = 0;                    // True for I-type instructions
    bool rd_mem = 0;                       // Read from memory
    bool wrt_mem = 0;                      // Write to memory
    bool alu_op = 0;                       // ALU op: 1 = add/lw/sw, 0 = sub
    bool wrt_enable = 0;                   // Enable register write-back
    bool nop;
};

// MEM (Memory Access) Stage
struct MEMStruct {
    bitset<32> ALUresult = bitset<32>(0);   // ALU output (address or result)
    bitset<32> Store_data = bitset<32>(0);  // Data to write to memory
    bitset<5> Rs = bitset<5>(0);            // Source register 1 (for debugging)
    bitset<5> Rt = bitset<5>(0);            // Source register 2 (for debugging)
    bitset<5> Wrt_reg_addr = bitset<5>(0);  // Destination register
    bool rd_mem = 0;                        // Memory read flag
    bool wrt_mem = 0;                       // Memory write flag
    bool wrt_enable = 0;                    // Register write-back enable
    bool nop;
};

// WB (Write Back) Stage
struct WBStruct {
    bitset<32> Wrt_data = bitset<32>(0);    // Data to write back to register file
    bitset<5> Rs = bitset<5>(0);            // Source register 1 (for debugging)
    bitset<5> Rt = bitset<5>(0);            // Source register 2 (for debugging)
    bitset<5> Wrt_reg_addr = bitset<5>(0);  // Register to write back to
    bool wrt_enable = 0;                    // Enable register write-back
    bool nop;
};

// Overall pipeline state (all pipeline registers for a given cycle)
struct stateStruct {
    IFStruct  IF;
    IDStruct  ID;
    EXStruct  EX;
    MEMStruct MEM;
    WBStruct  WB;
};

/*
 * RF - Register File
 * ----------------------------------
 * Simulates the 32 general-purpose registers in MIPS.
 * Provides read/write access with enforced zero-register behavior.
 */

 class RF
 {
 public:
     bitset<32> Reg_data; 
 
     // Initialize 32 registers, with register $zero hardwired to 0
     RF()
     {
         Registers.resize(32);
         Registers[0] = bitset<32>(0);
     }
 
     // Read data from the register at Reg_addr
     bitset<32> readRF(bitset<5> Reg_addr)
     {
         Reg_data = Registers[Reg_addr.to_ulong()];
         return Reg_data;
     }
 
     // Write data to register unless it's $zero (which is read-only)
     void writeRF(bitset<5> Reg_addr, bitset<32> Wrt_reg_data)
     {
         if (Reg_addr.to_ulong() != 0)
         {
             Registers[Reg_addr.to_ulong()] = Wrt_reg_data;
         }
     }
 
     // Append the current state of all registers to "RFresult.txt"
     void outputRF()
     {
         ofstream rfout("results/RFresult.txt", std::ios_base::app);
         if (rfout.is_open())
         {
             rfout << "State of RF:\t" << endl;
             for (int j = 0; j < 32; j++)
             {
                 rfout << Registers[j] << endl;
             }
             rfout.close();
         }
         else 
         {
             cout << "Unable to open file" << endl;
         }
     }
 
 private:
     vector<bitset<32>> Registers; // 32 general-purpose registers
 }; 

/*
 * INSMem - Instruction Memory
 * ----------------------------------
 * Byte-addressable, read-only instruction memory.
 * Loads machine code from "imem.txt", then returns 32-bit instructions on request.
 */

 class INSMem
 {
 public:
     bitset<32> Instruction;
 
     // Load instruction memory from imem.txt
     INSMem()
     {
         IMem.resize(MemSize);
         ifstream imem("imem.txt");
         string line;
         int i = 0;
 
         if (imem.is_open())
         {
             while (getline(imem, line))
             {
                 IMem[i] = bitset<8>(line);
                 i++;
             }
             imem.close();
         }
         else
         {
             cout << "Unable to open imem.txt" << endl;
         }
     }
 
     // Return a 32-bit instruction starting at ReadAddress (Big-Endian)
     bitset<32> readInstr(bitset<32> ReadAddress)
     {
         string insmem;
 
         insmem.append(IMem[ReadAddress.to_ulong()].to_string());
         insmem.append(IMem[ReadAddress.to_ulong() + 1].to_string());
         insmem.append(IMem[ReadAddress.to_ulong() + 2].to_string());
         insmem.append(IMem[ReadAddress.to_ulong() + 3].to_string());
 
         Instruction = bitset<32>(insmem);
         return Instruction;
     }
 
 private:
     vector<bitset<8>> IMem;
 }; 
 
 /*
 * DataMem - Data Memory
 * ----------------------------------
 * Byte-addressable read/write memory.
 * Loads initial contents from "dmem.txt" and stores final state to "dmemresult.txt".
 */

class DataMem
{
public:
    bitset<32> ReadData;

    // Load data memory from dmem.txt
    DataMem()
    {
        DMem.resize(MemSize);
        ifstream dmem("dmem.txt");
        string line;
        int i = 0;

        if (dmem.is_open())
        {
            while (getline(dmem, line))
            {
                DMem[i] = bitset<8>(line);
                i++;
            }
            dmem.close();
        }
        else
        {
            cout << "Unable to open dmem.txt" << endl;
        }
    }

    // Read 4 bytes from memory starting at Address (Big-Endian)
    bitset<32> readDataMem(bitset<32> Address)
    {
        string datamem;

        datamem.append(DMem[Address.to_ulong()].to_string());
        datamem.append(DMem[Address.to_ulong() + 1].to_string());
        datamem.append(DMem[Address.to_ulong() + 2].to_string());
        datamem.append(DMem[Address.to_ulong() + 3].to_string());

        ReadData = bitset<32>(datamem);
        return ReadData;
    }

    // Write a 32-bit word to memory starting at Address (Big-Endian)
    void writeDataMem(bitset<32> Address, bitset<32> WriteData)
    {
        DMem[Address.to_ulong()]     = bitset<8>(WriteData.to_string().substr(0, 8));
        DMem[Address.to_ulong() + 1] = bitset<8>(WriteData.to_string().substr(8, 8));
        DMem[Address.to_ulong() + 2] = bitset<8>(WriteData.to_string().substr(16, 8));
        DMem[Address.to_ulong() + 3] = bitset<8>(WriteData.to_string().substr(24, 8));
    }

    // Dump memory contents to dmemresult.txt
    void outputDataMem()
    {
        ofstream dmemout("results/dmemresult.txt");

        if (dmemout.is_open())
        {
            for (int j = 0; j < MemSize; j++)
            {
                dmemout << DMem[j] << endl;
            }
            dmemout.close();
        }
        else
        {
            cout << "Unable to open dmemresult.txt" << endl;
        }
    }

private:
    vector<bitset<8>> DMem;
};

/*
 * printState
 * ----------------------------------
 * Logs the state of all five pipeline stages (IF, ID, EX, MEM, WB)
 * at the end of each simulation cycle. Writes output to 
 * "stateresult.txt" in append mode to preserve full trace.
 *
 * Each stage’s data fields and control signals are printed to 
 * help visualize how instructions move through the pipeline and 
 * how hazards, forwarding, and control flow affect execution.
 */

 void printState(stateStruct state, int cycle)
 {
     ofstream printstate("results/stateresult.txt", ios_base::app); // Append mode
 
     if (!printstate.is_open()) {
         cout << "Unable to open stateresult.txt" << endl;
         return;
     }
 
     printstate << "State after executing cycle:\t" << cycle << endl;
 
     // ---------- IF Stage ----------
     printstate << "IF.PC:\t\t\t" << state.IF.PC.to_ulong() << endl;
     printstate << "IF.nop:\t\t\t" << state.IF.nop << endl;
 
     // ---------- ID Stage ----------
     printstate << "ID.Instr:\t\t" << state.ID.Instr << endl;
     printstate << "ID.nop:\t\t\t" << state.ID.nop << endl;
 
     // ---------- EX Stage ----------
     printstate << "EX.Read_data1:\t\t"   << state.EX.Read_data1 << endl;
     printstate << "EX.Read_data2:\t\t"   << state.EX.Read_data2 << endl;
     printstate << "EX.Imm:\t\t\t"        << state.EX.Imm << endl;
     printstate << "EX.Rs:\t\t\t"         << state.EX.Rs << endl;
     printstate << "EX.Rt:\t\t\t"         << state.EX.Rt << endl;
     printstate << "EX.Wrt_reg_addr:\t"   << state.EX.Wrt_reg_addr << endl;
     printstate << "EX.is_I_type:\t\t"    << state.EX.is_I_type << endl;
     printstate << "EX.rd_mem:\t\t"       << state.EX.rd_mem << endl;
     printstate << "EX.wrt_mem:\t\t"      << state.EX.wrt_mem << endl;
     printstate << "EX.alu_op:\t\t"       << state.EX.alu_op << endl;
     printstate << "EX.wrt_enable:\t\t"   << state.EX.wrt_enable << endl;
     printstate << "EX.nop:\t\t\t"        << state.EX.nop << endl;
 
     // ---------- MEM Stage ----------
     printstate << "MEM.ALUresult:\t\t"   << state.MEM.ALUresult << endl;
     printstate << "MEM.Store_data:\t\t"  << state.MEM.Store_data << endl;
     printstate << "MEM.Rs:\t\t\t"         << state.MEM.Rs << endl;
     printstate << "MEM.Rt:\t\t\t"         << state.MEM.Rt << endl;
     printstate << "MEM.Wrt_reg_addr:\t"  << state.MEM.Wrt_reg_addr << endl;
     printstate << "MEM.rd_mem:\t\t"      << state.MEM.rd_mem << endl;
     printstate << "MEM.wrt_mem:\t\t"     << state.MEM.wrt_mem << endl;
     printstate << "MEM.wrt_enable:\t\t"  << state.MEM.wrt_enable << endl;
     printstate << "MEM.nop:\t\t\t"       << state.MEM.nop << endl;
 
     // ---------- WB Stage ----------
     printstate << "WB.Wrt_data:\t\t"     << state.WB.Wrt_data << endl;
     printstate << "WB.Rs:\t\t\t"          << state.WB.Rs << endl;
     printstate << "WB.Rt:\t\t\t"          << state.WB.Rt << endl;
     printstate << "WB.Wrt_reg_addr:\t"   << state.WB.Wrt_reg_addr << endl;
     printstate << "WB.wrt_enable:\t\t"   << state.WB.wrt_enable << endl;
     printstate << "WB.nop:\t\t\t"        << state.WB.nop << endl;
 
     printstate << endl;
     printstate.close();
 }
 
/*
--------------------------------------------------------
       MIPS Pipeline Simulator - Main Loop
--------------------------------------------------------

Simulates a 5-stage pipelined MIPS processor:
    IF → ID → EX → MEM → WB

Each cycle performs:
  - Instruction fetch, decode, execution, memory access, and write-back
  - Forwarding and stalling for RAW hazard resolution
  - Static "not taken" branch prediction and control hazard squashing
  - Memory and register state tracking and logging

Program halts cleanly upon executing the custom `halt` instruction.
*/

int main()
{
    // Initialize components
    RF myRF;                     // Register File
    INSMem myInsMem;             // Instruction Memory
    DataMem myDataMem;           // Data Memory
    stateStruct state, newState; // Pipeline registers
    int cycle = 0;               // Simulation cycle count

    // Initial nop configuration (only IF runs first)
    state.IF.nop = 0;
    state.ID.nop = state.EX.nop = state.MEM.nop = state.WB.nop = 1;
    newState = state;

    while (true)
    {
        // Propagate NOPs to next pipeline stage
        newState.ID.nop  = state.IF.nop;
        newState.EX.nop  = state.ID.nop;
        newState.MEM.nop = state.EX.nop;
        newState.WB.nop  = state.MEM.nop;

        /* --------------------- WB Stage --------------------- */
        if (!state.WB.nop && state.WB.wrt_enable)
        {
            myRF.writeRF(state.WB.Wrt_reg_addr, state.WB.Wrt_data);
        }

        /* --------------------- MEM Stage --------------------- */
        if (!state.MEM.nop)
        {
            newState.WB.Rs = state.MEM.Rs;
            newState.WB.Rt = state.MEM.Rt;
            newState.WB.Wrt_reg_addr = state.MEM.Wrt_reg_addr;
            newState.WB.wrt_enable = state.MEM.wrt_enable;
            newState.WB.Wrt_data = state.MEM.ALUresult;

            if (state.MEM.rd_mem && !state.MEM.wrt_mem)
            {
                newState.WB.Wrt_data = myDataMem.readDataMem(state.MEM.ALUresult);
            }
            else if (state.MEM.wrt_mem && !state.MEM.rd_mem)
            {
                myDataMem.writeDataMem(state.MEM.ALUresult, state.MEM.Store_data);
                newState.WB.Wrt_data = bitset<32>(0);
            }
        }

        /* --------------------- EX Stage --------------------- */
        // Handle data forwarding and stalls for RAW hazards
        if (state.EX.Rs == state.MEM.Wrt_reg_addr && state.MEM.Wrt_reg_addr != 0)
        {
            if (state.MEM.rd_mem)
            {
                // Stall due to load-use hazard
                state.EX.nop = state.ID.nop = state.IF.nop = true;
                newState.EX.nop = false;
                newState.MEM.nop = true;
                newState.MEM.Wrt_reg_addr = 0;
            }
            else
            {
                state.EX.Read_data1 = state.MEM.ALUresult;
                newState.EX.Read_data1 = state.MEM.ALUresult;
            }
        }
        else if (state.EX.Rs == state.WB.Wrt_reg_addr && state.WB.Wrt_reg_addr != 0)
        {
            state.EX.Read_data1 = state.WB.Wrt_data;
            newState.EX.Read_data1 = state.WB.Wrt_data;
        }

        if (state.EX.Rt == state.MEM.Wrt_reg_addr && state.MEM.Wrt_reg_addr != 0)
        {
            if (state.MEM.rd_mem)
            {
                // Stall due to load-use hazard
                state.EX.nop = state.ID.nop = state.IF.nop = true;
                newState.EX.nop = false;
                newState.MEM.nop = true;
                newState.MEM.Wrt_reg_addr = 0;
            }
            else
            {
                state.EX.Read_data2 = state.MEM.ALUresult;
                newState.EX.Read_data2 = state.MEM.ALUresult;
            }
        }
        else if (state.EX.Rt == state.WB.Wrt_reg_addr && state.WB.Wrt_reg_addr != 0)
        {
            state.EX.Read_data2 = state.WB.Wrt_data;
            newState.EX.Read_data2 = state.WB.Wrt_data;
        }

        // Execute operation if not a stall
        if (!state.EX.nop)
        {
            newState.MEM.Rs = state.EX.Rs;
            newState.MEM.Rt = state.EX.Rt;
            newState.MEM.Wrt_reg_addr = state.EX.Wrt_reg_addr;
            newState.MEM.rd_mem = state.EX.rd_mem;
            newState.MEM.wrt_mem = state.EX.wrt_mem;
            newState.MEM.wrt_enable = state.EX.wrt_enable;

            if (state.EX.is_I_type)
            {
                if (!state.EX.rd_mem && !state.EX.wrt_mem && !state.EX.wrt_enable)
                {
                    newState.MEM.ALUresult = bitset<32>(0);
                    newState.MEM.Store_data = bitset<32>(0);
                }
                else
                {
                    newState.MEM.ALUresult = state.EX.Read_data1.to_ulong() + sign_extend(state.EX.Imm).to_ulong();
                    newState.MEM.Store_data = state.EX.Read_data2;
                }
            }
            else
            {
                // R-type ALU operation
                newState.MEM.ALUresult = state.EX.alu_op ?
                    (state.EX.Read_data1.to_ulong() + state.EX.Read_data2.to_ulong()) :
                    (state.EX.Read_data1.to_ulong() - state.EX.Read_data2.to_ulong());
                newState.MEM.Store_data = bitset<32>(0);
            }
        }

        /* --------------------- ID Stage --------------------- */
        if (!state.ID.nop)
        {
            bitset<6> opcode = state.ID.Instr.to_ulong() >> 26;

            // R-Type instruction
            if (opcode == bitset<6>(0))
            {
                newState.EX.is_I_type = false;
                newState.EX.Rs = state.ID.Instr.to_ulong() >> 21;
                newState.EX.Rt = state.ID.Instr.to_ulong() >> 16;
                newState.EX.Wrt_reg_addr = state.ID.Instr.to_ulong() >> 11;
                newState.EX.Read_data1 = myRF.readRF(newState.EX.Rs);
                newState.EX.Read_data2 = myRF.readRF(newState.EX.Rt);
                newState.EX.rd_mem = false;
                newState.EX.wrt_mem = false;
                newState.EX.wrt_enable = true;
                newState.EX.Imm = bitset<16>(0);

                bitset<3> funct = state.ID.Instr.to_ulong();
                newState.EX.alu_op = (funct == bitset<3>("001")); // addu or subu
            }
            else
            {
                // I-Type instruction (lw, sw, bne)
                newState.EX.is_I_type = true;
                newState.EX.alu_op = true;
                newState.EX.Rs = state.ID.Instr.to_ulong() >> 21;
                newState.EX.Read_data1 = myRF.readRF(newState.EX.Rs);
                newState.EX.Imm = state.ID.Instr.to_ulong();
                newState.EX.Rt = state.ID.Instr.to_ulong() >> 16;

                if (opcode == bitset<6>("100011")) // lw
                {
                    newState.EX.Read_data2 = bitset<32>(0);
                    newState.EX.Wrt_reg_addr = newState.EX.Rt;
                    newState.EX.rd_mem = true;
                    newState.EX.wrt_mem = false;
                    newState.EX.wrt_enable = true;
                }
                else if (opcode == bitset<6>("101011")) // sw
                {
                    newState.EX.Read_data2 = myRF.readRF(newState.EX.Rt);
                    newState.EX.Wrt_reg_addr = bitset<5>(0);
                    newState.EX.rd_mem = false;
                    newState.EX.wrt_mem = true;
                    newState.EX.wrt_enable = false;
                }
                else // bne
                {
                    newState.EX.Read_data2 = myRF.readRF(newState.EX.Rt);
                    newState.EX.Wrt_reg_addr = bitset<5>(0);
                    if (newState.EX.Read_data1 != newState.EX.Read_data2)
                    {
                        // Branch taken: squash next instruction and update PC
                        state.IF.PC = state.IF.PC.to_ulong() + sign_extend(newState.EX.Imm).to_ulong() * 4;
                    }
                    newState.EX.rd_mem = false;
                    newState.EX.wrt_mem = false;
                    newState.EX.wrt_enable = false;
                }
            }
        }

        /* --------------------- IF Stage --------------------- */
        if (!state.IF.nop)
        {
            newState.ID.Instr = myInsMem.readInstr(state.IF.PC);

            if (newState.ID.Instr == bitset<32>(0xFFFFFFFF)) // halt
            {
                newState.IF.nop = true;
                newState.ID.nop = true;
            }
            else
            {
                newState.IF.PC = state.IF.PC.to_ulong() + 4;
            }
        }

        // Exit condition: all pipeline stages idle
        if (state.IF.nop && state.ID.nop && state.EX.nop && state.MEM.nop && state.WB.nop)
            break;

        // Dump pipeline state and advance
        printState(newState, cycle);
        state = newState;
        cycle++;
    }

    // Final architectural state output
    myRF.outputRF();
    myDataMem.outputDataMem();
    return 0;
}