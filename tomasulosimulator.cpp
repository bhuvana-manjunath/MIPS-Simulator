#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <limits>
#include <sstream>
#include <map>
#include <iomanip>

using std::cout;
using std::endl;
using std::string;
using std::vector;

string inputtracename = "trace.txt";
// remove the ".txt" and add ".out.txt" to the end as output name
string outputtracename = inputtracename.substr(0, inputtracename.length() - 4) + ".out.txt";
string hardwareconfigname = "config.txt";

// Common Data Bus
struct CDBMessage
{
	std::string stationName;
	int remainCycle; // Name of the functional unit (reservation station)
	int op;
	CDBMessage(const std::string &name, int cycle, int opInt)
	{
		stationName = name;
		remainCycle = cycle;
		op = opInt;
	}
};

class CommonDataBus
{
private:
	std::vector<CDBMessage> messages;

public:
	// Function to broadcast a message on the CDB
	void broadcast(const std::string &stationName, int rcycle, int op)
	{
		CDBMessage message(stationName, rcycle, op);
		messages.push_back(message);
	}

	// Function to get the last message from the CDB
	CDBMessage getLastMessage()
	{
		CDBMessage smallest("", 99, -1);
		if (!messages.empty())
		{
			// Initialize with default values
			smallest.remainCycle = std::numeric_limits<int>::max(); // Set to maximum possible value

			for (const auto &m : messages)
			{
				if (m.remainCycle < smallest.remainCycle)
				{
					smallest = m;
				}
			}
			return smallest;
		}
		else
		{
			// Return a default message if the CDB is empty
			smallest = CDBMessage("NONE", 1, -1);
		}
		// cout << "RS with the least IssueCycle updates RRS: " << smallest.stationName << endl;
		return smallest;
	}

	// To print the CDB Queue
	void printCDB()
	{
		for (auto &c : messages)
		{
			cout << "|| Name: " << c.stationName << " Issue Cycle: " << c.remainCycle << " Op: " << c.op << " ||    ";
		}
		cout << endl;
	}

	// Removes the CDB Message from the Messages Queue with the smallest IssueCycle
	CDBMessage RemoveSmallest()
	{
		if (!messages.empty())
		{
			auto smallest = messages.begin();
			for (auto it = messages.begin() + 1; it != messages.end(); ++it)
			{
				if (it->remainCycle < smallest->remainCycle)
				{
					smallest = it;
				}
			}

			CDBMessage smallestMessage = *smallest;
			messages.erase(smallest); // Remove the smallest message from the vector
			return smallestMessage;
		}
		else
		{
			// If the vector is empty, return a default message
			return CDBMessage("NONE", 1, -1);
		}
	}
};

CommonDataBus cdb;

// Operations
enum Operation
{
	ADD,
	SUB,
	MULT,
	DIV,
	LOAD,
	STORE
};

// The execute cycle of each operation: ADD, SUB, MULT, DIV, LOAD, STORE
const int OperationCycle[6] = {2, 2, 10, 40, 2, 2};

// Hardware Configuration
struct HardwareConfig
{
	int LoadRSsize;	 // number of load reservation stations
	int StoreRSsize; // number of store reservation stations
	int AddRSsize;	 // number of add reservation stations
	int MultRSsize;	 // number of multiply reservation stations
	int FRegSize;	 // number of fp registers
};

// Structure for Instructions
struct Instruction
{
	Operation op;
	string destRegister;
	std::string srcReg1;
	std::string srcReg2;
};

// Helper function to read instructions from a file and store in a vector
std::vector<Instruction> readInstruction(const std::string &filename)
{
	std::ifstream file(filename);
	std::vector<Instruction> instructions;

	if (file.is_open())
	{
		std::string line;
		while (std::getline(file, line))
		{
			std::istringstream iss(line);
			std::string opStr, destReg, srcReg1, srcReg2;
			iss >> opStr >> destReg >> srcReg1 >> srcReg2;

			Operation op;
			if (opStr == "ADD")
			{
				op = Operation::ADD;
			}
			else if (opStr == "SUB")
			{
				op = Operation::SUB;
			}
			else if (opStr == "MULT")
			{
				op = Operation::MULT;
			}
			else if (opStr == "DIV")
			{
				op = Operation::DIV;
			}
			else if (opStr == "LOAD")
			{
				op = Operation::LOAD;
			}
			else if (opStr == "STORE")
			{
				op = Operation::STORE;
			}
			// int destRegister = extractNumericPart(destReg);
			instructions.push_back({op, destReg, srcReg1, srcReg2});
		}
		file.close();
	}
	else
	{
		std::cout << "Unable to open file " << filename << std::endl;
	}

	return instructions;
}

// Helper function to extract numeric part of fp register
int extractNumericPart(const std::string &str)
{
	std::string numericPart;
	for (char ch : str)
	{
		if (std::isdigit(ch))
		{
			numericPart += ch;
		}
	}
	return std::stoi(numericPart);
}

// Structure to record the time of each instruction
struct InstructionStatus
{
	int cycleIssued;
	int cycleExecuted; // execution completed
	int cycleWriteResult;
	bool isComplete = false;
};

/*********************************** ↓↓↓ Todo: Implement by you ↓↓↓ ******************************************/
// Register Result Status
struct RegisterResultStatus
{
	string ReservationStationName;
	bool dataReady;
};

class RegisterResultStatuses
{
public:
	void initRegisterResultStatuses(int numFPReg)
	{
		_registers.resize(numFPReg);
		for (int i = 0; i < _registers.size(); i++)
		{
			_registers[i].dataReady = false;
			_registers[i].ReservationStationName = "";
		}
	}

	void updateRegisterStatus(const std::string &destRegister, const std::string &reservationStationName)
	{
		// Extracting the numeric part from destRegister
		std::string numericPart = destRegister.substr(1); // Assuming the register names start with 'F'
		// Convert the numeric part to an integer for comparison
		int destRegisterNumber = std::stoi(numericPart);

		for (int i = 0; i < _registers.size(); ++i)
		{
			if (destRegisterNumber == i)
			{
				// Update the destination register status with the reservation station name
				_registers[i].ReservationStationName = reservationStationName;
				_registers[i].dataReady = false;

				break; // Once found, exit the loop
			}
		}
	}

	string _printRegisterResultStatus() const
	{
		std::ostringstream result;
		for (int idx = 0; idx < _registers.size(); idx++)
		{
			result << "F" + std::to_string(idx) << ": ";
			result << _registers[idx].ReservationStationName << ", ";
			result << "dataRdy: " << (_registers[idx].dataReady ? "Y" : "N") << ", ";
			result << "\n";
		}
		return result.str();
	}

	bool isDataReady(const std::string &registerName) const
	{
		// Check if Data is Ready in a particular Floating Point Register
		int registerNumber = extractNumericPart(registerName);
		if (registerNumber >= 0 && registerNumber < _registers.size())
		{
			return _registers[registerNumber].dataReady;
		}
		return false; // Invalid register name or number
	}

	std::string getReservationStationName(const std::string &registerName) const
	{
		// Decode Operand, obtained the RS name corresponding to the Given Floating Point Regiser
		int registerNumber = extractNumericPart(registerName);
		if (registerNumber >= 0 && registerNumber < _registers.size())
		{
			return _registers[registerNumber].ReservationStationName;
		}
		return ""; // Invalid register name or number
	}

	void updateRRS(string RSName)
	{
		// Update the Register with the input RS name with data ready  =  true
		for (auto &r : _registers)
		{

			if (r.ReservationStationName == RSName)
			{
				r.dataReady = true;
			}
		}
	}

	string getRegisterWithOutput(string stationName)
	{
		// Returns the Name of the register corresponding to the stationname with data Ready  = True
		for (int i = 0; i < _registers.size(); i++)
		{
			if (_registers[i].ReservationStationName == stationName && _registers[i].dataReady == true)
			{
				return "F" + std::to_string(i);
			}
		}
	}

private:
	vector<RegisterResultStatus> _registers;
};

// Reservation Station
struct ReservationStation
{
	string name;
	bool busy;
	string Vj;
	string Vk;
	string Qj;
	string Qk;
	int remainCycle;
	int op;
	string dest;
	int issueCycle;
	int executeCycle;
	int writeCycle;
	int instructionIndex;
};

class ReservationStations
{
private:
	std::vector<ReservationStation> stations;

public:
	ReservationStations(int size, const std::string &name)
	{
		stations.resize(size);
		for (int i = 0; i < size; ++i)
		{
			stations[i].name = name + std::to_string(i); // Naming each station uniquely
			// Initialize other fields as needed
			stations[i].busy = false;
			stations[i].Vj = "";
			stations[i].Vk = "";
			stations[i].Qj = "";
			stations[i].Qk = "";
			stations[i].remainCycle = 999;
			stations[i].op = -1;
			stations[i].issueCycle = -1;
			stations[i].executeCycle = -1;
			stations[i].writeCycle = -1;
			stations[i].instructionIndex = -1;
		}
	}

	ReservationStation *findAvailableStation()
	{
		// Look for RS with Busy = 0
		for (auto &station : stations)
		{
			if (!station.busy)
			{
				return &station;
			}
		}
		return nullptr; // No available station found
	}

	void update(string RSName)
	{
		// If any of the Reservation Station is waiting for another RS output (Qj/Qk), set the operands as ready, Qj->Vj or Qk->Vk
		for (auto &station : stations)
		{
			if (RSName == "")
			{
				continue;
			}
			else
			{
				if (station.Qj == RSName)
				{
					station.Vj = "R(" + RSName + ")";
					station.Qj = "";
				}
				if (station.Qk == RSName)
				{
					station.Vk = "R(" + RSName + ")";
					station.Qk = "";
				}
			}
		}
	}

	void updateRemainingCycles(RegisterResultStatuses &RRS, vector<InstructionStatus> &instructionStatuses, int currentCycle)
	{
		for (auto &station : stations)
		{
			if (station.busy)
			{
				if (!station.Vj.empty() && !station.Vk.empty() && station.Qj.empty() && station.Qk.empty())
				{
					// Both Vj and Vk are available, decrement remaining cycle by 1
					station.remainCycle--;

					if (station.remainCycle == 0)
					{
						// Execution of the Instruction in RS Complete in this Cycle
						station.executeCycle = currentCycle;
						instructionStatuses[station.instructionIndex].cycleExecuted = station.executeCycle;
						instructionStatuses[station.instructionIndex].isComplete = false;
					}

					if (station.remainCycle == -1)
					{
						// BroadCast Output of RS to CDB for Writeback
						cdb.broadcast(station.name, station.issueCycle, station.op);
					}
				}
			}
		}
	}

	void completeWriteBack(string RSName, vector<InstructionStatus> &instructionStatuses, int currentCycle)
	{
		for (auto &station : stations)
		{
			if (station.name == RSName)
			{
				// Write Back is Complete, set Busy = False for RS and reset Vj Vk ; Update the WritebackCycle Value in Instruction Status
				station.busy = false;
				station.Vj = "";
				station.Vk = "";
				station.writeCycle = currentCycle;
				instructionStatuses[station.instructionIndex].cycleWriteResult = station.writeCycle;
				instructionStatuses[station.instructionIndex].isComplete = true;
			}
		}
	}

	void printStations() const
	{

		for (const auto &station : stations)
		{
			std::cout << std::left << std::setw(20) << ("Name: " + station.name) << std::setw(10)
					  << ("Busy: " + std::to_string(station.busy)) << std::setw(10) << ("Vj: " + station.Vj)
					  << std::setw(10) << ("Vk: " + station.Vk) << std::setw(10) << ("Qj: " + station.Qj)
					  << std::setw(10) << ("Qk: " + station.Qk) << std::setw(15) << ("RemainCycle: " + std::to_string(station.remainCycle))
					  << std::setw(15) << ("\tIssueCycle: " + std::to_string(station.issueCycle))
					  << std::setw(15) << ("\tExecuteCycle: " + std::to_string(station.executeCycle))
					  << std::setw(15) << ("\tWriteCycle " + std::to_string(station.writeCycle))
					  << std::setw(15) << ("\tInstructionIndex " + std::to_string(station.instructionIndex)) << endl;
		}
	}
};

class TomasuloAlgorithm
{
public:
	RegisterResultStatuses RRS;
	std::vector<InstructionStatus> instructionStatuses;
	int instrSize;

public:
	TomasuloAlgorithm(const HardwareConfig &config, int instructionSize)
		: loadRS(config.LoadRSsize, "Load"),
		  storeRS(config.StoreRSsize, "Store"),
		  addRS(config.AddRSsize, "Add"),
		  multRS(config.MultRSsize, "Mult"),
		  instructionStatuses()

	{
		RRS.initRegisterResultStatuses(config.FRegSize);
		instructionStatuses.resize(instructionSize);
		instrSize = instructionSize;
	}

	bool issueInstructions(const Instruction &instr, int currentCycle, int instrCycle)
	{
		// cout << "Current Cycle ->" << currentCycle << endl;
		// cout << "OP: " << instr.op << " " << instr.destRegister << " " << instr.srcReg1 << " " << instr.srcReg2 << endl;
		ReservationStation *selectedRS = nullptr;
		switch (instr.op)
		{
		case Operation::ADD:
			selectedRS = addRS.findAvailableStation();
			if (selectedRS)
			{
				selectedRS->remainCycle = OperationCycle[ADD];
				selectedRS->op = ADD;
			}
			break;

		case Operation::SUB:
			selectedRS = addRS.findAvailableStation();
			if (selectedRS)
			{
				selectedRS->remainCycle = OperationCycle[SUB];
				selectedRS->op = SUB;
			}
			break;

		case Operation::MULT:
			selectedRS = multRS.findAvailableStation();
			if (selectedRS)
			{
				selectedRS->remainCycle = OperationCycle[MULT];
				selectedRS->op = MULT;
			}
			break;

		case Operation::DIV:
			selectedRS = multRS.findAvailableStation();
			if (selectedRS)
			{
				selectedRS->remainCycle = OperationCycle[DIV];
				selectedRS->op = DIV;
			}
			break;

		case Operation::LOAD:
			selectedRS = loadRS.findAvailableStation();
			if (selectedRS)
			{
				selectedRS->remainCycle = OperationCycle[LOAD];
				selectedRS->op = LOAD;
			}
			break;

		case Operation::STORE:
			selectedRS = storeRS.findAvailableStation();
			if (selectedRS)
			{
				selectedRS->remainCycle = OperationCycle[STORE];
				selectedRS->op = STORE;
			}
			break;

			// Add cases for other operation types as needed
		}
		if (selectedRS)
		{
			// if (instrCycle <= instructionStatuses.size())
			//{
			selectedRS->issueCycle = currentCycle;
			if (instrCycle >= instrSize)
			{
				selectedRS->instructionIndex = instrSize - 1;
			}
			else
			{
				selectedRS->instructionIndex = instrCycle;
			}

			instructionStatuses[instrCycle].cycleIssued = currentCycle;
			instructionStatuses[instrCycle].isComplete = false;
			//}
			// Update the issueCycle in instructionStatuses

			// if RS is of LOAD or STORE, update VK Vj accordingly
			bool allOperandsAvailable = true;
			selectedRS->busy = true;

			if (instr.op == LOAD)
			{
				selectedRS->Vj = instr.srcReg1;
				selectedRS->Vk = instr.srcReg2;
				selectedRS->op = (instr.op == LOAD) ? 4 : 5;
				selectedRS->remainCycle = (instr.op == LOAD) ? OperationCycle[LOAD] : OperationCycle[STORE];
				selectedRS->busy = true;
				selectedRS->dest = instr.destRegister;
			}
			else
			{

				// Check if both source registers are available
				if (instr.op == STORE)
				{
					if (!RRS.isDataReady(instr.destRegister) && instr.destRegister[0] == 'F')
					{
						selectedRS->Qj = RRS.getReservationStationName(instr.destRegister);
						selectedRS->Vj = "";
						selectedRS->Vk = "";
					}
					else
					{
						selectedRS->Vj = instr.srcReg1;
						selectedRS->Vk = instr.srcReg2;
					}
				}

				if (RRS.isDataReady(instr.srcReg1) == false && instr.srcReg1[0] == 'F')
				{
					// Source register 1 is not ready
					selectedRS->Qj = RRS.getReservationStationName(instr.srcReg1);
					allOperandsAvailable = false;
				}
				else
				{
					selectedRS->Vj = instr.srcReg1; // Source register 1 is available
				}

				if (RRS.isDataReady(instr.srcReg2) == false && instr.srcReg2[0] == 'F')
				{
					// Source register 2 is not ready
					selectedRS->Qk = RRS.getReservationStationName(instr.srcReg2);
					allOperandsAvailable = false;
				}
				else
				{
					selectedRS->Vk = instr.srcReg2; // Source register 2 is available
				}
			}

			if (!selectedRS->Vj.empty() && !selectedRS->Vk.empty() && selectedRS->Qj.empty() && selectedRS->Qk.empty())
			{
				// Both operands are available, issue the instruction
				selectedRS->busy = true;
				selectedRS->op = instr.op;
				selectedRS->dest = instr.destRegister;
			}
			if (instr.op != STORE)
			{
				RRS.updateRegisterStatus(instr.destRegister, selectedRS->name);
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	void executeInstruction(int currentCycle)
	{

		loadRS.updateRemainingCycles(RRS, instructionStatuses, currentCycle);
		storeRS.updateRemainingCycles(RRS, instructionStatuses, currentCycle);
		addRS.updateRemainingCycles(RRS, instructionStatuses, currentCycle);
		multRS.updateRemainingCycles(RRS, instructionStatuses, currentCycle);

		CDBMessage message("", 1, -1);
		message = cdb.getLastMessage();
		int op = message.op;
		// cout << "ALERT: Station to be updated in RS and RRS: " << message.stationName << " Op: " << op << endl;

		addRS.update(message.stationName);
		multRS.update(message.stationName);
		loadRS.update(message.stationName);
		storeRS.update(message.stationName);

		switch (message.op)
		{
		case ADD:
			cout << "Update Step) Updating CDB RS: " << message.stationName << endl;

			RRS.updateRRS(message.stationName);

			break;
		case SUB:
			RRS.updateRRS(message.stationName);
			break;
		case MULT:
			RRS.updateRRS(message.stationName);
			break;
		case DIV:
			RRS.updateRRS(message.stationName);
			multRS.completeWriteBack(message.stationName, instructionStatuses, currentCycle);
			break;
		case LOAD:
			RRS.updateRRS(message.stationName);
			break;
		case STORE:
			RRS.updateRRS(message.stationName);
			break;
		}

		addRS.completeWriteBack(message.stationName, instructionStatuses, currentCycle);
		multRS.completeWriteBack(message.stationName, instructionStatuses, currentCycle);
		loadRS.completeWriteBack(message.stationName, instructionStatuses, currentCycle);
		storeRS.completeWriteBack(message.stationName, instructionStatuses, currentCycle);

		cdb.RemoveSmallest();
	}

	RegisterResultStatuses getFinalRegisterResultStatuses()
	{
		return RRS;
	}

	void printAllStations() const
	{
		std::cout << "Load Reservation Stations:" << std::endl;
		loadRS.printStations();

		std::cout << "\nStore Reservation Stations:" << std::endl;
		storeRS.printStations();

		std::cout << "\nAdd Reservation Stations:" << std::endl;
		addRS.printStations();

		std::cout << "\nMultiply Reservation Stations:" << std::endl;
		multRS.printStations();
	}

	std::vector<InstructionStatus> getInstructionStatuses() const
	{
		return instructionStatuses;
	}

	bool getExecutionStatus()
	{
		// Return True after a particular instruction RS has written back to RRS
		bool executionComplete = true;

		for (auto &st : instructionStatuses)
		{
			if (st.isComplete == false)
			{
				bool executionComplete = false;
				return executionComplete;
			}
		}
		cout << endl;
		return executionComplete;
	}

private:
	ReservationStations loadRS, storeRS, addRS, multRS;
};

/*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/

/*
print the instruction status, the reservation stations and the register result status
@param filename: output file name
@param instructionStatus: instruction status
*/
void PrintResult4Grade(const string &filename, const vector<InstructionStatus> &instructionStatus)
{
	std::ofstream outfile(filename, std::ios_base::app); // append result to the end of file
	outfile << "Instruction Status:\n";
	for (int idx = 0; idx < instructionStatus.size(); idx++)
	{
		outfile << "Instr" << idx << ": ";
		outfile << "Issued: " << instructionStatus[idx].cycleIssued << ", ";
		outfile << "Completed: " << instructionStatus[idx].cycleExecuted << ", ";
		outfile << "Write Result: " << instructionStatus[idx].cycleWriteResult << ", ";
		outfile << "\n";
	}
	outfile.close();
}

/*
print the register result status each 5 cycles
@param filename: output file name
@param registerResultStatus: register result status
@param thiscycle: current cycle
*/
void PrintRegisterResultStatus4Grade(const string &filename,
									 const RegisterResultStatuses &registerResultStatus,
									 const int thiscycle)
{
	if (thiscycle % 5 != 0)
		return;
	std::ofstream outfile(filename, std::ios_base::app); // append result to the end of file
	outfile << "Cycle " << thiscycle << ":\n";
	outfile << registerResultStatus._printRegisterResultStatus() << "\n";
	outfile.close();
}

int main(int argc, char **argv)
{
	if (argc > 1)
	{
		hardwareconfigname = argv[1];
		inputtracename = argv[2];
	}

	HardwareConfig hardwareConfig;
	std::ifstream config;
	config.open(hardwareconfigname);
	config >> hardwareConfig.LoadRSsize;  // number of load reservation stations
	config >> hardwareConfig.StoreRSsize; // number of store reservation stations
	config >> hardwareConfig.AddRSsize;	  // number of add reservation stations
	config >> hardwareConfig.MultRSsize;  // number of multiply reservation stations
	config >> hardwareConfig.FRegSize;	  // number of fp registers
	config.close();

	// Initialize RSS, Instruction Status Vector and Instruction Queue
	vector<Instruction> instructions = readInstruction("trace.txt");
	RegisterResultStatuses RSS;
	vector<InstructionStatus> instructionStatus;

	// Initialize Algorithm
	TomasuloAlgorithm algo(hardwareConfig, instructions.size());

	int cycleCount = 1;
	bool instructionExecuted = false;
	int instructionIndex = 0;
	while (cycleCount < 100000)
	{
		// cout << "+++++++++++++++++++++++++|| CYCLE : " << cycleCount << " ||+++++++++++++++++++++++++++++++++++++++++++" << endl;

		algo.executeInstruction(cycleCount);

		// Check if there are more instructions to issue or process

		// cout << "Current Cycle in Main Loop: " << cycleCount - 1 << endl;
		// cout << "Fetching Instruction Number: " << instructionIndex << endl;
		if (instructionIndex < instructions.size())
		{

			instructionExecuted = algo.issueInstructions(instructions[instructionIndex], cycleCount, instructionIndex);
		}

		if (instructionExecuted)
		{
			++instructionIndex;

			instructionExecuted = false;
		}

		// algo.printAllStations(); // To Debug;
		PrintRegisterResultStatus4Grade(outputtracename, algo.getFinalRegisterResultStatuses(), cycleCount);
		// cout << algo.getFinalRegisterResultStatuses()._printRegisterResultStatus() << endl; // Debug;

		//   Update cycle count
		++cycleCount;
		// cdb.printCDB();

		// check if all all instructions are executed
		if (algo.getExecutionStatus())
		{
			break;
		}
	}
	instructionStatus = algo.getInstructionStatuses();

	/*********************************** ↑↑↑ Todo: Implement by you ↑↑↑ ******************************************/

	// At the end of the program, print Instruction Status Table for grading
	PrintResult4Grade(outputtracename, instructionStatus);

	return 0;
}
