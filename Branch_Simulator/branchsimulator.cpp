#include <iostream>
#include <fstream>
#include <math.h>
#include <vector>
#include <bitset>
#include <sstream>

using namespace std;

struct BHT
{
	vector<vector<bitset<1>>> table;

	BHT(int w, int h)
	{
		table.resize(pow(2, h));
		for (int i = 0; i < table.size(); i++)
		{
			table[i].resize(w, 0);
		}
	}
};

int main(int argc, char **argv)
{
	ifstream config;
	config.open(argv[1]);

	int m, w, h;
	config >> m;
	config >> h;
	config >> w;
	config.close();

	ofstream out;
	string out_file_name = string(argv[2]) + ".out";
	out.open(out_file_name.c_str());

	ifstream trace;
	trace.open(argv[2]);

	// Initialize PHT
	vector<bitset<2>> pht;
	pht.resize(pow(2, m), bitset<2>(2));

	// Initialize BHT
	BHT bht(w, h);

	// Prediction Variables
	int pred = 0;
	int truth = 0;

	string line;

	while (!trace.eof())
	{
		// Get PC Address and True Branch Prediction - Taken (1) or Not Taken(0)
		getline(trace, line);

		if (line.empty())
		{
			continue; // Skip empty lines
		}

		istringstream iss(line);
		string addrHex;

		// Read Hexadecimal from String
		iss >> addrHex >> truth;

		// Hexadecimal->integer
		unsigned int AddrInt;
		stringstream ss;
		ss << std::hex << addrHex;
		ss >> AddrInt;

		// Integer->Bitset<32>
		bitset<32> addr = AddrInt;

		// Get BHT Index - Last H bits from the hth LSB of PC Addr
		bitset<32> BHTIndex = (addr >> 2) & bitset<32>(pow(2, h) - 1);

		// Get BHT Entry
		vector<bitset<1>> BHTEntry = bht.table[BHTIndex.to_ulong()];

		// Get PHT Index, (m-w)(2bits) bits from the 3rd LSB of PC addr CONCAT with BHTEntry(3Bits) Total 5bits
		bitset<32> PHTIndex = ((addr >> 2) & bitset<32>(pow(2, m - w) - 1));

		// Concatenation to get PHT Index
		for (int i = 0; i < BHTEntry.size(); i++)
		{
			PHTIndex <<= 1;
			PHTIndex |= bitset<32>(BHTEntry[i][0]);
		}

		// Get PHT Entry
		bitset<2> PHTEntry = pht[PHTIndex.to_ulong()].to_ulong();

		// Case 1 - Pred -> Not taken (PHT = 00)
		if (PHTEntry == bitset<2>(00))
		{
			pred = 0; // Branch Prediction = NOT TAKEN

			// If Truth 0, then PHT = Strong NT else Weak NT
			truth == 0 ? pht[PHTIndex.to_ulong()] = bitset<2>("00") : pht[PHTIndex.to_ulong()] = bitset<2>("01");
		}

		// Case 2 - Pred -> Not taken (PHT = 01)
		else if (PHTEntry == bitset<2>(01))
		{
			pred = 0; // Branch Prediction = NOT TAKEN

			// If Truth = 0, then PHT Entry = Stongly NT else Weakly Taken
			truth == 0 ? pht[PHTIndex.to_ulong()] = bitset<2>("00") : pht[PHTIndex.to_ulong()] = bitset<2>("10");
		}

		// Case 3 - Pred -> taken (PHT = 10)
		else if (PHTEntry == bitset<2>(10))
		{
			pred = 1; // Branch Prediction = TAKEN

			// If Truth  == 0, then PHT Entry is Weakly NT else Strongly Taken
			truth == 0 ? pht[PHTIndex.to_ulong()] = bitset<2>("01") : pht[PHTIndex.to_ulong()] = bitset<2>("11");
		}

		// Case 4 - Pred -> taken (PHT = 11)
		else
		{
			pred = 1; // Branch Prediction = TAKEN

			// If Truth  == 1, then PHT Entry is Strongly Taken else Weakly Taken
			truth == 1 ? pht[PHTIndex.to_ulong()] = bitset<2>("11") : pht[PHTIndex.to_ulong()] = bitset<2>("10");
		}

		// Update BHT Entry

		// bht.table[BHTIndex.to_ulong()].erase(bht.table[BHTIndex.to_ulong()].begin());
		// bht.table[BHTIndex.to_ulong()].push_back(truth); // Push 0 to BHT

		vector<bitset<1>> v = bht.table[BHTIndex.to_ulong()];
		for (int i = 0; i < v.size() - 1; i++)
		{
			v[i] = v[i + 1];
		}
		v[v.size() - 1] = truth;
		bht.table[BHTIndex.to_ulong()] = v;
		out << pred << endl;
	}

	trace.close();
	out.close();
}

// Path: branchsimulator_skeleton_23.cpp