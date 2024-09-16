/*
Cache Simulator
Level one L1 and level two L2 cache parameters are read from file (block size, line per set and set per cache).
The 32 bit address is divided into tag bits (t), set index bits (s) and block offset bits (b)
s = log2(#sets)   b = log2(block size in bytes)  t=32-s-b
32 bit address (MSB -> LSB): TAG || SET || OFFSET

Tag Bits   : the tag field along with the valid bit is used to determine whether the block in the cache is valid or not.
Index Bits : the set index field is used to determine which set in the cache the block is stored in.
Offset Bits: the offset field is used to determine which byte in the block is being accessed.
*/

#include <algorithm>
#include <bitset>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <vector>
using namespace std;
//access state:
#define NA 0 // no action
#define RH 1 // read hit
#define RM 2 // read miss
#define WH 3 // Write hit
#define WM 4 // write miss
#define NOWRITEMEM 5 // no write to memory
#define WRITEMEM 6   // write to memory

struct config
{
    int L1blocksize;
    int L1setsize;
    int L1size;
    int L2blocksize;
    int L2setsize;
    int L2size;
};

/*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
// You need to define your cache class here, or design your own data structure for L1 and L2 cache

/*
A single cache block:
    - valid bit (is the data in the block valid?)
    - dirty bit (has the data in the block been modified by means of a write?)
    - tag (the tag bits of the address)
    - data (the actual data stored in the block, in our case, we don't need to store the data)
*/

/*
A CacheSet:
    - a vector of CacheBlocks
    - a counter to keep track of which block to evict next
*/

// You can design your own data structure for L1 and L2 cache; just an example here

struct Block
{
   int valid_bit = 0; //ValidBit
   int dirty_bit = 0; //DirtyBit
   int tag = 0; //TagBit
};

struct Set
{
    vector<Block> block_vec;
    int counter = 0;
};


struct Cache
{
    vector<Set> set_vec;
    int set_size;
    int num_blocks; //Total Number of Blocks in a Cache
    int num_sets; //Total Number of Sets in a Cache
    int block_offset_num; //Number of Bits Required to address each byte of a block
    int set_index_num; //Number of Bits Required to address each set of a cache

    int block_offset = 0;
    int set_index = 0;
    int tag = 0;


    Cache(int blocksize, int setsize, int cachesize)
    {
        
        set_size = setsize;
        if(set_size == 0)
        {
            set_size = 1024*cachesize/blocksize;
        }
        num_blocks = 1024*cachesize/blocksize;
        num_sets = num_blocks/set_size;
        block_offset_num = log2(blocksize);
        set_index_num = log2(num_sets);

        
        set_vec.resize(num_sets);
        for(int i = 0;i<num_sets;i++)
        {
            set_vec[i].block_vec.resize(set_size);
        }
    }

    void get_index(auto addr)
    {
        block_offset = ((addr<<(32-block_offset_num))>>(32-block_offset_num)).to_ulong();
        set_index = (((addr>>block_offset_num)<<(32-set_index_num))>>(32-set_index_num)).to_ulong();
        tag = (addr>>(block_offset_num+set_index_num)).to_ulong();
    }
};

class CacheSystem
{
    public:
    Cache L1 = decltype(L1)(1,1,1);
    Cache L2 = decltype(L2)(1,1,1);

    int L1AcceState = NA; // L1 access state variable, can be one of NA, RH, RM, WH, WM;
    int L2AcceState = NA; // L2 access state variable, can be one of NA, RH, RM, WH, WM;
    int MemAcceState = NOWRITEMEM;

    int i = 0;
    int empty_idx = 0;

    CacheSystem(Cache a, Cache b)
    {
        L1 = a;
        L2 = b;
    }

    int check(Cache L, bitset<32> addr)
    {
        L.get_index(addr);
        int empty_idx = -1;
        for(i = 0; i<L.set_size; i++)
        {
            if(L.set_vec[L.set_index].block_vec[i].valid_bit == 0)
            {
                empty_idx = i;
                break;
            }
        }
        return empty_idx;
    }

    void writeL1(bitset<32> addr){
        /*
        step 1: select the set in our L1 cache using set index bits
        step 2: iterate through each way in the current set
            - If Matching tag and Valid Bit High -> WriteHit!
                                                    -> Dirty Bit High
        step 3: Otherwise? -> WriteMiss!

        return WH or WM
        */
        L1.get_index(addr);
        L1AcceState = WM;
        for(i = 0; i<L1.set_size; i++)
        {
            if(L1.set_vec[L1.set_index].block_vec[i].tag == L1.tag && L1.set_vec[L1.set_index].block_vec[i].valid_bit == 1)
            {
                L1.set_vec[L1.set_index].block_vec[i].dirty_bit = 1;
                L1AcceState = WH;
                L2AcceState = NA;
                MemAcceState = NOWRITEMEM;
                break;
            }
        }
    }

    void writeL2(bitset<32> addr){
        /*
        step 1: select the set in our L2 cache using set index bits
        step 2: iterate through each way in the current set
            - If Matching tag and Valid Bit High -> WriteHit!
                                                -> Dirty Bit High
        step 3: Otherwise? -> WriteMiss!

        return {WM or WH, WRITEMEM or NOWRITEMEM}
        */
        L2.get_index(addr);
        L2AcceState = WM;
        MemAcceState = WRITEMEM;
        for(i = 0; i<L2.set_size; i++)
        {
            if(L2.set_vec[L2.set_index].block_vec[i].tag == L2.tag && L2.set_vec[L2.set_index].block_vec[i].valid_bit == 1)
            {
                L2.set_vec[L2.set_index].block_vec[i].dirty_bit = 1;
                L2AcceState = WH;
                MemAcceState = NOWRITEMEM;
                break;
            }
        }
    }

    void readL1(bitset<32> addr){
        /*
        step 1: select the set in our L1 cache using set index bits
        step 2: iterate through each way in the current set
            - If Matching tag and Valid Bit High -> ReadHit!
        step 3: Otherwise? -> ReadMiss!

        return RH or RM
        */
        L1.get_index(addr);
        L1AcceState = RM;
        for(i = 0; i<L1.set_size; i++)
        {
            if(L1.set_vec[L1.set_index].block_vec[i].tag == L1.tag && L1.set_vec[L1.set_index].block_vec[i].valid_bit == 1)
            {
                L1AcceState = RH;
                L2AcceState = NA;
                MemAcceState = NOWRITEMEM;
                break;
            }
        }
    }
    
    void readL2(bitset<32> addr){
        /*
        step 1: select the set in our L2 cache using set index bits
        step 2: iterate through each way in the current set
            - If Matching tag and Valid Bit High -> ReadHit!
                                                -> copy dirty bit
        step 3: otherwise? -> ReadMiss! -> need to pull data from Main Memory
        step 4: find a place in L1 for our requested data
            - case 1: empty way in L1 -> place requested data
            - case 2: no empty way in L1 -> evict from L1 to L2
                    - case 2.1: empty way in L2 -> place evicted L1 data there
                    - case 2.2: no empty way in L2 -> evict from L2 to memory

        return {RM or RH, WRITEMEM or NOWRITEMEM}
        */
        L2.get_index(addr);
        L2AcceState = RM;
        for(i = 0; i<L2.set_size; i++)
        {
            if(L2.set_vec[L2.set_index].block_vec[i].tag == L2.tag && L2.set_vec[L2.set_index].block_vec[i].valid_bit == 1)
            {
                L2AcceState = RH;
                L2.set_vec[L2.set_index].block_vec[i].valid_bit = 0;
                int temp_dirty = L2.set_vec[L2.set_index].block_vec[i].dirty_bit;
                
                empty_idx = check(L1, addr);
                if(empty_idx == -1)
                {
                    bitset<32> tag_binary = L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].tag;
                    bitset<32> set_binary = L1.set_index;
                    bitset<32> stored_address(0);
                    stored_address = (tag_binary<<(L1.block_offset_num + L1.set_index_num))|(set_binary<<L1.block_offset_num);
                    int stored_dirty_bit = L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].dirty_bit;
                    
                    L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].tag = L1.tag;
                    L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].valid_bit = 1;
                    L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].dirty_bit = temp_dirty;
                    L1.set_vec[L1.set_index].counter = (L1.set_vec[L1.set_index].counter+1) % L1.set_size;

                    L2.get_index(stored_address);
                    empty_idx = check(L2, stored_address);
                    if(empty_idx == -1)
                    {
                        if(L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].dirty_bit == 1)
                        {
                            MemAcceState = WRITEMEM;
                        }   
                        else
                        {
                            MemAcceState = NOWRITEMEM;
                        }  
                        L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].tag = L2.tag;
                        L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].valid_bit = 1;
                        L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].dirty_bit = stored_dirty_bit;
                        L2.set_vec[L2.set_index].counter = (L2.set_vec[L2.set_index].counter+1) % L2.set_size;
                    }

                    else
                    {
                        L2.set_vec[L2.set_index].block_vec[empty_idx].tag = L2.tag;
                        L2.set_vec[L2.set_index].block_vec[empty_idx].valid_bit = 1;
                        L2.set_vec[L2.set_index].block_vec[empty_idx].dirty_bit = stored_dirty_bit;
                        MemAcceState = NOWRITEMEM;
                    }
                }

                else
                {
                    L1.set_vec[L1.set_index].block_vec[empty_idx].tag = L1.tag;
                    L1.set_vec[L1.set_index].block_vec[empty_idx].valid_bit = 1;
                    L1.set_vec[L1.set_index].block_vec[empty_idx].dirty_bit = temp_dirty; 
                    MemAcceState = NOWRITEMEM;
                }
                break;
            }
        }

        if(L2AcceState == RM)
        {
            empty_idx = check(L1, addr);
            if(empty_idx == -1)
            {
                bitset<32> tag_binary = L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].tag;
                bitset<32> set_binary = L1.set_index;
                bitset<32> stored_address(0);
                stored_address = (tag_binary<<(L1.block_offset_num + L1.set_index_num))|(set_binary<<L1.block_offset_num);
                int stored_dirty_bit = L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].dirty_bit;
                
                L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].tag = L1.tag;
                L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].valid_bit = 1;
                L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].dirty_bit = 0;
                L1.set_vec[L1.set_index].counter = (L1.set_vec[L1.set_index].counter+1) % L1.set_size;

                L2.get_index(stored_address);
                empty_idx = check(L2, stored_address);
                if(empty_idx == -1)
                {
                    if(L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].dirty_bit == 1)
                    {
                        MemAcceState = WRITEMEM;
                    }   
                    else
                    {
                        MemAcceState = NOWRITEMEM;
                    }  
                    L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].tag = L2.tag;
                    L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].valid_bit = 1;
                    L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].dirty_bit = stored_dirty_bit;
                    L2.set_vec[L2.set_index].counter = (L2.set_vec[L2.set_index].counter+1) % L2.set_size;
                }

                else
                {
                    L2.set_vec[L2.set_index].block_vec[empty_idx].tag = L2.tag;
                    L2.set_vec[L2.set_index].block_vec[empty_idx].valid_bit = 1;
                    L2.set_vec[L2.set_index].block_vec[empty_idx].dirty_bit = stored_dirty_bit;
                    MemAcceState = NOWRITEMEM;
                }
                

            }
            else
            {
                L1.set_vec[L1.set_index].block_vec[empty_idx].tag = L1.tag;
                L1.set_vec[L1.set_index].block_vec[empty_idx].valid_bit = 1;
                L1.set_vec[L1.set_index].block_vec[empty_idx].dirty_bit = 0;
                MemAcceState = NOWRITEMEM;
            }
        }
    }
};
    

/*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/





int main(int argc, char *argv[])
{

    config cacheconfig;
    ifstream cache_params;
    string dummyLine;
    cache_params.open(argv[1]);
    while (!cache_params.eof())                   // read config file
    {
        cache_params >> dummyLine;                // L1:
        cache_params >> cacheconfig.L1blocksize;  // L1 Block size
        cache_params >> cacheconfig.L1setsize;    // L1 Associativity
        cache_params >> cacheconfig.L1size;       // L1 Cache Size
        cache_params >> dummyLine;                // L2:
        cache_params >> cacheconfig.L2blocksize;  // L2 Block size
        cache_params >> cacheconfig.L2setsize;    // L2 Associativity
        cache_params >> cacheconfig.L2size;       // L2 Cache Size
    }
    ifstream traces;
    ofstream tracesout;
    string outname;
    outname = string(argv[2]) + ".out";
    traces.open(argv[2]);
    tracesout.open(outname.c_str());
    string line;
    string accesstype;     // the Read/Write access type from the memory trace;
    string xaddr;          // the address from the memory trace store in hex;
    unsigned int addr;     // the address from the memory trace store in unsigned int;
    bitset<32> accessaddr; // the address from the memory trace store in the bitset;




/*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
    // Implement by you:
    // initialize the hirearch cache system with those configs
    // probably you may define a Cache class for L1 and L2, or any data structure you like

    if (cacheconfig.L1blocksize!=cacheconfig.L2blocksize){
        printf("please test with the same block size\n");
        return 1;
    }
    // cache c1(cacheconfig.L1blocksize, cacheconfig.L1setsize, cacheconfig.L1size,
    //          cacheconfig.L2blocksize, cacheconfig.L2setsize, cacheconfig.L2size);

    int L1AcceState = NA; // L1 access state variable, can be one of NA, RH, RM, WH, WM;
    int L2AcceState = NA; // L2 access state variable, can be one of NA, RH, RM, WH, WM;
    int MemAcceState = NOWRITEMEM; // Main Memory access state variable, can be either NA or WH;
    
    Cache L1(cacheconfig.L1blocksize, cacheconfig.L1setsize, cacheconfig.L1size);
    Cache L2(cacheconfig.L2blocksize, cacheconfig.L2setsize, cacheconfig.L2size);
    CacheSystem myCache(L1,L2);


    if (traces.is_open() && tracesout.is_open())
    {   
        int line_number = 0;
        while (getline(traces, line))
        { // read mem access file and access Cache
            line_number++;
            // cout<<"Line: "<<line_number<<"\t"<<line<<endl;
            istringstream iss(line);
            if (!(iss >> accesstype >> xaddr)){
                break;
            }
            stringstream saddr(xaddr);
            saddr >> std::hex >> addr;
            accessaddr = bitset<32>(addr);

            // access the L1 and L2 Cache according to the trace;
            if (accesstype.compare("R") == 0)  // a Read request
            {
                // Implement by you:
                //   read access to the L1 Cache,
                //   and then L2 (if required),
                //   update the access state variable;
                //   return: L1AcceState L2AcceState MemAcceState
                
                // For example:
                // L1AcceState = cache.readL1(addr); // read L1
                // if(L1AcceState == RM){
                //     L2AcceState, MemAcceState = cache.readL2(addr); // if L1 read miss, read L2
                // }
                // else{ ... }
                myCache.readL1(accessaddr);
                // cout<<"L1 TAG: "<<myCache.L1.tag<<"\tSET: "<<myCache.L1.set_index<<endl;
                if(myCache.L1AcceState == RM)
                {
                    myCache.readL2(accessaddr);
                    // cout<<"L2 TAG: "<<myCache.L2.tag<<"\tSET: "<<myCache.L2.set_index<<endl;
                }
            }
            
            else{ // a Write request
                // Implement by you:
                //   write access to the L1 Cache, or L2 / main MEM,
                //   update the access state variable;
                //   return: L1AcceState L2AcceState

                // For example:
                // L1AcceState = cache.writeL1(addr);
                // if (L1AcceState == WM){
                //     L2AcceState, MemAcceState = cache.writeL2(addr);
                // }
                // else if(){...}
                myCache.writeL1(accessaddr);
                // cout<<"L1 TAG: "<<myCache.L1.tag<<"\tSET: "<<myCache.L1.set_index<<endl;
                if(myCache.L1AcceState == WM)
                {
                    myCache.writeL2(accessaddr);
                    // cout<<"L2 TAG: "<<myCache.L2.tag<<"\tSET: "<<myCache.L2.set_index<<endl;
                }
            }
            
            L1AcceState = myCache.L1AcceState;
            L2AcceState = myCache.L2AcceState;
            MemAcceState = myCache.MemAcceState;
            
            // cout<<L1AcceState<<L2AcceState<<MemAcceState<<endl;
            // int i;
            // int k;
            
            // cout<<"*********************************************************"<<endl;
            // cout<<"L1"<<endl;
            // for(i = 0; i<myCache.L1.num_sets; i++)
            // {
            //     cout<<"Set"<<i<<" { ";
            //     for(k = 0; k<myCache.L1.set_size; k++)
            //     {
            //         cout<<" || "<<"B"<<k<<" || "<<myCache.L1.set_vec[i].block_vec[k].tag<<" | "<<myCache.L1.set_vec[i].block_vec[k].valid_bit<<" | "<<myCache.L1.set_vec[i].block_vec[k].dirty_bit;
            //     }
            //     cout<<" } "<<endl;
            // }
            // cout<<endl;
            // cout<<"L2"<<endl;
            // for(i = 0; i<myCache.L2.num_sets; i++)
            // {
            //     cout<<"Set"<<i<<" { ";
            //     for(k = 0; k<myCache.L2.set_size; k++)
            //     {
            //         cout<<" || "<<"B"<<k<<" || "<<myCache.L2.set_vec[i].block_vec[k].tag<<" | "<<myCache.L2.set_vec[i].block_vec[k].valid_bit<<" | "<<myCache.L2.set_vec[i].block_vec[k].dirty_bit;
            //     }
            //     cout<<" } "<<endl;
            // }
            // cout<<"*********************************************************"<<endl<<endl;
/*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/




            // Grading: don't change the code below.
            // We will print your access state of each cycle to see if your simulator gives the same result as ours.
            tracesout << L1AcceState << " " << L2AcceState << " " << MemAcceState << endl; // Output hit/miss results for L1 and L2 to the output file;
        } 
        traces.close();
        tracesout.close();
    }
    else
        cout << "Unable to open trace or traceout file ";

    return 0;
}
