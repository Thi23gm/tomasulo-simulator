# Tomasulo Simulator

This is a Tomasulo simulator program that simulates the execution of instructions using the Tomasulo algorithm.

## Instructions for Running the Program

1. **Compile the Program:**
    - Compile the program source files using a C++ compiler. For example:
      ```
      g++ -o tomasulo_simulator main.cpp tomasulo.cpp
      ```

2. **Run the Program:**
    - Run the compiled executable with the command:
      ```
      ./tomasulo_simulator
      ```

3. **Enter Instruction File Name:**
    - When prompted, enter the name of the instruction file containing the instructions to be executed.

4. **Execute Commands:**
    - After entering the instruction file name, you will be prompted with a command line interface.
    - You can enter the following commands:
      - `r`: Display register values.
      - `f`: Display functional units status.
      - `n`: Execute one cycle of the simulator.
      - `c`: Display the current cycle.
      - `e`: Exit the simulation.

5. **Simulation Completion:**
    - Once all operations are completed, the program will display a message indicating simulation completion.
    - Press Enter to exit the program.

## Example Instruction Files

- You can use the provided example instruction files to test the simulator:
    - `inputs/instrucoes.txt`: Contains divd, add, mul and sub instructions.
    - `inputs/instrucoes2.txt`: Contains lw, sw, divd, add, mul and sub instructions.

