#include<iostream>
#include<string>
#include<vector>
#include<bitset>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

#define MemSize (65536)

class PhyMem    
{
  public:
    bitset<32> readdata;  
    PhyMem()
    {
      DMem.resize(MemSize); 
      ifstream dmem;
      string line;
      int i=0;
      dmem.open("pt_initialize.txt");
      if (dmem.is_open())
      {
        while (getline(dmem,line))
        {      
          DMem[i] = bitset<8>(line);
          i++;
        }
      }
      else cout<<"Unable to open page table init file";
      dmem.close();

    }  
    bitset<32> outputMemValue (bitset<12> Address) 
    {    
      bitset<32> readdata;
      /**TODO: implement!
       * Returns the value stored in the physical address 
       */
      string datamem;
      datamem.append(DMem[Address.to_ulong()].to_string());
      datamem.append(DMem[Address.to_ulong()+1].to_string());
      datamem.append(DMem[Address.to_ulong()+2].to_string());
      datamem.append(DMem[Address.to_ulong()+3].to_string());
      readdata = bitset<32>(datamem);	

      return readdata;     
    }              

  private:
    vector<bitset<8> > DMem;

};  

int main(int argc, char *argv[])
{
    PhyMem myPhyMem;

    ifstream traces;
    ifstream PTB_file;
    ofstream tracesout;

    string outname;
    outname = "pt_results.txt";

    traces.open(argv[1]);
    PTB_file.open(argv[2]);
    tracesout.open(outname.c_str());

    //Initialize the PTBR
    bitset<12> PTBR;
    PTB_file >> PTBR;

    string line;
    bitset<14> virtualAddr;

    /*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/

    // Read a virtual address form the PageTable and convert it to the physical address - CSA23
    if(traces.is_open() && tracesout.is_open())
    {
        while (getline(traces, line))
        {
            //TODO: Implement!
            // Access the outer page table 

            // If outer page table valid bit is 1, access the inner page table 

            //Return valid bit in outer and inner page table, physical address, and value stored in the physical memory.
            // Each line in the output file for example should be: 1, 0, 0x000, 0x00000000

            virtualAddr = bitset<14>(line);
            bitset<6> VPO = virtualAddr.to_ulong();
            bitset<4> IPT = virtualAddr.to_ulong()>>6;
            bitset<4> OPT = virtualAddr.to_ulong()>>10;
            
            bitset<32> OPTE = myPhyMem.outputMemValue(bitset<12> (PTBR.to_ulong() + OPT.to_ulong()*4));
            bitset<1> OPT_Valid = OPTE.to_ulong();
            
            bitset<1> IPT_Valid = 0;
            bitset<12> PA = 0;
            bitset<32> data = 0;
            
            if(OPT_Valid == 1)
            {
              bitset<12> PA_IPT = OPTE.to_ulong()>>20;
              bitset<32> IPTE = myPhyMem.outputMemValue(bitset<12> (PA_IPT.to_ulong() + IPT.to_ulong()*4));
              IPT_Valid = IPTE.to_ulong();
              if(IPT_Valid == 1)
              {
                bitset<6> frame = IPTE.to_ulong()>>26;
                PA = frame.to_ulong()<<6 | VPO.to_ulong();
                data = myPhyMem.outputMemValue(PA);
              }

            }

            tracesout<<OPT_Valid<<", "<<IPT_Valid<<", "<<"0x"<<std::setw(3)<<std::setfill('0')<<std::hex<<PA.to_ulong()<<", "<<"0x"<<std::setw(8)<<std::setfill('0')<<std::hex<<data.to_ulong()<<endl;
        }
        traces.close();
        tracesout.close();
    }

    else
        cout << "Unable to open trace or traceout file ";

    return 0;
}
