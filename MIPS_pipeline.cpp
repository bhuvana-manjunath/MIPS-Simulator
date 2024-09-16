#include<iostream>
#include<string>
#include<vector>
#include<bitset>
#include<fstream>
using namespace std;

#define MemSize 1000 // memory size, in reality, the memory size should be 2^32, but for this lab csa23, for the space resaon, we keep it as this large number, but the memory is still 32-bit addressable.

//Sign Extend Function
template<size_t N>
bitset<32> sign_extend(bitset<N> imm)
{
    return imm.to_ulong()|(bitset<32>(bitset<16>(-imm[imm.size()-1]).to_ulong()<<16)).to_ulong();
}

struct IFStruct {
    bitset<32>  PC = bitset<32>(0);
    bool        nop;
};

struct IDStruct {
    bitset<32>  Instr = bitset<32>(0);
    bool        nop;
};

struct EXStruct {
    bitset<32>  Read_data1 = bitset<32>(0);
    bitset<32>  Read_data2 = bitset<32>(0);
    bitset<16>  Imm = bitset<16>(0);
    bitset<5>   Rs = bitset<5> (0);
    bitset<5>   Rt = bitset<5> (0);
    bitset<5>   Wrt_reg_addr = bitset<5> (0);
    bool        is_I_type = 0;
    bool        rd_mem = 0;
    bool        wrt_mem = 0;
    bool        alu_op = 0;     //1 for addu, lw, sw, 0 for subu
    bool        wrt_enable = 0;
    bool        nop;
};

struct MEMStruct {
    bitset<32>  ALUresult = bitset<32>(0);
    bitset<32>  Store_data = bitset<32>(0);
    bitset<5>   Rs = bitset<5> (0);
    bitset<5>   Rt = bitset<5> (0);
    bitset<5>   Wrt_reg_addr = bitset<5> (0);
    bool        rd_mem = 0;
    bool        wrt_mem = 0;
    bool        wrt_enable = 0;
    bool        nop;
};

struct WBStruct {
    bitset<32>  Wrt_data = bitset<32>(0);
    bitset<5>   Rs = bitset<5> (0);
    bitset<5>   Rt = bitset<5> (0);
    bitset<5>   Wrt_reg_addr = bitset<5> (0);
    bool        wrt_enable = 0;
    bool        nop;
};

struct stateStruct {
    IFStruct    IF;
    IDStruct    ID;
    EXStruct    EX;
    MEMStruct   MEM;
    WBStruct    WB;
};

class RF
{
    public:
        bitset<32> Reg_data;
     	RF()
    	{
			Registers.resize(32);
            Registers[0] = bitset<32> (0);

        }

        bitset<32> readRF(bitset<5> Reg_addr)
        {
            Reg_data = Registers[Reg_addr.to_ulong()];
            return Reg_data;
        }

        void writeRF(bitset<5> Reg_addr, bitset<32> Wrt_reg_data)
        {
            Registers[Reg_addr.to_ulong()] = Wrt_reg_data;
        }

		void outputRF()
		{
			ofstream rfout;
			rfout.open("RFresult.txt",std::ios_base::app);
			if (rfout.is_open())
			{
				rfout<<"State of RF:\t"<<endl;
				for (int j = 0; j<32; j++)
				{
					rfout << Registers[j]<<endl;
				}
			}
			else cout<<"Unable to open file";
			rfout.close();
		}

	private:
		vector<bitset<32> >Registers;
};

class INSMem
{
	public:
        bitset<32> Instruction;
        INSMem()
        {
			IMem.resize(MemSize);
            ifstream imem;
			string line;
			int i=0;
			imem.open("imem.txt");
			if (imem.is_open())
			{
				while (getline(imem,line))
				{
					IMem[i] = bitset<8>(line);
					i++;
				}
			}
            else cout<<"Unable to open file";
			imem.close();
		}

		bitset<32> readInstr(bitset<32> ReadAddress)
		{
			string insmem;
			insmem.append(IMem[ReadAddress.to_ulong()].to_string());
			insmem.append(IMem[ReadAddress.to_ulong()+1].to_string());
			insmem.append(IMem[ReadAddress.to_ulong()+2].to_string());
			insmem.append(IMem[ReadAddress.to_ulong()+3].to_string());
			Instruction = bitset<32>(insmem);		//read instruction memory
			return Instruction;
		}

    private:
        vector<bitset<8> > IMem;
};

class DataMem
{
    public:
        bitset<32> ReadData;
        DataMem()
        {
            DMem.resize(MemSize);
            ifstream dmem;
            string line;
            int i=0;
            dmem.open("dmem.txt");
            if (dmem.is_open())
            {
                while (getline(dmem,line))
                {
                    DMem[i] = bitset<8>(line);
                    i++;
                }
            }
            else cout<<"Unable to open file";
                dmem.close();
        }

        bitset<32> readDataMem(bitset<32> Address)
        {
			string datamem;
            datamem.append(DMem[Address.to_ulong()].to_string());
            datamem.append(DMem[Address.to_ulong()+1].to_string());
            datamem.append(DMem[Address.to_ulong()+2].to_string());
            datamem.append(DMem[Address.to_ulong()+3].to_string());
            ReadData = bitset<32>(datamem);		//read data memory
            return ReadData;
		}

        void writeDataMem(bitset<32> Address, bitset<32> WriteData)
        {
            DMem[Address.to_ulong()] = bitset<8>(WriteData.to_string().substr(0,8));
            DMem[Address.to_ulong()+1] = bitset<8>(WriteData.to_string().substr(8,8));
            DMem[Address.to_ulong()+2] = bitset<8>(WriteData.to_string().substr(16,8));
            DMem[Address.to_ulong()+3] = bitset<8>(WriteData.to_string().substr(24,8));
        }

        void outputDataMem()
        {
            ofstream dmemout;
            dmemout.open("dmemresult.txt");
            if (dmemout.is_open())
            {
                for (int j = 0; j< 1000; j++)
                {
                    dmemout << DMem[j]<<endl;
                }

            }
            else cout<<"Unable to open file";
            dmemout.close();
        }

    private:
		vector<bitset<8> > DMem;
};

void printState(stateStruct state, int cycle)
{
    ofstream printstate;
    printstate.open("stateresult.txt", std::ios_base::app);
    if (printstate.is_open())
    {
        printstate<<"State after executing cycle:\t"<<cycle<<endl; 
        
        printstate<<"IF.PC:\t"<<state.IF.PC.to_ulong()<<endl;        
        printstate<<"IF.nop:\t"<<state.IF.nop<<endl; 
        
        printstate<<"ID.Instr:\t"<<state.ID.Instr<<endl; 
        printstate<<"ID.nop:\t"<<state.ID.nop<<endl;
        
        printstate<<"EX.Read_data1:\t"<<state.EX.Read_data1<<endl;
        printstate<<"EX.Read_data2:\t"<<state.EX.Read_data2<<endl;
        printstate<<"EX.Imm:\t"<<state.EX.Imm<<endl; 
        printstate<<"EX.Rs:\t"<<state.EX.Rs<<endl;
        printstate<<"EX.Rt:\t"<<state.EX.Rt<<endl;
        printstate<<"EX.Wrt_reg_addr:\t"<<state.EX.Wrt_reg_addr<<endl;
        printstate<<"EX.is_I_type:\t"<<state.EX.is_I_type<<endl; 
        printstate<<"EX.rd_mem:\t"<<state.EX.rd_mem<<endl;
        printstate<<"EX.wrt_mem:\t"<<state.EX.wrt_mem<<endl;        
        printstate<<"EX.alu_op:\t"<<state.EX.alu_op<<endl;
        printstate<<"EX.wrt_enable:\t"<<state.EX.wrt_enable<<endl;
        printstate<<"EX.nop:\t"<<state.EX.nop<<endl;        

        printstate<<"MEM.ALUresult:\t"<<state.MEM.ALUresult<<endl;
        printstate<<"MEM.Store_data:\t"<<state.MEM.Store_data<<endl; 
        printstate<<"MEM.Rs:\t"<<state.MEM.Rs<<endl;
        printstate<<"MEM.Rt:\t"<<state.MEM.Rt<<endl;   
        printstate<<"MEM.Wrt_reg_addr:\t"<<state.MEM.Wrt_reg_addr<<endl;              
        printstate<<"MEM.rd_mem:\t"<<state.MEM.rd_mem<<endl;
        printstate<<"MEM.wrt_mem:\t"<<state.MEM.wrt_mem<<endl; 
        printstate<<"MEM.wrt_enable:\t"<<state.MEM.wrt_enable<<endl;         
        printstate<<"MEM.nop:\t"<<state.MEM.nop<<endl;        

        printstate<<"WB.Wrt_data:\t"<<state.WB.Wrt_data<<endl;
        printstate<<"WB.Rs:\t"<<state.WB.Rs<<endl;
        printstate<<"WB.Rt:\t"<<state.WB.Rt<<endl;        
        printstate<<"WB.Wrt_reg_addr:\t"<<state.WB.Wrt_reg_addr<<endl;
        printstate<<"WB.wrt_enable:\t"<<state.WB.wrt_enable<<endl;        
        printstate<<"WB.nop:\t"<<state.WB.nop<<endl; 
    }
    else cout<<"Unable to open file";
    printstate.close();
}

int main()
{

    RF myRF;
    INSMem myInsMem;
    DataMem myDataMem;
    stateStruct state, newState;
    int cycle = 0;

    state.IF.nop = 0;
    state.ID.nop = state.EX.nop = state.MEM.nop = state.WB.nop = 1;
    newState.IF.nop = 0;
    newState.ID.nop = newState.EX.nop = newState.MEM.nop = newState.WB.nop = 1;

    while (1) {
        cout<<"*****************************************"<<endl;
        cout<<"Cycle: "<<cycle<<endl<<"\n"<<endl;
        cout<<"*****************************************"<<endl;
        
        
        cout<<"\n"<<"IF.nop: \t"<<state.IF.nop<<endl;
        cout<<"ID.nop: \t"<<state.ID.nop<<endl;
        cout<<"EX.nop: \t"<<state.EX.nop<<endl;
        cout<<"MEM.nop: \t"<<state.MEM.nop<<endl;
        cout<<"WB.nop: \t"<<state.WB.nop<<"\n"<<endl;
        
        newState.ID.nop = state.IF.nop;
        newState.EX.nop = state.ID.nop;
        newState.MEM.nop = state.EX.nop;
        newState.WB.nop = state.MEM.nop;
        
        
        /* --------------------- WB stage --------------------- */
        if(state.WB.nop == false)
        {
            cout<<"WB stage"<<endl;
            cout<<"WB.Wrt_data:\t"<<state.WB.Wrt_data<<endl;
            cout<<"WB.Rs:\t"<<state.WB.Rs<<endl;
            cout<<"WB.Rt:\t"<<state.WB.Rt<<endl;
            cout<<"WB.Wrt_reg_addr:\t"<<state.WB.Wrt_reg_addr<<endl;
            cout<<"WB.wrt_enable:\t"<<state.WB.wrt_enable<<endl;
            cout<<"\n"<<endl;

            if(state.WB.wrt_enable == 1)
            {
                myRF.writeRF(state.WB.Wrt_reg_addr, state.WB.Wrt_data);
            }
        }


        /* --------------------- MEM stage --------------------- */
        if(state.MEM.nop == false)
        {
            cout<<"MEM stage"<<endl;
            cout<<"MEM.ALUresult:\t"<<state.MEM.ALUresult<<endl;
            cout<<"MEM.Store_data:\t"<<state.MEM.Store_data<<endl;
            cout<<"MEM.Rs:\t"<<state.MEM.Rs<<endl;
            cout<<"MEM.Rt:\t"<<state.MEM.Rt<<endl;
            cout<<"MEM.Wrt_reg_addr:\t"<<state.MEM.Wrt_reg_addr<<endl;
            cout<<"MEM.rd_mem:\t"<<state.MEM.rd_mem<<endl;
            cout<<"MEM.wrt_mem:\t"<<state.MEM.wrt_mem<<endl;
            cout<<"MEM.wrt_enable:\t"<<state.MEM.wrt_enable<<endl;
            cout<<"\n"<<endl;

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
            cout<<"EX stage"<<endl;
            cout<<"EX.Read_data1:\t"<<state.EX.Read_data1<<endl;
            cout<<"EX.Read_data2:\t"<<state.EX.Read_data2<<endl;
            cout<<"EX.Imm:\t"<<state.EX.Imm<<endl;
            cout<<"EX.Rs:\t"<<state.EX.Rs<<endl;
            cout<<"EX.Rt:\t"<<state.EX.Rt<<endl;
            cout<<"EX.Wrt_reg_addr:\t"<<state.EX.Wrt_reg_addr<<endl;
            cout<<"EX.is_I_type:\t"<<state.EX.is_I_type<<endl;
            cout<<"EX.rd_mem:\t"<<state.EX.rd_mem<<endl;
            cout<<"EX.wrt_mem:\t"<<state.EX.wrt_mem<<endl;
            cout<<"EX.alu_op:\t"<<state.EX.alu_op<<endl;
            cout<<"EX.wrt_enable:\t"<<state.EX.wrt_enable<<endl;
            cout<<"\n"<<endl;

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
            cout<<"ID stage"<<endl;
            cout<<"ID.Instr:\t"<<state.ID.Instr<<endl;
            cout<<"\n"<<endl;

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


        printState(newState, cycle); //print states after executing cycle 0, cycle 1, cycle 2 ...
        state = newState; /*** The end of the cycle and updates the current state with the values calculated in this cycle. csa23 ***/
        cycle++;
    }

    myRF.outputRF(); // dump RF
	myDataMem.outputDataMem(); // dump data mem
	return 0;
}