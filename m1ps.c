
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

struct imps_file {
    uint32_t num_instructions;
    uint32_t entry_point;
    uint32_t *instructions;
    uint32_t *debug_offsets;
    uint16_t memory_size;
    uint8_t *initial_data;
};

void read_imps_file(char *path, struct imps_file *executable);
void execute_imps(struct imps_file *executable, int trace_mode, char *path);
void print_uint32_in_hexadecimal(FILE *stream, uint32_t value);
void print_int32_in_decimal(FILE *stream, int32_t value);
uint32_t read_num_inst(FILE *file);
uint16_t read_num_inst_16(FILE *file);

int main(int argc, char *argv[]) {

    char *pathname;
    int trace_mode = 0;

    if (argc == 2) {
        pathname = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "-t") == 0) {
        trace_mode = 1;
        pathname = argv[2];
    } else {
        fprintf(stderr, "Usage: imps [-t] <executable>\n");
        exit(1);
    }

    struct imps_file executable = {0};
    read_imps_file(pathname, &executable);

    execute_imps(&executable, trace_mode, pathname);

    free(executable.debug_offsets);
    free(executable.instructions);
    free(executable.initial_data);

    return 0;
}

// this reads a 32 bit number a given file - (helper function)
uint32_t read_num_inst(FILE *file) {

    uint32_t val = 0;

    // reads 4 bytes at a time until the end of the file
    for (int i = 0; i < 4; i++) {
        int byte = fgetc(file);

        if (byte == EOF) {
            continue;
        }

        // shifts the byte where it needs to be
        val = val | ((uint32_t)(byte) << (i * 8));
    }

    // returns the new value
    return val;
}

// this reads a 16 bit number from a given file - (helper function)
uint16_t read_num_inst_16(FILE *file) {

    uint16_t val = 0;

    // reads 2 bytes at a time until the end of the file
    for (int i = 0; i < 2; i++) {
        int byte = fgetc(file);

        if (byte == EOF) {
            continue;
        }

        // shifts the bytes to where it needs to be
        val = val | ((uint16_t)(byte & 0xFF) << (i * 8));
    }

    // returns the new value
    return val;
}


// Read an IMPS executable file from the file at `path` into `executable`.
// Exits the program if the file can't be accessed or is not well-formed.
void read_imps_file(char *path, struct imps_file *executable) {

    // opens the file
    FILE *file = fopen(path, "rb");

    // error checking
    if (file == NULL) {
        perror(path);
        exit(1);
    }

    // initialises then reads the magic number
    uint8_t m_number[4];

    for (int i = 0; i < 4; i++) {
        m_number[i] = fgetc(file);
    }

    // checks if the magic number is the correct one for IMPS
    if (m_number[0] != 0x49 || m_number[1] != 0x4d || m_number[2] != 0x50 || m_number[3] != 0x53) {
        fprintf(stderr, "Invalid IMPS file\n");
        fclose(file);
        exit(1);
    }

    // reads the number of instructions
    executable->num_instructions = read_num_inst(file);
    executable->entry_point = read_num_inst(file);

    // allocating memeory
    executable->instructions = malloc(executable->num_instructions * sizeof(uint32_t));

    // checking if it worked
    if (!(executable->instructions)) {
        perror("That wasnt meant to happen :(   ");
        fclose(file);
        exit(1);
    }

    // reads all instructions and inserts them into array
    for (uint32_t i = 0; i < executable->num_instructions; i++) {
        executable->instructions[i] = read_num_inst(file);
    }

    // allocates memory
    executable->debug_offsets = malloc(executable->num_instructions * sizeof(uint32_t));

    // checking if it worked
    if (!(executable->debug_offsets)) {
        perror("That wasnt meant to happen :(   ");
        free(executable->instructions);
        fclose(file);
        exit(1);
    }

    // reads all debug offsets and inserts them into array
    for (uint32_t i = 0; i < executable->num_instructions; i++) {
        executable->debug_offsets[i] = read_num_inst(file);
    }

    // reads the memory (16 bit one)
    executable->memory_size = read_num_inst_16(file);

    // allocating memory
    executable->initial_data = malloc(executable->memory_size * sizeof(uint8_t));

    // checking if it worked
    if (!(executable->initial_data)) {
        perror("That wasnt meant to happen :(   ");
        free(executable->instructions);
        free(executable->debug_offsets);
        fclose(file);
        exit(1);
    }

    // reads data and inserts it into array
    for (uint16_t i = 0; i < executable->memory_size; i++) {
        int byte = fgetc(file);
        executable->initial_data[i] = (uint8_t)(byte & 0xFF);
    }

    // closes the file
    fclose(file);

}


// Executa an IMPS program
void execute_imps(struct imps_file *executable, int trace_mode, char *path) {

    // initialises all registers to 0
    uint32_t registar[32] = {0};

    // program couter is the entry point now
    int prog_counter = executable->entry_point;

    // loops through instructions
    while (prog_counter < executable->num_instructions) {
        // currect instruction
        uint32_t instruction = executable->instructions[prog_counter];
        // current operation
        uint8_t operation = (instruction >> 26) & 0x3F;

        // checking which operation is used (wherther syscall, addi, etc)
        if (operation == 0) {

            uint32_t function = instruction & 0x3F;

            // add operation (the "else if" function is the syscall and the
            // "else" is an error)
            if (function == 0x20) {
                uint32_t rs = (instruction >> 21) & 0x1F;
                uint32_t rt = (instruction >> 16) & 0x1F;
                uint32_t rd = (instruction >> 11) & 0x1F;

                // excluding registar 0
                if (rd != 0) {
                    registar[rd] = registar[rs] + registar[rt];
                }
            } else if (function == 0xC) {
                uint32_t sys_call = registar[2];
                // performing relevant syscalls
                if (sys_call == 1) {
                    print_int32_in_decimal(stdout, registar[4]);
                } else if (sys_call == 10) {
                    exit(1);
                } else if (sys_call == 11) {
                    putchar(registar[4]);
                } else {
                    fprintf(stderr, "IMPS error: bad syscall number\n");
                    exit(1);
                }
            } else {
                // errors
                fprintf(stderr, "IMPS error: bad instruction ");
                print_uint32_in_hexadecimal(stdout, instruction);
                fprintf(stderr, "\n");
                exit(1);
            }

        } else if (operation == 0x08) {
            // ADDI instruction
            uint32_t rs = (instruction >> 21) & 0x1F;
            uint32_t rt = (instruction >> 16) & 0x1F;
            int16_t imm = (instruction & 0xFFFF);

            // excluding the 0 registar
            if (rt != 0) {
                registar[rt] = registar[rs] + imm;
            }

        } else if (operation == 0x0D) {
            // ORI instruction
            uint32_t rs = (instruction >> 21) & 0x1F;
            uint32_t rt = (instruction >> 16) & 0x1F;
            uint16_t imm = (instruction & 0xFFFF);

            // excluding the 0 registar
            if (rt != 0) {
                registar[rt] = registar[rs] | imm;
            }

        } else if (operation == 0x0F) {
            // LUI instruction
            uint32_t rt = (instruction >> 16) & 0x1F;
            uint16_t imm = instruction & 0xFFFF;

            // excluding the 0 registar
            if (rt != 0) {
                registar[rt] = ((uint32_t)imm) << 16;
            }

        } else {
            // error
            fprintf(stderr, "IMPS error: bad instruction ");
            print_uint32_in_hexadecimal(stderr, instruction);
            fprintf(stderr, "\n");
            exit(1);
        }

        // increases the counter
        prog_counter++;
    }

    // another error message
    fprintf(stderr, "IMPS error: execution past the end of instructions\n");
    exit(1);

}

// Print out a 32 bit integer in hexadecimal, including the leading `0x`.
//
// @param stream The file stream to output to.
// @param value The 32 bit integer to output.
void print_uint32_in_hexadecimal(FILE *stream, uint32_t value) {
    fprintf(stream, "0x%08" PRIx32, value);
}

// Print out a signed 32 bit integer in decimal.
//
// @param stream The file stream to output to.
// @param value The 32 bit integer to output.
void print_int32_in_decimal(FILE *stream, int32_t value) {
    fprintf(stream, "%" PRIi32, value);
}
