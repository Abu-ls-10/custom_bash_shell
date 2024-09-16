# Custom Bash Shell

## Overview

This custom “bash” shell is a Unix-like command-line interpreter developed in the C programming language. Inspired by the classic Bash shell, this project aims to provide users with a robust and efficient command-line interface for interacting with the operating system. The shell supports a wide range of built-in commands, piping, input/output redirection, and job control functionalities, offering users a powerful tool for managing their system tasks and workflows.


## Key Features

- **Command Execution**: Allows users to execute various system commands and scripts directly from the shell, providing a familiar interface for interacting with the operating system.
  
- **Piping**: Supports the chaining of multiple commands together using pipes (`|`), enabling users to redirect the output of one command as the input to another.
  
- **Input/Output Redirection**: Facilitates input and output redirection using `<` and `>` operators, allowing users to read from and write to files directly from the command line.
  
- **Job Control**: Implements job control functionalities, enabling users to run commands in the background (`&`), suspend and resume processes, and manage process groups effectively.
  
- **Scripting Support**: Provides users with the ability to write shell scripts to automate tasks and workflows, enhancing productivity and efficiency in system administration tasks.


## Technologies Used

- **IDE**: CLion and VSCode
  
- **C Programming Language**: Utilized C to develop the shell, leveraging its low-level system access and efficiency in handling system calls and process management.
  
- **Unix System Calls**: Leveraged Unix system calls such as `fork`, `exec`, `pipe`, `dup2`, and `wait` to implement the core functionalities of the shell, including process creation, execution, and management.
  
- **Command-Line Interface (CLI)**: Designed a user-friendly CLI, allowing users to interact with the shell using text-based commands and keyboard inputs.
  
- **File I/O Operations**: Implemented file input/output operations to support input redirection (`<`) and output redirection (`>`), enabling users to interact with files directly from the command line.


## How to Run

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/custom-bash-shell.git
   cd custom-bash-shell

2. Compile the project:
   ```bash
   make

4. Run the shell:
   ```bash
   ./custom_shell


## Usage Examples

- **Execute a command**:
   ```bash
   /bin/ls -l

- **Pipe multiple commands**:
   ```bash
   /bin/ls -l | /usr/bin/wc -l

- **Redirect output to a file**:
   ```bash
   /bin/echo "Hello, World!" > output.txt

- **Redirect input from a file**:
   ```bash
   /usr/bin/wc -l < input.txt

- **Run a command in the background**:
   ```bash
   /bin/sleep 10 &


## Acknowledgements

Much appreciation to Saad.H and Adam.E for contributing to this project.

