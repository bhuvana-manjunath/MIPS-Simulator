/*
--------------------------------------------------------
         Tomasulo's Algorithm Simulator - Core Setup
--------------------------------------------------------

This module sets up the data structures and utilities for simulating
Tomasulo’s Algorithm, supporting out-of-order execution via:

  - Reservation stations for ADD, MULT, LOAD, STORE units
  - Common Data Bus (CDB) to broadcast results
  - Register result status tracking
  - Instruction timing (issue, execute, write result)

Instructions are read from `trace.txt`, and hardware specs from `config.txt`.
Outputs will be saved in `trace.out.txt`.

Supported operations: ADD, SUB, MULT, DIV, LOAD, STORE
*/

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

// -----------------------------------
// Common Data Bus (CDB)
// -----------------------------------

struct CDBMessage {
	std::string stationName; // Source reservation station name
	int remainCycle;         // Issue cycle (used for prioritization)
	int op;                  // Operation type (ADD, MULT, etc.)

	CDBMessage(const std::string &name, int cycle, int opInt)
		: stationName(name), remainCycle(cycle), op(opInt) {}
};

class CommonDataBus {
private:
	std::vector<CDBMessage> messages;

public:
	// Broadcast a result on the CDB
	void broadcast(const std::string &stationName, int rcycle, int op) {
		messages.emplace_back(stationName, rcycle, op);
	}

	// Get the CDB message with the earliest issue cycle
	CDBMessage getLastMessage() {
		CDBMessage smallest("NONE", std::numeric_limits<int>::max(), -1);

		for (const auto &m : messages) {
			if (m.remainCycle < smallest.remainCycle) {
				smallest = m;
			}
		}
		return smallest;
	}

	// Print all current CDB messages (for debugging)
	void printCDB() {
		for (auto &c : messages) {
			cout << "|| Name: " << c.stationName << " Issue Cycle: "
				 << c.remainCycle << " Op: " << c.op << " ||    ";
		}
		cout << endl;
	}

	// Remove and return the CDB message with the earliest issue cycle
	CDBMessage RemoveSmallest() {
		if (messages.empty()) return CDBMessage("NONE", 1, -1);

		auto smallest = messages.begin();
		for (auto it = messages.begin() + 1; it != messages.end(); ++it) {
			if (it->remainCycle < smallest->remainCycle) {
				smallest = it;
			}
		}

		CDBMessage result = *smallest;
		messages.erase(smallest);
		return result;
	}
};

CommonDataBus cdb;  // Global CDB instance

// -----------------------------------
// Operation Types and Execution Latencies
// -----------------------------------

enum Operation { ADD, SUB, MULT, DIV, LOAD, STORE };

// Execution latency for each instruction type
const int OperationCycle[6] = {2, 2, 10, 40, 2, 2};

// -----------------------------------
// Hardware Configuration
// -----------------------------------

struct HardwareConfig {
	int LoadRSsize;
	int StoreRSsize;
	int AddRSsize;
	int MultRSsize;
	int FRegSize;
};

// -----------------------------------
// Instruction Format
// -----------------------------------

struct Instruction {
	Operation op;
	string destRegister;
	std::string srcReg1;
	std::string srcReg2;
};

// Read instructions from trace file
std::vector<Instruction> readInstruction(const std::string &filename) {
	std::ifstream file(filename);
	std::vector<Instruction> instructions;

	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			std::istringstream iss(line);
			std::string opStr, destReg, srcReg1, srcReg2;
			iss >> opStr >> destReg >> srcReg1 >> srcReg2;

			Operation op;
			if (opStr == "ADD") op = Operation::ADD;
			else if (opStr == "SUB") op = Operation::SUB;
			else if (opStr == "MULT") op = Operation::MULT;
			else if (opStr == "DIV") op = Operation::DIV;
			else if (opStr == "LOAD") op = Operation::LOAD;
			else if (opStr == "STORE") op = Operation::STORE;

			instructions.push_back({op, destReg, srcReg1, srcReg2});
		}
		file.close();
	} else {
		std::cout << "Unable to open file " << filename << std::endl;
	}
	return instructions;
}

// Extract numeric part from register name (e.g., F6 → 6)
int extractNumericPart(const std::string &str) {
	std::string numericPart;
	for (char ch : str) {
		if (std::isdigit(ch)) numericPart += ch;
	}
	return std::stoi(numericPart);
}

// -----------------------------------
// Instruction Status Tracker
// -----------------------------------

struct InstructionStatus {
	int cycleIssued;
	int cycleExecuted;       // When execution completes
	int cycleWriteResult;    // When result is broadcast on CDB
	bool isComplete = false;
};

// --------------------------------------------------------
// Register Result Status (RRS)
// --------------------------------------------------------
// Tracks the status of each floating-point register:
//   - Which reservation station (RS) is producing its value
//   - Whether the data is ready
// This module supports updates during instruction issue,
// CDB broadcasts, and periodic printing for debugging
// --------------------------------------------------------

struct RegisterResultStatus {
	string ReservationStationName; // Name of the RS writing to this register
	bool dataReady;                // True if data has been written and is ready
};

class RegisterResultStatuses {
public:
	// Initialize RRS table with `numFPReg` entries, one per floating-point register
	void initRegisterResultStatuses(int numFPReg) {
		_registers.resize(numFPReg);
		for (int i = 0; i < _registers.size(); i++) {
			_registers[i].ReservationStationName = "";
			_registers[i].dataReady = false;
		}
	}

	// Set a destination register as waiting on a reservation station (during issue)
	void updateRegisterStatus(const std::string &destRegister, const std::string &reservationStationName) {
		int destRegisterNumber = extractNumericPart(destRegister);
		if (destRegisterNumber >= 0 && destRegisterNumber < _registers.size()) {
			_registers[destRegisterNumber].ReservationStationName = reservationStationName;
			_registers[destRegisterNumber].dataReady = false;
		}
	}

	// Return a string representing the status of all FP registers
	string _printRegisterResultStatus() const {
		std::ostringstream result;
		for (int idx = 0; idx < _registers.size(); idx++) {
			result << "F" << idx << ": ";
			result << _registers[idx].ReservationStationName << ", ";
			result << "dataRdy: " << (_registers[idx].dataReady ? "Y" : "N") << ",\n";
		}
		return result.str();
	}

	// Check whether the given register has data ready
	bool isDataReady(const std::string &registerName) const {
		int registerNumber = extractNumericPart(registerName);
		if (registerNumber >= 0 && registerNumber < _registers.size()) {
			return _registers[registerNumber].dataReady;
		}
		return false;
	}

	// Get the RS name currently writing to the specified register
	std::string getReservationStationName(const std::string &registerName) const {
		int registerNumber = extractNumericPart(registerName);
		if (registerNumber >= 0 && registerNumber < _registers.size()) {
			return _registers[registerNumber].ReservationStationName;
		}
		return "";
	}

	// Mark all registers being written by a completed RS as dataReady = true
	void updateRRS(string RSName) {
		for (auto &reg : _registers) {
			if (reg.ReservationStationName == RSName) {
				reg.dataReady = true;
			}
		}
	}

	// Return the register name being written by the given RS (if data is ready)
	string getRegisterWithOutput(string stationName) {
		for (int i = 0; i < _registers.size(); i++) {
			if (_registers[i].ReservationStationName == stationName && _registers[i].dataReady == true) {
				return "F" + std::to_string(i);
			}
		}
		return "";
	}

private:
	vector<RegisterResultStatus> _registers; // Vector of per-register status entries
};

// --------------------------------------------------------
// Reservation Station (RS) & ReservationStations Manager
// --------------------------------------------------------
// Each RS represents a functional unit slot for ADD/SUB, MULT/DIV, LOAD, STORE
// - Tracks operand readiness (Vj/Vk or Qj/Qk)
// - Holds execution countdown (`remainCycle`)
// - Handles issue, execute, and writeback timing
// The ReservationStations class manages a group of these
// --------------------------------------------------------

// Reservation Station structure representing one slot
struct ReservationStation {
	string name;         // Station name (e.g., Add0, Load1)
	bool busy;           // True if this station is currently executing or waiting
	string Vj, Vk;       // Operand values (if available)
	string Qj, Qk;       // Reservation stations producing Vj/Vk (if not ready)
	int remainCycle;     // Remaining execution cycles
	int op;              // Operation type (enum)
	string dest;         // Destination register
	int issueCycle;      // Cycle in which instruction was issued
	int executeCycle;    // Cycle when execution completes
	int writeCycle;      // Cycle when result is written to CDB
	int instructionIndex;// Index of associated instruction
};

// ReservationStations manages a group of RS units for one operation type
class ReservationStations {
private:
	std::vector<ReservationStation> stations;

public:
	// Constructor initializes N RS entries with a given name prefix
	ReservationStations(int size, const std::string &name) {
		stations.resize(size);
		for (int i = 0; i < size; ++i) {
			stations[i].name = name + std::to_string(i);
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

	// Find the first available (non-busy) RS
	ReservationStation* findAvailableStation() {
		for (auto &station : stations) {
			if (!station.busy) return &station;
		}
		return nullptr;
	}

	// Update RS operands if a dependency has been resolved (CDB broadcast)
	void update(string RSName) {
		for (auto &station : stations) {
			if (station.Qj == RSName) {
				station.Vj = "R(" + RSName + ")";
				station.Qj = "";
			}
			if (station.Qk == RSName) {
				station.Vk = "R(" + RSName + ")";
				station.Qk = "";
			}
		}
	}

	// Decrement timers and handle CDB broadcast triggers
	void updateRemainingCycles(RegisterResultStatuses &RRS, vector<InstructionStatus> &instructionStatuses, int currentCycle) {
		for (auto &station : stations) {
			if (station.busy && station.Qj.empty() && station.Qk.empty() && !station.Vj.empty() && !station.Vk.empty()) {
				station.remainCycle--;

				if (station.remainCycle == 0) {
					station.executeCycle = currentCycle;
					instructionStatuses[station.instructionIndex].cycleExecuted = currentCycle;
					instructionStatuses[station.instructionIndex].isComplete = false;
				}

				if (station.remainCycle == -1) {
					// Push to CDB to trigger writeback in the next cycle
					cdb.broadcast(station.name, station.issueCycle, station.op);
				}
			}
		}
	}

	// Free a station after its result is written back
	void completeWriteBack(string RSName, vector<InstructionStatus> &instructionStatuses, int currentCycle) {
		for (auto &station : stations) {
			if (station.name == RSName) {
				station.busy = false;
				station.Vj = "";
				station.Vk = "";
				station.writeCycle = currentCycle;
				instructionStatuses[station.instructionIndex].cycleWriteResult = currentCycle;
				instructionStatuses[station.instructionIndex].isComplete = true;
			}
		}
	}

	// Print current state of all RS entries (for debugging/logging)
	void printStations() const {
		for (const auto &station : stations) {
			std::cout << std::left
					  << std::setw(20) << ("Name: " + station.name)
					  << std::setw(10) << ("Busy: " + std::to_string(station.busy))
					  << std::setw(10) << ("Vj: " + station.Vj)
					  << std::setw(10) << ("Vk: " + station.Vk)
					  << std::setw(10) << ("Qj: " + station.Qj)
					  << std::setw(10) << ("Qk: " + station.Qk)
					  << std::setw(15) << ("RemainCycle: " + std::to_string(station.remainCycle))
					  << std::setw(15) << ("IssueCycle: " + std::to_string(station.issueCycle))
					  << std::setw(15) << ("ExecuteCycle: " + std::to_string(station.executeCycle))
					  << std::setw(15) << ("WriteCycle: " + std::to_string(station.writeCycle))
					  << std::setw(15) << ("InstrIndex: " + std::to_string(station.instructionIndex))
					  << endl;
		}
	}
};

// ---------------------------------------------------------------------
// TomasuloAlgorithm Class
// ---------------------------------------------------------------------
// Simulates instruction issue, execution, and writeback using Tomasulo’s algorithm.
// Handles dependencies, register result status, reservation station status,
// and CDB communication for all instruction types (ADD, SUB, MULT, DIV, LOAD, STORE).
// ---------------------------------------------------------------------
class TomasuloAlgorithm
{
public:
	RegisterResultStatuses RRS;                  // Tracks destination registers and RS dependencies
	std::vector<InstructionStatus> instructionStatuses;  // Records issue, execute, write cycles
	int instrSize;                               // Number of instructions in trace

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

	// ---------------------------------------------------------------------
	// Issue Logic
	// ---------------------------------------------------------------------
	// Finds an available RS and issues the instruction into it if possible.
	// Sets operands or marks pending dependencies via Qj/Qk.
	// Updates Register Result Status (RRS) for the destination register.
	// ---------------------------------------------------------------------
	bool issueInstructions(const Instruction &instr, int currentCycle, int instrCycle)
	{
		ReservationStation *selectedRS = nullptr;

		// Choose the appropriate RS pool based on operation
		switch (instr.op)
		{
		case Operation::ADD:
		case Operation::SUB:
			selectedRS = addRS.findAvailableStation();
			break;
		case Operation::MULT:
		case Operation::DIV:
			selectedRS = multRS.findAvailableStation();
			break;
		case Operation::LOAD:
			selectedRS = loadRS.findAvailableStation();
			break;
		case Operation::STORE:
			selectedRS = storeRS.findAvailableStation();
			break;
		}

		// If an RS is available, set up and issue instruction
		if (selectedRS)
		{
			selectedRS->remainCycle = OperationCycle[instr.op];
			selectedRS->op = instr.op;
			selectedRS->issueCycle = currentCycle;
			selectedRS->instructionIndex = std::min(instrCycle, instrSize - 1);
			instructionStatuses[instrCycle].cycleIssued = currentCycle;
			instructionStatuses[instrCycle].isComplete = false;
			selectedRS->busy = true;

			// LOAD case: address-based
			if (instr.op == LOAD)
			{
				selectedRS->Vj = instr.srcReg1;
				selectedRS->Vk = instr.srcReg2;
				selectedRS->dest = instr.destRegister;
			}
			// STORE and other register-based ops
			else
			{
				if (instr.op == STORE && !RRS.isDataReady(instr.destRegister) && instr.destRegister[0] == 'F')
				{
					selectedRS->Qj = RRS.getReservationStationName(instr.destRegister);
				}
				else if (instr.op == STORE)
				{
					selectedRS->Vj = instr.srcReg1;
					selectedRS->Vk = instr.srcReg2;
				}

				if (!RRS.isDataReady(instr.srcReg1) && instr.srcReg1[0] == 'F')
					selectedRS->Qj = RRS.getReservationStationName(instr.srcReg1);
				else
					selectedRS->Vj = instr.srcReg1;

				if (!RRS.isDataReady(instr.srcReg2) && instr.srcReg2[0] == 'F')
					selectedRS->Qk = RRS.getReservationStationName(instr.srcReg2);
				else
					selectedRS->Vk = instr.srcReg2;
			}

			if (instr.op != STORE)
				RRS.updateRegisterStatus(instr.destRegister, selectedRS->name);

			return true;
		}

		return false; // RS unavailable
	}

	// ---------------------------------------------------------------------
	// Execute & Writeback
	// ---------------------------------------------------------------------
	// Steps:
	//   1. Advance timers for executable instructions
	//   2. Update RS operands if their producer RS just broadcasted
	//   3. Writeback to register file and release RS
	// ---------------------------------------------------------------------
	void executeInstruction(int currentCycle)
	{
		// Countdown for all RS banks
		loadRS.updateRemainingCycles(RRS, instructionStatuses, currentCycle);
		storeRS.updateRemainingCycles(RRS, instructionStatuses, currentCycle);
		addRS.updateRemainingCycles(RRS, instructionStatuses, currentCycle);
		multRS.updateRemainingCycles(RRS, instructionStatuses, currentCycle);

		// Get completed instruction on CDB and update consumers
		CDBMessage message = cdb.getLastMessage();
		addRS.update(message.stationName);
		multRS.update(message.stationName);
		loadRS.update(message.stationName);
		storeRS.update(message.stationName);

		// Broadcast result to register file
		if (message.op != -1)
			RRS.updateRRS(message.stationName);

		// Mark instruction as written back and release RS
		addRS.completeWriteBack(message.stationName, instructionStatuses, currentCycle);
		multRS.completeWriteBack(message.stationName, instructionStatuses, currentCycle);
		loadRS.completeWriteBack(message.stationName, instructionStatuses, currentCycle);
		storeRS.completeWriteBack(message.stationName, instructionStatuses, currentCycle);

		// Remove the message that was processed
		cdb.RemoveSmallest();
	}

	// Returns final register result status
	RegisterResultStatuses getFinalRegisterResultStatuses() { return RRS; }

	// Pretty print all RS states
	void printAllStations() const
	{
		std::cout << "Load Reservation Stations:\n";
		loadRS.printStations();
		std::cout << "\nStore Reservation Stations:\n";
		storeRS.printStations();
		std::cout << "\nAdd Reservation Stations:\n";
		addRS.printStations();
		std::cout << "\nMultiply Reservation Stations:\n";
		multRS.printStations();
	}

	// Returns status of all instructions
	std::vector<InstructionStatus> getInstructionStatuses() const { return instructionStatuses; }

	// True when all instructions have written their results
	bool getExecutionStatus()
	{
		for (auto &st : instructionStatuses)
		{
			if (!st.isComplete)
				return false;
		}
		return true;
	}

private:
	// Reservation Station pools
	ReservationStations loadRS, storeRS, addRS, multRS;
};

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
	// Parse command-line arguments for config and trace file names
	if (argc > 1)
	{
		hardwareconfigname = argv[1];
		inputtracename = argv[2];
	}

	// Load hardware configuration from file
	HardwareConfig hardwareConfig;
	std::ifstream config;
	config.open(hardwareconfigname);
	config >> hardwareConfig.LoadRSsize;   // Number of Load RS entries
	config >> hardwareConfig.StoreRSsize;  // Number of Store RS entries
	config >> hardwareConfig.AddRSsize;    // Number of Add RS entries
	config >> hardwareConfig.MultRSsize;   // Number of Mult RS entries
	config >> hardwareConfig.FRegSize;     // Number of FP registers
	config.close();

	// Read instruction trace from input file
	vector<Instruction> instructions = readInstruction("trace.txt");

	// Initialize register result status and instruction status tracker
	RegisterResultStatuses RSS;
	vector<InstructionStatus> instructionStatus;

	// Initialize Tomasulo simulator
	TomasuloAlgorithm algo(hardwareConfig, instructions.size());

	int cycleCount = 1;
	bool instructionExecuted = false;
	int instructionIndex = 0;

	// Main simulation loop
	while (cycleCount < 100000)
	{
		// Step 1: Execute stage – check all RS and update remaining cycles
		algo.executeInstruction(cycleCount);

		// Step 2: Issue instruction if available and RS has space
		if (instructionIndex < instructions.size())
		{
			instructionExecuted = algo.issueInstructions(instructions[instructionIndex], cycleCount, instructionIndex);
		}

		// Step 3: If issued successfully, move to next instruction
		if (instructionExecuted)
		{
			++instructionIndex;
			instructionExecuted = false;
		}

		// Step 4: Log register result status every 5 cycles
		PrintRegisterResultStatus4Grade(outputtracename, algo.getFinalRegisterResultStatuses(), cycleCount);

		// Step 5: Increment global cycle count
		++cycleCount;

		// Step 6: Exit loop when all instructions have completed
		if (algo.getExecutionStatus())
		{
			break;
		}
	}

	// Fetch final instruction status timeline
	instructionStatus = algo.getInstructionStatuses();

	// Print final instruction status to output file
	PrintResult4Grade(outputtracename, instructionStatus);

	return 0;
}