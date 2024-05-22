#include <fstream>
#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <algorithm>
#include "tomasulo.hpp"

// Vector to store instructions
std::vector<inst_t> inst_list;
// Deque for instruction pointers
std::deque<inst_t *> insts;
// Vector for reorder buffer pointers
std::vector<inst_t *> reorder_buffer;
// Clock ticks counter
unsigned int ticks = 0;

// Operation strings
const std::string str_op[6] = {"add", "mul", "lw", "sub", "div", "sw"};

// Register names
const std::string str_reg[REGISTERS_MAX + 1] = {
    "-", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11",
    "ra", "rb", "rc", "rd", "re", "rf", "rg", "rh", "ri", "rj", "rk", "rl", "rm",
    "rn", "ro", "rp", "rq", "rr", "rs", "rt", "ru", "rv", "rw", "rx"};

// Functional unit names
const std::string str_fus[ALL_STATIONS + 1] = {
    "-", "add1", "add2", "mult1", "mult2", "load1", "load2"};

// Register status array initialization
regstat_t registers[REGISTERS_MAX + 1] = {
    {-1, free_reg, -1, noreg, noreg, 0}};

// Add station initialization
fu_t add_stations[ADD_STATIONS] = {
    {0, false, nullptr, 0, 0, -1, -1, -1, 0, 0}};

// Multiply station initialization
fu_t mult_stations[MUL_STATIONS] = {
    {0, false, nullptr, 0, 0, -1, -1, -1, 0, 0}};

// Load station initialization
fu_t load_stations[LOAD_STATIONS] = {
    {0, false, nullptr, 0, 0, -1, -1, -1, 0, 0}};

// Array of station sizes
int station_sizes[STATION_TYPES] = {ADD_STATIONS, MUL_STATIONS, LOAD_STATIONS};

// Array of all functional units
fu_t *all_stations[STATION_TYPES] = {add_stations, mult_stations, load_stations};

// Next Reorder
static unsigned next_reorder = 0;

void init_fus()
{
    // Initialize the ID counter
    int next_id = 0;

    // Iterate over all types of functional units
    for (int i = 0; i < STATION_TYPES; i++)
    {
        // Iterate over each station within the current functional unit type
        for (int j = 0; j < station_sizes[i]; j++)
        {
            // Set the ID for the current functional unit
            all_stations[i][j].id = next_id++;

            // Mark the station as not busy
            all_stations[i][j].busy = false;

            // Initialize the instruction pointer to null
            all_stations[i][j].inst = nullptr;

            // Initialize operand values and tags
            all_stations[i][j].vj = 0;
            all_stations[i][j].vk = 0;
            all_stations[i][j].qj = -1;
            all_stations[i][j].qk = -1;

            // Initialize the time left for execution to -1 (indicating not in use)
            all_stations[i][j].time_left = -1;

            // Initialize the lock flags
            all_stations[i][j].locks1 = false;
            all_stations[i][j].locks2 = false;
        }
    }
}

void rename(inst_t *i)
{
    // Check if the destination register is in use (i.e., false dependencies exist)
    if (registers[i->dest].used_as != free_reg)
    {
        reg_t rename = noreg;
        // Find a free register to use for renaming
        for (int j = VISIBLE_REGISTERS + 1; j <= REGISTERS_MAX; j++)
        {
            if (registers[j].used_as == free_reg)
            {
                rename = (reg_t)j;
                registers[rename].used_as = dest; // Mark as destination
                break;
            }
        }
        // Update rename mappings
        registers[i->dest].renamed_to = rename;
        registers[rename].renamed_from = i->dest;
        i->dest = rename;
        registers[i->dest].rename_ref_count++;
    }

    // Check if the first source register needs renaming
    if (registers[i->src1].used_as == src)
    {
        reg_t rename = noreg;
        // Find a free register to use for renaming
        for (int j = VISIBLE_REGISTERS + 1; j <= REGISTERS_MAX; j++)
        {
            if (registers[j].used_as == free_reg)
            {
                rename = (reg_t)j;
                registers[rename].used_as = src; // Mark as source
                break;
            }
        }
        // Update rename mappings
        registers[i->src1].renamed_to = rename;
        registers[rename].renamed_from = i->src1;
        i->src1 = rename;

        // If the source register is not used as a destination by another instruction
        if (registers[i->src1].dest_used_by == -1)
        {
            registers[i->src1].used_as = src;
        }
        registers[i->src1].rename_ref_count++;
    }

    // Check if the second source register needs renaming
    if (registers[i->src2].used_as == src)
    {
        reg_t rename = noreg;
        // Find a free register to use for renaming
        for (int j = VISIBLE_REGISTERS + 1; j <= REGISTERS_MAX; j++)
        {
            if (registers[j].used_as == free_reg)
            {
                rename = (reg_t)j;
                registers[rename].used_as = src; // Mark as source
                break;
            }
        }
        // Update rename mappings
        registers[i->src2].renamed_to = rename;
        registers[rename].renamed_from = i->src2;
        i->src2 = rename;

        // If the source register is not used as a destination by another instruction
        if (registers[i->src2].dest_used_by == -1)
        {
            registers[i->src2].used_as = src;
        }
        registers[i->src2].rename_ref_count++;
    }
}

void issue()
{
    // Check if there are instructions to issue
    if (insts.empty())
    {
        return;
    }
    // Get the instruction from the front of the instruction queue
    inst_t *i = insts.front();

    // Check if registers must be renamed before issuing the instruction
    rename(i);

    // Determine the appropriate functional unit type based on the operation
    fu_t *stations = all_stations[i->op % STATION_TYPES];

    // Find an empty station within the selected functional unit type
    int empty_station = -1;
    for (int j = 0; j < station_sizes[i->op % STATION_TYPES]; j++)
    {
        if (!stations[j].busy)
        {
            empty_station = j;
            break;
        }
    }

    // Issue the instruction to an available station if one is found
    if (empty_station != -1)
    {
        stations[empty_station].inst = i; // Assign the instruction to the station
        i->issue = ticks;                 // Record the issue time
        insts.pop_front();                // Remove the instruction from the queue
    }
}

void undo_rename(reg_t reg)
{
    // Decrease the reference count for rename
    if (registers[reg].rename_ref_count > 0)
    {
        registers[reg].rename_ref_count--;
    }

    // Check if the register can be freed
    if (registers[reg].rename_ref_count == 0 && registers[reg].used_as != dest)
    {
        // Reset the register to its original state
        registers[reg].used_as = free_reg;
        registers[registers[reg].renamed_from].renamed_to = noreg;
        registers[reg].renamed_from = noreg;
        registers[reg].value = 0;
        registers[reg].dest_used_by = -1;
    }
    else if (registers[reg].used_as == dest)
    {
        // If the register is still used as a destination, clear its reference
        registers[reg].dest_used_by = -1;
    }

    // Update the value of the register the original register was renamed from
    registers[registers[reg].renamed_from].value = registers[reg].value;
}

void exec_fu(fu_t *fu)
{
    // Macro to check if a register is used as destination by another instruction and not by the current functional unit
#define USED_AS_DEST(reg) (registers[reg].used_as == dest && registers[reg].dest_used_by != -1 && registers[reg].dest_used_by != fu->id)

    if (fu->busy)
    {
        // If the functional unit is busy:

        // Handle true dependencies for source operands
        if (fu->locks1 && USED_AS_DEST(fu->inst->src1))
        {
            // If there's a true dependency on source operand 1, set the corresponding reservation station
            fu->qj = registers[fu->inst->src1].dest_used_by;
            return; // Stop further execution
        }
        else if (fu->locks1)
        {
            // If no true dependency on source operand 1, update the reservation station and release the lock
            fu->qj = -1;
            fu->vj = fu->inst->src1;
            fu->locks1 = false;
        }

        if (fu->locks2 && USED_AS_DEST(fu->inst->src2))
        {
            // If there's a true dependency on source operand 2, set the corresponding reservation station
            fu->qk = registers[fu->inst->src2].dest_used_by;
            return; // Stop further execution
        }
        else if (fu->locks2)
        {
            // If no true dependency on source operand 2, update the reservation station and release the lock
            fu->qk = -1;
            fu->vk = fu->inst->src2;
            fu->locks2 = false;
        }

        // Decrement time left for execution
        fu->time_left--;

        // Check if execution is finished
        if (fu->time_left == 1)
        {
            // If execution is finished, mark the execution time
            fu->inst->exec = ticks;
        }
        else if (fu->time_left == 0)
        {
            // If the execution is completed:

            // Mark the write time
            fu->inst->write = ticks;

            // Write the result to the destination register and undo renaming
            registers[fu->inst->dest].value = fu->id + 1;
            undo_rename(fu->inst->dest);
            undo_rename(fu->inst->src1);
            undo_rename(fu->inst->src2);

            // Place the instruction to the reorder buffer
            reorder_buffer.push_back(fu->inst);

            // Reset functional unit state
            fu->busy = false;
            fu->inst = nullptr;
            fu->time_left = -1;
            fu->vj = 0;
            fu->vk = 0;
            fu->qj = -1;
            fu->qk = -1;
            fu->locks1 = false;
            fu->locks2 = false;
        }
    }
    else
    {
        // If the functional unit is not busy:

        if (fu->inst == nullptr)
        {
            // If there is no instruction assigned to the functional unit, return
            return;
        }

        // Mark the functional unit as busy and set the execution time
        fu->busy = true;
        fu->time_left = fu->inst->time;

        // Mark the destination register as used and update its usage by the functional unit
        registers[fu->inst->dest].used_as = dest;
        registers[fu->inst->dest].dest_used_by = fu->id;

        // Update source registers if they are not used as destinations by other instructions
        if (!USED_AS_DEST(fu->inst->src1))
            registers[fu->inst->src1].used_as = src;
        if (!USED_AS_DEST(fu->inst->src2))
            registers[fu->inst->src2].used_as = src;

        // Handle true dependencies for source operands
        if (USED_AS_DEST(fu->inst->src1))
        {
            fu->qj = registers[fu->inst->src1].dest_used_by;
            fu->vj = 0;
            fu->locks1 = true;
        }
        else
        {
            fu->qj = -1;
            fu->vj = fu->inst->src1;
        }

        if (USED_AS_DEST(fu->inst->src2))
        {
            fu->qk = registers[fu->inst->src2].dest_used_by;
            fu->vk = 0;
            fu->locks2 = true;
        }
        else
        {
            fu->qk = -1;
            fu->vk = fu->inst->src2;
        }
    }
}

void reorder()
{
    // Find the next instruction to commit in the reorder buffer
    auto i = std::find(reorder_buffer.begin(), reorder_buffer.end(), &inst_list[next_reorder]);

    // Continue until either the end of the reorder buffer is reached or all instructions are committed
    while (i != reorder_buffer.end() && next_reorder < inst_list.size())
    {
        // Commit the instruction by marking its commit time
        (*i)->commit = ticks;

        // Remove the instruction from the reorder buffer
        reorder_buffer.erase(i);

        // Move to the next instruction in the inst_list
        next_reorder++;

        // Find the next instruction to commit in the reorder buffer
        i = std::find(reorder_buffer.begin(), reorder_buffer.end(), &inst_list[next_reorder]);
    }
}

int exec()
{
    int ret = 1; // Assume all instructions are executed until proven otherwise

    // Check if any functional unit is still busy executing an instruction
    for (int i = 0; i < STATION_TYPES; i++)
    {
        for (int j = 0; j < station_sizes[i]; j++)
        {
            if (all_stations[i][j].busy)
            {
                ret = 0; // Set to 0 if any functional unit is busy
            }
        }
    }

    // Check if the instruction queue is empty
    ret = ret && insts.empty();

    // If all instructions are executed and the instruction queue is empty, return
    if (ret)
        return ret;

    // Increment the clock cycle
    ticks++;

    // Issue the next instruction
    issue();

    // Execute instructions in all functional units
    for (int i = 0; i < STATION_TYPES; i++)
    {
        for (int j = 0; j < station_sizes[i]; j++)
        {
            exec_fu(&all_stations[i][j]); // Execute functional unit
        }
    }

    // Reorder the buffer
    reorder();

    return ret; // Return whether all instructions are executed
}

void fus()
{
    // Print header for functional units status
    std::cout << "Unidades Funcionais:\n";
    std::cout << "Time\tFU\tBusy\tOp\tVi\tVj\tVk\tQj\tQk\n";

    // Loop through all functional units
    for (int i = 0; i < STATION_TYPES; i++)
    {
        for (int j = 0; j < station_sizes[i]; j++)
        {
            // Print status of each functional unit
            if (all_stations[i][j].time_left != -1)
            {
                // Print remaining execution time if the unit is busy
                std::cout << all_stations[i][j].time_left;
            }
            std::cout << "\t" << str_fus[(2 * i) + j + 1] << "\t" << all_stations[i][j].busy << "\t"; // Print unit ID and busy status

            if (all_stations[i][j].busy)
            {
                // If the unit is busy, print the operation it is executing
                std::cout << str_op[all_stations[i][j].inst->op];
            }
            else
            {
                // If the unit is idle, print a dash
                std::cout << "-";
            }
            std::cout << "\t";

            if (all_stations[i][j].inst != nullptr)
            {
                // If there is an instruction in the unit, print its destination register
                std::cout << str_reg[all_stations[i][j].inst->dest];
            }
            else
            {
                // If no instruction is in the unit, print a dash
                std::cout << "-";
            }
            std::cout << "\t" << str_reg[all_stations[i][j].vj] << "\t" << str_reg[all_stations[i][j].vk] << "\t"; // Print source operands (Vj and Vk)

            // Print reservation stations for source operands (Qj and Qk)
            std::cout << str_fus[all_stations[i][j].qj + 1] << "\t" << str_fus[all_stations[i][j].qk + 1] << "\n";
        }
    }
    std::cout << "\n"; // Add a newline for better readability
}

void show()
{
    // Print header for the registers display
    std::cout << "Registradores: \n";
    std::cout << "Visiveis       |      Invisiveis\n";

    // Loop through visible registers and their corresponding invisible registers
    for (int i = 1; i <= VISIBLE_REGISTERS; i++)
    {
        // Print the value of each visible register and its corresponding invisible registers
        std::cout << str_reg[i] << ": " << registers[i].value << "\t\t    " << str_reg[i + VISIBLE_REGISTERS] << ": " << registers[i + VISIBLE_REGISTERS].value << "\t" << str_reg[i + (2 * VISIBLE_REGISTERS)] << ": " << registers[i + (2 * VISIBLE_REGISTERS)].value << "\n";
    }
    std::cout << "\n"; // Add a newline for better readability
}

void menu()
{
    // Print menu options
    std::cout << "Menu:\n";
    std::cout << "\tCycle (c)\n";           // Option to execute one cycle
    std::cout << "\tExit (e)\n";            // Option to exit the program
    std::cout << "\tFunctional units (f)\n"; // Option to display functional units status
    std::cout << "\tNext (n)\n";            // Option to execute until the next instruction is issued
    std::cout << "\tRegister (r)\n";   // Option to display register values
}

std::vector<inst_t> read(const std::string &filename)
{
    std::vector<inst_t> code;
    static std::map<std::string, op_t> op_map;

    // Initialize op_map if empty
    if (op_map.empty())
    {
        // Mapping operation strings to op_t enum values
        op_map["add"] = add;
        op_map["sub"] = sub;
        op_map["mul"] = mul;
        op_map["div"] = divd;
        op_map["lw "] = lw;
        op_map["sw "] = sw;
    }

    static std::map<std::string, reg_t> reg_map;

    // Initialize reg_map if empty
    if (reg_map.empty())
    {
        // Mapping register strings to reg_t enum values
        // Mapping visible registers
        reg_map["r0"] = r0;
        reg_map["r1"] = r1;
        reg_map["r2"] = r2;
        reg_map["r3"] = r3;
        reg_map["r4"] = r4;
        reg_map["r5"] = r5;
        reg_map["r6"] = r6;
        reg_map["r7"] = r7;
        reg_map["r8"] = r8;
        reg_map["r9"] = r9;
        reg_map["r10"] = r10;
        reg_map["ra"] = ra;
        reg_map["rb"] = rb;
        reg_map["rc"] = rc;
        reg_map["rd"] = rd;
        reg_map["re"] = re;
        reg_map["rf"] = rf;
        reg_map["rg"] = rg;
        reg_map["rh"] = rh;
        reg_map["ri"] = ri;
        reg_map["rj"] = rj;
        reg_map["rk"] = rk;
        reg_map["rl"] = rl;
        reg_map["rm"] = rm;
        reg_map["rn"] = rn;
        reg_map["ro"] = ro;
        reg_map["rp"] = rp;
        reg_map["rq"] = rq;
        reg_map["rr"] = rr;
        reg_map["rs"] = rs;
        reg_map["rt"] = rt;
        reg_map["ru"] = ru;
        reg_map["rv"] = rv;
        reg_map["rw"] = rw;
        reg_map["rx"] = rx;
    }

    static int op_time[6] = {add_time, sub_time, mul_time, lw_time, div_time, sw_time};

    std::string raw_inst;
    // Open the file for reading
    std::ifstream file(filename);
    if (file.is_open())
    {
        // Read each line of the file
        while (std::getline(file, raw_inst))
        {
            // Initialize fields of the instruction
            inst_t i;
            size_t comma1, comma2, paren;
            i.issue = 0;
            i.exec = 0;
            i.write = 0;
            i.commit = 0;
            i.imm = -1;
            i.op = op_map[raw_inst.substr(0, 3)];

            // Parse the instruction
            if (i.op == sw || i.op == lw)
            {
                comma1 = raw_inst.find(',');
                if (i.op == sw)
                {
                    i.dest = noreg;
                    i.src1 = reg_map[raw_inst.substr(3, comma1 - 3)];
                }
                else
                {
                    i.dest = reg_map[raw_inst.substr(3, comma1 - 3)];
                    i.src1 = noreg;
                }
                paren = raw_inst.find('(');
                i.imm = std::stoi(raw_inst.substr(comma1 + 2, paren - comma1 - 2));
                i.src2 = reg_map[raw_inst.substr(paren + 1, raw_inst.length() - paren - 2)];
                i.time = op_time[i.op];
            }
            else
            {
                comma1 = raw_inst.find(',');
                i.dest = reg_map[raw_inst.substr(4, comma1 - 4)];
                comma2 = raw_inst.find(',', comma1 + 1);
                i.src1 = reg_map[raw_inst.substr(comma1 + 2, comma2 - comma1 - 2)];
                i.src2 = reg_map[raw_inst.substr(comma2 + 2, raw_inst.length() - comma2 - 2)];
                i.time = op_time[i.op];
            }
            // Add parsed instruction to the vector
            code.push_back(i);
        }
        file.close(); // Close the file
    }
    else
    {
        // Print error message if file couldn't be opened
        std::cout << "Error opening file: " << filename << std::endl;
    }

    for (auto &i : code)
    {
        insts.push_back(&i);
    }
    return code; // Return vector containing parsed instructions
}

int main()
{
    std::string filename;

    std::cout << "Tomasulo simulator\n";
    std::cout << "Enter the instruction file name: ";
    std::getline(std::cin, filename); // Get the instruction file name from the user

    std::vector<inst_t> inst_list = read(filename); // Read instructions from the file
    if (inst_list.empty())
    {
        std::cout << "Invalid file name or empty file\n";
        return 1; // If file reading failed or file is empty, return error code 1
    }

    std::string input;
    init_fus(); // Initialize functional units

    while (true)
    {
        std::cout << ">"; // Display prompt
        std::getline(std::cin, input); // Get user input

        // Handle user commands
        if (input == "registers" || input == "r")
        {
            show(); // Display register values
        }
        else if (input == "fus" || input == "f")
        {
            fus(); // Display functional units status
        }
        else if (input == "next" || input == "n")
        {
            if (exec()) // Execute one cycle of the simulator
            {
                std::cout << "All operations done\n";
            }
            else
            {
                std::cout << "Cycle: " << ticks << "\n"; // Display current cycle
            }
        }
        else if (input == "clock" || input == "c")
        {
            std::cout << "Cycle: " << ticks << "\n"; // Display current cycle
        }
        else if (input == "exit" || input == "e")
        {
            break; // Exit the simulation loop
        }
        else
        {
            std::cout << "Invalid command\n"; // Handle invalid user input
            menu(); // Display menu options
        }
    }

    std::cout << "Simulation complete (press enter to exit)" << std::endl;
    std::cin.get(); // Wait for user to press enter before exiting
    return 0; // Return success code
}
