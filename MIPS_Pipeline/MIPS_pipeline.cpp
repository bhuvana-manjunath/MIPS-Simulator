/*
--------------------------------------------------------
               MIPS 5-Stage Pipeline Simulator
--------------------------------------------------------

This program simulates a simplified MIPS processor using the classic
5-stage pipeline architecture:

    IF  - Instruction Fetch  
    ID  - Instruction Decode 
    EX  - Execute 
    MEM - Memory Access  
    WB  - Write Back

It models core components of the datapath and control flow, including:

  - Memory constraints for instruction and data memory
  - Sign extension for immediate values
  - Structured pipeline registers for each stage
  - A register file (RF) supporting read, write, and output
  - Hazard handling (data forwarding and stalls)
  
Each instruction moves through the pipeline cycle by cycle, with
state output logged at every stage to track execution and debug behavior.
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

// ----------------------------
// Utility Functions
// ----------------------------

/*
 * sign_extend
 * ----------------------------------
 * Takes an N-bit immediate value (e.g., 16-bit) and sign-extends it to 32 bits.
 */
template<size_t N>
bitset<32> sign_extend(bitset<N> imm)
{
    // Replicate the sign bit across the upper bits
    return imm.to_ulong() | 
           (bitset<32>(bitset<16>(-imm[imm.size() - 1]).to_ulong() << 16)).to_ulong();
}

// ----------------------------
// Pipeline Stage Structures
// ----------------------------

/*
 * Each of the following structs represents the data passed 
 * between stages in a pipelined MIPS architecture.
 * These structures encapsulate both data values and control signals.
 */

// IF (Instruction Fetch) Stage
struct IFStruct {
    bitset<32> PC = bitset<32>(0); // Program Counter
    bool nop;                      // True if the stage is executing a NOP (bubble)
};

// ID (Instruction Decode) Stage
struct IDStruct {
    bitset<32> Instr = bitset<32>(0); // Raw 32-bit instruction fetched from memory
    bool nop;
};

// EX (Execute) Stage
struct EXStruct {
    bitset<32> Read_data1 = bitset<32>(0); // Operand 1 from register file
    bitset<32> Read_data2 = bitset<32>(0); // Operand 2 from register file or for store
    bitset<16> Imm = bitset<16>(0);        // Immediate field (for I-type instructions)
    bitset<5> Rs = bitset<5>(0);           // Source register 1
    bitset<5> Rt = bitset<5>(0);           // Source register 2
    bitset<5> Wrt_reg_addr = bitset<5>(0); // Destination register address
    bool is_I_type = 0;                    // True for I-type instruction (uses immediate)
    bool rd_mem = 0;                       // Read memory signal
    bool wrt_mem = 0;                      // Write memory signal
    bool alu_op = 0;                       // ALU operation: 1 for add/lw/sw, 0 for sub
    bool wrt_enable = 0;                   // Write-back to register file
    bool nop;
};

// MEM (Memory Access) Stage
struct MEMStruct {
    bitset<32> ALUresult = bitset<32>(0);   // Result from ALU (address or computed value)
    bitset<32> Store_data = bitset<32>(0);  // Data to store into memory
    bitset<5> Rs = bitset<5>(0);            // Source register 1 (debug/tracking)
    bitset<5> Rt = bitset<5>(0);            // Source register 2 (debug/tracking)
    bitset<5> Wrt_reg_addr = bitset<5>(0);  // Destination register address
    bool rd_mem = 0;                        // Read memory flag
    bool wrt_mem = 0;                       // Write memory flag
    bool wrt_enable = 0;                    // Enable write-back to register file
    bool nop;
};

// WB (Write-Back) Stage
struct WBStruct {
    bitset<32> Wrt_data = bitset<32>(0);    // Final data to write into register file
    bitset<5> Rs = bitset<5>(0);            // Source register 1 (debug/tracking)
    bitset<5> Rt = bitset<5>(0);            // Source register 2 (debug/tracking)
    bitset<5> Wrt_reg_addr = bitset<5>(0);  // Register address to write into
    bool wrt_enable = 0;                    // Write enable signal
    bool nop;
};

// Entire pipeline state bundled into one struct
struct stateStruct {
    IFStruct  IF;
    IDStruct  ID;
    EXStruct  EX;
    MEMStruct MEM;
    WBStruct  WB;
};

// ----------------------------
// Register File Class (RF)
// ----------------------------

/*
 * RF - Register File
 * ----------------------------------
 * Simulates the 32 general-purpose registers in MIPS.
 * Provides methods for reading, writing, and printing register states.
 * Enforces hardware behavior: register $0 is always hardwired to 0.
 */

class RF
{
public:
    bitset<32> Reg_data; 

    // Constructor: initialize all 32 registers, $0 hardwired to 0
    RF()
    {
        Registers.resize(32);             
        Registers[0] = bitset<32>(0);     
    }

    /*
     * readRF
     * ----------------------------------
     * Reads the contents of a register given its 5-bit address.
     */
    bitset<32> readRF(bitset<5> Reg_addr)
    {
        Reg_data = Registers[Reg_addr.to_ulong()];
        return Reg_data;
    }

    /*
     * writeRF
     * ----------------------------------
     * Writes data to a register unless it's register 0.
     * MIPS disallows writing to $zero.
     */
    void writeRF(bitset<5> Reg_addr, bitset<32> Wrt_reg_data)
    {
        if (Reg_addr.to_ulong() != 0)
        {
            Registers[Reg_addr.to_ulong()] = Wrt_reg_data;
        }
    }

    /*
     * outputRF
     * ----------------------------------
     * Dumps the current state of all 32 registers to "RFresult.txt".
     * Useful for debugging or logging pipeline state after each cycle.
     */
    void outputRF()
    {
        ofstream rfout("RFresult.txt", std::ios_base::app); // Append mode
        if (rfout.is_open())
        {
            rfout << "State of RF:\t" << endl;
            for (int j = 0; j < 32; j++)
            {
                rfout << Registers[j] << endl;
            }
        }
        else 
        {
            cout << "Unable to open file" << endl;
        }

        rfout.close();
    }

private:
    vector<bitset<32>> Registers; // 32 registers, each 32 bits wide
};

// ----------------------------
// Instruction Memory (INSMem)
// ----------------------------

/*
 * INSMem - Instruction Memory
 * ----------------------------------
 * Simulates a byte-addressable read-only instruction memory.
 * Loads 8-bit instruction bytes from "imem.txt" into an internal buffer.
 * Supports 32-bit instruction reads (assembled from 4 bytes).
 */

 class INSMem
 {
 public:
     bitset<32> Instruction; // Output of last read (32-bit instruction)
 
     // Constructor: Load instruction memory from file into IMem[]
     INSMem()
     {
         IMem.resize(MemSize); // 1000 bytes of memory
         ifstream imem("imem.txt");
         string line;
         int i = 0;
 
         if (imem.is_open())
         {
             while (getline(imem, line))
             {
                 IMem[i] = bitset<8>(line); // Load 8-bit chunks into memory
                 i++;
             }
         }
         else
         {
             cout << "Unable to open file";
         }
 
         imem.close();
     }
 
     /*
      * readInstr
      * ----------------------------------
      * Reads 4 consecutive bytes from memory starting at ReadAddress
      * and assembles them into a single 32-bit instruction.
      */
     bitset<32> readInstr(bitset<32> ReadAddress)
     {
         string insmem;
 
         // Concatenate four 8-bit instruction bytes into a 32-bit string
         insmem.append(IMem[ReadAddress.to_ulong()].to_string());
         insmem.append(IMem[ReadAddress.to_ulong() + 1].to_string());
         insmem.append(IMem[ReadAddress.to_ulong() + 2].to_string());
         insmem.append(IMem[ReadAddress.to_ulong() + 3].to_string());
 
         Instruction = bitset<32>(insmem);
         return Instruction;
     }
 
 private:
     vector<bitset<8>> IMem; // Byte-addressable instruction memory
 };
 
 // ----------------------------
 // Data Memory (DataMem)
 // ----------------------------
 
 /*
  * DataMem - Data Memory
  * ----------------------------------
  * Simulates byte-addressable data memory.
  * Supports both read and write operations for 32-bit words.
  * Loads initial values from "dmem.txt", and dumps results to "dmemresult.txt".
  */
 
 class DataMem
 {
 public:
     bitset<32> ReadData; // Output of last memory read
 
     // Constructor: Load data memory from file into DMem[]
     DataMem()
     {
         DMem.resize(MemSize); // 1000 bytes of memory
         ifstream dmem("dmem.txt");
         string line;
         int i = 0;
 
         if (dmem.is_open())
         {
             while (getline(dmem, line))
             {
                 DMem[i] = bitset<8>(line); // Load 8-bit chunks into memory
                 i++;
             }
         }
         else
         {
             cout << "Unable to open file";
         }
 
         dmem.close();
     }
 
     /*
      * readDataMem
      * ----------------------------------
      * Reads 4 consecutive bytes from memory starting at Address
      * and assembles them into a single 32-bit word.
      */
     bitset<32> readDataMem(bitset<32> Address)
     {
         string datamem;
 
         // Concatenate four 8-bit memory bytes into a 32-bit word
         datamem.append(DMem[Address.to_ulong()].to_string());
         datamem.append(DMem[Address.to_ulong() + 1].to_string());
         datamem.append(DMem[Address.to_ulong() + 2].to_string());
         datamem.append(DMem[Address.to_ulong() + 3].to_string());
 
         ReadData = bitset<32>(datamem);
         return ReadData;
     }
 
     /*
      * writeDataMem
      * ----------------------------------
      * Splits a 32-bit value into 4 bytes and writes them
      * to memory starting at the given address.
      */
     void writeDataMem(bitset<32> Address, bitset<32> WriteData)
     {
         // Write high to low byte (big-endian order)
         DMem[Address.to_ulong()]     = bitset<8>(WriteData.to_string().substr(0, 8));
         DMem[Address.to_ulong() + 1] = bitset<8>(WriteData.to_string().substr(8, 8));
         DMem[Address.to_ulong() + 2] = bitset<8>(WriteData.to_string().substr(16, 8));
         DMem[Address.to_ulong() + 3] = bitset<8>(WriteData.to_string().substr(24, 8));
     }
 
     /*
      * outputDataMem
      * ----------------------------------
      * Dumps the contents of the entire memory to "dmemresult.txt".
      * Used for checking results after simulation completes.
      */
     void outputDataMem()
     {
         ofstream dmemout("dmemresult.txt");
 
         if (dmemout.is_open())
         {
             for (int j = 0; j < MemSize; j++)
             {
                 dmemout << DMem[j] << endl;
             }
         }
         else
         {
             cout << "Unable to open file";
         }
 
         dmemout.close();
     }
 
 private:
     vector<bitset<8>> DMem; // Byte-addressable data memory
 };
 

// ----------------------------
// Pipeline State Logger
// ----------------------------

/*
 * Logs the current state of all five pipeline stages (IF, ID, EX, MEM, WB)
 * after each simulation cycle. Output is written to "stateresult.txt" in 
 * append mode to preserve the full cycle-by-cycle trace.
 *
 * Each stage's control signals and relevant data values are printed to help
 * visualize how instructions flow through the pipeline and how data evolves.
 */

void printState(stateStruct state, int cycle)
{
    ofstream printstate;
    printstate.open("stateresult.txt", std::ios_base::app); // Append to avoid overwriting

    if (printstate.is_open())
    {
        printstate << "State after executing cycle:\t" << cycle << endl;

        // ---------------- IF Stage ----------------
        printstate << "IF.PC:\t" << state.IF.PC.to_ulong() << endl;
        printstate << "IF.nop:\t" << state.IF.nop << endl;

        // ---------------- ID Stage ----------------
        printstate << "ID.Instr:\t" << state.ID.Instr << endl;
        printstate << "ID.nop:\t" << state.ID.nop << endl;

        // ---------------- EX Stage ----------------
        printstate << "EX.Read_data1:\t"     << state.EX.Read_data1     << endl;
        printstate << "EX.Read_data2:\t"     << state.EX.Read_data2     << endl;
        printstate << "EX.Imm:\t"            << state.EX.Imm            << endl;
        printstate << "EX.Rs:\t"             << state.EX.Rs             << endl;
        printstate << "EX.Rt:\t"             << state.EX.Rt             << endl;
        printstate << "EX.Wrt_reg_addr:\t"   << state.EX.Wrt_reg_addr   << endl;
        printstate << "EX.is_I_type:\t"      << state.EX.is_I_type      << endl;
        printstate << "EX.rd_mem:\t"         << state.EX.rd_mem         << endl;
        printstate << "EX.wrt_mem:\t"        << state.EX.wrt_mem        << endl;
        printstate << "EX.alu_op:\t"         << state.EX.alu_op         << endl;
        printstate << "EX.wrt_enable:\t"     << state.EX.wrt_enable     << endl;
        printstate << "EX.nop:\t"            << state.EX.nop            << endl;

        // ---------------- MEM Stage ----------------
        printstate << "MEM.ALUresult:\t"     << state.MEM.ALUresult     << endl;
        printstate << "MEM.Store_data:\t"    << state.MEM.Store_data    << endl;
        printstate << "MEM.Rs:\t"            << state.MEM.Rs            << endl;
        printstate << "MEM.Rt:\t"            << state.MEM.Rt            << endl;
        printstate << "MEM.Wrt_reg_addr:\t"  << state.MEM.Wrt_reg_addr  << endl;
        printstate << "MEM.rd_mem:\t"        << state.MEM.rd_mem        << endl;
        printstate << "MEM.wrt_mem:\t"       << state.MEM.wrt_mem       << endl;
        printstate << "MEM.wrt_enable:\t"    << state.MEM.wrt_enable    << endl;
        printstate << "MEM.nop:\t"           << state.MEM.nop           << endl;

        // ---------------- WB Stage ----------------
        printstate << "WB.Wrt_data:\t"       << state.WB.Wrt_data       << endl;
        printstate << "WB.Rs:\t"             << state.WB.Rs             << endl;
        printstate << "WB.Rt:\t"             << state.WB.Rt             << endl;
        printstate << "WB.Wrt_reg_addr:\t"   << state.WB.Wrt_reg_addr   << endl;
        printstate << "WB.wrt_enable:\t"     << state.WB.wrt_enable     << endl;
        printstate << "WB.nop:\t"            << state.WB.nop            << endl;
    }
    else 
    {
        cout << "Unable to open file";
    }

    printstate.close(); 
}

/*
--------------------------------------------------------
       MIPS Pipeline Simulator - Main Loop
--------------------------------------------------------

Simulates a simplified 5-stage MIPS processor:
    IF → ID → EX → MEM → WB

This loop executes cycle by cycle:
  - Simulates instruction flow through the pipeline
  - Handles data forwarding and stalling
  - Applies control and memory operations
  - Dumps state to files for post-run analysis
*/

int main()
{

    // Initialize modules
    RF myRF;               // Register file
    INSMem myInsMem;       // Instruction memory
    DataMem myDataMem;     // Data memory
    stateStruct state, newState;  // Holds pipeline state
    int cycle = 0;         // Simulation cycle count

    // Set initial nop states
    state.IF.nop = 0;  
    state.ID.nop = state.EX.nop = state.MEM.nop = state.WB.nop = 1;
    newState = state;  

    while (1) {
        // Propagate NOPs down the pipeline
        newState.ID.nop = state.IF.nop;
        newState.EX.nop = state.ID.nop;
        newState.MEM.nop = state.EX.nop;
        newState.WB.nop = state.MEM.nop;
        
        /* --------------------- WB stage --------------------- */
        if(state.WB.nop == false)
        {
            if(state.WB.wrt_enable == 1)
            {
                myRF.writeRF(state.WB.Wrt_reg_addr, state.WB.Wrt_data);
            }
        }

        /* --------------------- MEM stage --------------------- */
        if(state.MEM.nop == false)
        {
            newState.WB.Rs = state.MEM.Rs;
            newState.WB.Rt = state.MEM.Rt;
            newState.WB.Wrt_reg_addr = state.MEM.Wrt_reg_addr;
            newState.WB.wrt_enable = state.MEM.wrt_enable;
            newState.WB.Wrt_data = state.MEM.ALUresult;

            if(state.MEM.rd_mem == true && state.MEM.wrt_mem == false)
            {
                newState.WB.Wrt_data = myDataMem.readDataMem(state.MEM.ALUresult);
            }

            else if(state.MEM.wrt_mem == true && state.MEM.rd_mem == false)
            {
                myDataMem.writeDataMem(state.MEM.ALUresult, state.MEM.Store_data);
                newState.WB.Wrt_data = bitset<32> (0);
            }
        }

        /* --------------------- EX stage --------------------- */
        if(state.EX.Rs == state.MEM.Wrt_reg_addr && state.MEM.Wrt_reg_addr != 0)
            {
                if(state.MEM.rd_mem == true)
                {
                    state.EX.nop = true;
                    state.ID.nop = true;
                    state.IF.nop = true;
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
        
        else if(state.EX.Rs == state.WB.Wrt_reg_addr && state.WB.Wrt_reg_addr != 0)
            {
                state.EX.Read_data1 = state.WB.Wrt_data;
                newState.EX.Read_data1 = state.WB.Wrt_data;
            }
        
        if(state.EX.Rt == state.MEM.Wrt_reg_addr && state.MEM.Wrt_reg_addr != 0)
            {
                if(state.MEM.rd_mem == true)
                {
                    state.EX.nop = true;
                    state.ID.nop = true;
                    state.IF.nop = true;
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
        else if(state.EX.Rt == state.WB.Wrt_reg_addr && state.WB.Wrt_reg_addr != 0)
            {
                state.EX.Read_data2 = state.WB.Wrt_data;
                newState.EX.Read_data2 = state.WB.Wrt_data;
            }

        
        if (state.EX.nop == false)
        {
            newState.MEM.Rs = state.EX.Rs;
            newState.MEM.Rt = state.EX.Rt;
            newState.MEM.Wrt_reg_addr = state.EX.Wrt_reg_addr;
            newState.MEM.rd_mem = state.EX.rd_mem;
            newState.MEM.wrt_mem = state.EX.wrt_mem;
            newState.MEM.wrt_enable = state.EX.wrt_enable;

            if (state.EX.is_I_type)
            {
                if(state.EX.rd_mem == 0 && state.EX.wrt_mem == 0 && state.EX.wrt_enable == 0)
                {
                    newState.MEM.ALUresult = bitset<32> (0);
                    newState.MEM.Store_data = bitset<32> (0);
                }

                else
                {
                    newState.MEM.ALUresult = state.EX.Read_data1.to_ulong() + sign_extend(state.EX.Imm).to_ulong();
                    newState.MEM.Store_data = state.EX.Read_data2;
                }

            }

            else
            {
                if(state.EX.alu_op == true)
                {
                    newState.MEM.ALUresult = state.EX.Read_data1.to_ulong() + state.EX.Read_data2.to_ulong();
                    newState.MEM.Store_data == bitset<32> (0);
                }
                else
                {
                    newState.MEM.ALUresult = state.EX.Read_data1.to_ulong() - state.EX.Read_data2.to_ulong();
                    newState.MEM.Store_data == bitset<32> (0);
                }

            }
        }

        /* --------------------- ID stage --------------------- */
        if(state.ID.nop == false)
        {
            bitset<6> opcode = state.ID.Instr.to_ulong()>>26;

            // R-type
            if(opcode == bitset<6>(0))
            {
                newState.EX.is_I_type = false;
                newState.EX.Rs = state.ID.Instr.to_ulong()>>21;
                newState.EX.Rt = state.ID.Instr.to_ulong()>>16;
                newState.EX.Wrt_reg_addr = state.ID.Instr.to_ulong()>>11;
                newState.EX.Read_data1 = myRF.readRF(newState.EX.Rs);
                newState.EX.Read_data2 = myRF.readRF(newState.EX.Rt);
                newState.EX.rd_mem = false;
                newState.EX.wrt_mem = false;
                newState.EX.wrt_enable = true;
                newState.EX.Imm = bitset<16> (0);

                bitset<3> ALUop = state.ID.Instr.to_ulong();
                if (ALUop == bitset<3> ("001"))
                {
                   newState.EX.alu_op = true;
                }
                if (ALUop == bitset<3> ("011"))
                {
                   newState.EX.alu_op = false;
                }
            }

            // I-type
            else
            {
                newState.EX.is_I_type = true;
                newState.EX.alu_op = true;
                newState.EX.Rs = state.ID.Instr.to_ulong()>>21;
                newState.EX.Read_data1 = myRF.readRF(newState.EX.Rs);
                newState.EX.Imm = newState.ID.Instr.to_ulong();
                //LW
                if (opcode == bitset<6>("100011"))
                {
                    newState.EX.Rt = bitset<5> (0);
                    newState.EX.Read_data2 = bitset<32> (0);
                    newState.EX.Wrt_reg_addr = state.ID.Instr.to_ulong()>>16;
                    newState.EX.rd_mem = true;
                    newState.EX.wrt_mem = false;
                    newState.EX.wrt_enable = true;
                }
                //SW
                else if (opcode == bitset<6>("101011"))
                {
                    newState.EX.Rt = state.ID.Instr.to_ulong()>>16;
                    newState.EX.Read_data2 = myRF.readRF(newState.EX.Rt);
                    newState.EX.Wrt_reg_addr = bitset<5> (0);
                    newState.EX.rd_mem = false;
                    newState.EX.wrt_mem = true;
                    newState.EX.wrt_enable = false;
                }
                //BNE
                else
                {
                    newState.EX.Rt = state.ID.Instr.to_ulong()>>16;
                    newState.EX.Read_data2 = myRF.readRF(newState.EX.Rt);
                    newState.EX.Wrt_reg_addr = bitset<5> (0);
                    if(newState.EX.Read_data1 != newState.EX.Read_data2)
                    {
                        state.IF.PC = state.IF.PC.to_ulong() + sign_extend(newState.EX.Imm).to_ulong()*4;
                    }
                    newState.EX.rd_mem = false;
                    newState.EX.wrt_mem = false;
                    newState.EX.wrt_enable = false;
                }
            }

        }

        /* --------------------- IF stage --------------------- */
        if (state.IF.nop == false)
        {
            cout<<"IF stage"<<endl;
            cout<<"IF.PC:\t"<<state.IF.PC.to_ulong()<<endl;
            cout<<"\n"<<endl;

            newState.ID.Instr = myInsMem.readInstr(state.IF.PC);

            if (newState.ID.Instr == bitset<32> (0xFFFFFFFF))
            {
                newState.IF.nop = true;
                newState.ID.nop = true;
            }

            else
            {
                newState.IF.PC = state.IF.PC.to_ulong() + 4;
            }
        }


        if (state.IF.nop && state.ID.nop && state.EX.nop && state.MEM.nop && state.WB.nop)
            break;


        printState(newState, cycle); 
        state = newState; 
        cycle++;
    }

    // Final state dumps
    myRF.outputRF(); // dump RF
	myDataMem.outputDataMem(); // dump data mem
	return 0;
}