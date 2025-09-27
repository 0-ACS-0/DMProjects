/*

*/

/* ---- Library --------------------------------------------------- */
#include "../inc/dmcli_io.h"
#include <ctype.h>

/* ---- Helper functions implementation prototypes ---------------- */
bool _dmcli_helper_enterm_rawmode(dmcli_io_pt dmcli_io);
bool _dmcli_helper_disterm_rawmode(dmcli_io_pt dmcli_io);


/* ---- INTERNAL - Functions implementation ----------------------- */
// ======== Allocators:
/*
    @brief Function to allocate and prepare the structure dmcli_io to defaults for its use.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.

    @retval true: If allocation succeeded.
    @retval false: If allocation failed.
*/
bool dmcli_io_alloc(dmcli_io_pt * dmcli_io){
    // Reference check:
    if (!dmcli_io) return false;

    // Structure allocation:
    *dmcli_io = calloc(1, sizeof(dmcli_io_t));
    if (!(*dmcli_io)) return false;

    // Allocation to defaults:
    if (!dmcli_io_set_default(*dmcli_io)){
        free(*dmcli_io);
        *dmcli_io = NULL;
        return false;
    }

    return true;
}

/*
    @brief Function to deallocate all the memory reserved for dmcli_io, reseting all the values to invalid or
    default.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io.

    @retval true: if deallocation succeeded.
    @retval false: if deallocation failed.
*/
bool dmcli_io_dealloc(dmcli_io_pt * dmcli_io){
    // Check references:
    if (!dmcli_io || !(*dmcli_io)) return false;

    // Deallocation of the prompt string if set.
    if ((*dmcli_io)->prompt) {free((*dmcli_io)->prompt); (*dmcli_io)->prompt = NULL;}

    // Deallocation of the input string if set.
    if ((*dmcli_io)->input) {free((*dmcli_io)->input); (*dmcli_io)->input = NULL;}

    // Deallocation of all the input log:
    for (size_t i = 0; i < (*dmcli_io)->ilog_capacity; i++){
        if ((*dmcli_io)->ilog && (*dmcli_io)->ilog[i]) {free((*dmcli_io)->ilog[i]); (*dmcli_io)->ilog[i] = NULL;}
    }
    if ((*dmcli_io)->ilog) {free((*dmcli_io)->ilog); (*dmcli_io)->ilog = NULL;}

    // Reset input log capacity to default:
    (*dmcli_io)->ilog_capacity = 0;
    (*dmcli_io)->ilog_length = 0;

    // Deallocate the io structure completly:
    free(*dmcli_io);
    *dmcli_io = NULL;

    return true;
}

// ==== "Getters" & "Setters":
/*
    @brief Function to set the prompt and the input log length to default values.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.

    @retval true: If succeeded in setting values to default.
    @retval false: If failed in setting values to default.
*/
bool dmcli_io_set_default(dmcli_io_pt dmcli_io){
    // Reference check:
    if (!dmcli_io) return false;

    // Default prompt and ilog length:
    bool ret = true;
    ret &= dmcli_io_set_inputcap(dmcli_io, DEFAULT_IO_INPUT_CAP);
    ret &= dmcli_io_set_prompt(dmcli_io, DEFAULT_IO_PROMPT_STRING);
    ret &= dmcli_io_set_ilogcap(dmcli_io, DEFAULT_IO_ILOG_CAP); 

    return ret;
}

/*
    @brief Function to set the input capacity.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.
    @param size_t input_capacity: Capacity allowed for the user input.

    @retval true: If allocation succeeded.
    @retval false: If allocation failed.
*/
bool dmcli_io_set_inputcap(dmcli_io_pt dmcli_io, size_t input_capacity){
    // References check:
    if (!dmcli_io) return false;

    // Deallocate previous input capacity (if allocated):
    if (dmcli_io->input) free(dmcli_io->input);

    // Allocate the memory for user input:
    dmcli_io->input = calloc(input_capacity, sizeof(char));
    if (!dmcli_io->input) return false;

    // Set the capacity and length:
    dmcli_io->input_capacity = input_capacity;
    dmcli_io->input_length = 0;
    return true;
}

/*
    @brief Function to set the prompt to a custom string.
    @note: This function doesn't explicitly checks if the prompt string is '\0' terminated,
    that formatting is assumed correctly always when using this function!

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.
    @param const char * prompt_str: String of the prompt.

    @retval true: If succeeded in setting the prompt.
    @retval false: If failed in setting the prompt.
*/
bool dmcli_io_set_prompt(dmcli_io_pt dmcli_io, const char * prompt_str){
    // References check:
    if (!dmcli_io || !prompt_str) return false;

    // Temporary allocation of the new prompt string:
    char * temp_prompt = strdup(prompt_str);
    if (!temp_prompt) return false;

    // Deallocation the old prompt memory and assignment (always) of the new prompt string:
    if (dmcli_io->prompt) free(dmcli_io->prompt);
    dmcli_io->prompt = temp_prompt;
    dmcli_io->prompt_length = strlen(temp_prompt);

    return true;
}

/*
    @brief Function to establish the input log length (number of user input to register in memory).
    
    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.
    @param size_t ilog_capacity: Length of the new input log capacity.

    @return true: If succeeded in setting the new ilog_length.
    @return false: If failed in setting the new ilog_length.
*/
bool dmcli_io_set_ilogcap(dmcli_io_pt dmcli_io, size_t ilog_capacity){
    // Reference check:
    if (!dmcli_io) return false;

    // Deallocates the previous input log:
    for (size_t i = 0; i < dmcli_io->ilog_capacity; i++){
        if (dmcli_io->ilog && dmcli_io->ilog[i]) {free(dmcli_io->ilog[i]); dmcli_io->ilog[i] = NULL;}
    }
    if (dmcli_io->ilog) {free(dmcli_io->ilog); dmcli_io->ilog = NULL;}

    // Set the capacity value:
    dmcli_io->ilog_capacity = ilog_capacity;
    dmcli_io->ilog_length = 0;

    // In case capacity is 0, is already done:
    if (!ilog_capacity) return true;

    // In case of capacity given, reserves the memory to allocate input strings:
    dmcli_io->ilog = calloc(dmcli_io->ilog_capacity, sizeof(char *));
    if (!dmcli_io->ilog) {dmcli_io->ilog_capacity = 0; return false;}

    return true;
}

/*
    @brief Function to get the prompt string.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io struct.

    @return NULL: If fails.
    @return char *: Reference to the prompt string.
*/
char * dmcli_io_get_prompt(dmcli_io_pt dmcli_io){
    // Check references and bounds:
    if (!dmcli_io) return NULL;

    // Copy the prompt string to the external variable promopt_str:
    return dmcli_io->prompt;
}

/*
    @brief Function to retrieve the input capacity.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.

    @retval size_t: Capacity allocated for user input.
*/
size_t dmcli_io_get_inputcap(dmcli_io_pt dmcli_io){
    // Reference check:
    if (!dmcli_io) return 0;
    
    return dmcli_io->input_capacity;
}

/*
    @brief Function to retreive the input length.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.

    @retval size_t: Length of the actual input string.
*/
size_t dmcli_io_get_inputlen(dmcli_io_pt dmcli_io){
    // Reference check:
    if (!dmcli_io) return 0;

    return dmcli_io->input_length;
}


/*
    @brief Function to get the capacity of the input log.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.

    @return size_t: The capacity of the input log.
*/
size_t dmcli_io_get_ilogcap(dmcli_io_pt dmcli_io){
    // Reference check:
    if (!dmcli_io) return 0;

    return dmcli_io->ilog_capacity;
}


/*
    @brief Function to get the length of the input log.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.

    @return size_t: The length of the input log.
*/
size_t dmcli_io_get_iloglen(dmcli_io_pt dmcli_io){
    // Reference check:
    if (!dmcli_io) return 0;

    return dmcli_io->ilog_length;
}

/*
    @brief Function to retrieve the input at a specific position of the input log.
    @note: This function doesn't make a copy of the string, if a new input is issued,
    the position of the input in the log will change and the last reference will be both
    invalid or the reference of a newer input entry.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io_struct.
    @param size_t ilog_index: Position of the input in the log.

    @retval NULL: No input logged or error.
    @retval char *: Reference to the input string at position.
*/
char * dmcli_io_get_ilogat(dmcli_io_pt dmcli_io, size_t ilog_index){
    // Reference & index bounds check:
    if (!dmcli_io) return NULL;
    if ((ilog_index >= dmcli_io->ilog_capacity) || (ilog_index > dmcli_io->ilog_length)) return NULL;

    // Reference:
    return dmcli_io->ilog[ilog_index];
}

/*
    @brief Function to retrieve the input issued instantly. 
    @note: This function retrieve the input reference (a copy), which means that any
    user input will overwrite the last input stored at that reference. That's why the brief
    says "instantly".

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.
    
    @retval NULL: If any error occurred.
    @retval char *: Reference to the input string.
*/
char * dmcli_io_get_input(dmcli_io_pt dmcli_io){
    // Reference check:
    if (!dmcli_io) return NULL;

    // Input reference copy return:
    return dmcli_io->input;
}

// ==== Utils:
/*
    @brief Function that blocks and waits for user input (\n) and process it to be accessible and
    register to the input log.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io structure.

    @return true: If everything was correct.
    @return false: If failed midway.
*/
bool dmcli_wait4input(dmcli_io_pt dmcli_io){
    // Reference check: 
    if (!dmcli_io) return false;

    // Raw mode check (only works if raw mode is active):
    if (!dmcli_io->is_raw) return false;

    // Clear the input buffer:
    memset(dmcli_io->input, '\0', dmcli_io->input_capacity);

    // Show prompt:
    printf("%s", dmcli_io->prompt);
    fflush(stdout);

    // Read/Write from the user (until '\n' is pressed):
    size_t ilog_index = 0;
    while (true){
        // Char pressed & read number local variables:
        char ch = '\0';
        ssize_t rn = 0;

        // Read character from user input (1 Byte):
        rn = read(STDIN_FILENO, &ch, 1);
        if (rn <= 0) return false;

        // Special characters functions:
        if ((ch == '\r') || (ch == '\n')){
            // Enter key:
            break;
        }

        if ((ch == 127) || (ch == '\b')){
            // Backspace key:
            continue;
        }

        if (ch == '\x1b'){
            // Escape sequence:
            char seq[2];
            if ((rn = read(STDIN_FILENO, &seq[0], 1)) == 0) continue;
            if (rn < 0) return false;

            if ((rn = read(STDIN_FILENO, &seq[1], 1)) == 0) continue;
            if (rn < 0) return false;

            // Up arrow:
            if ((seq[0] == '[') && (seq[1] == 'A')) {}

            // Down arrow:
            if ((seq[0] == '[') && (seq[1] == 'B')) {}

            continue;
        }

        if (isprint(ch)){
            // Printable character:
            continue;
        }
    }


    // Move the previous input to input log:
    if (dmcli_io->input){
        // In case the input log is full, deallocate the older input log:
        if (dmcli_io->ilog_length == dmcli_io->ilog_capacity) free(dmcli_io->ilog[dmcli_io->ilog_capacity-1]);

        // Move all the input logs one to the "right" (one older):
        for (size_t i = dmcli_io->ilog_length; i > 0; i--){
            dmcli_io->ilog[i] = dmcli_io->ilog[i-1];
        }

        // Copy the last input to the input log first position (newer):
        dmcli_io->ilog[0] = strdup(dmcli_io->input);
    }

    return true;
}

/* ---- INTERNAL - Helpers functions implementation --------------- */
// ==== Terminal ctl:
/*
    @brief Function to enable terminal raw mode.

    @param dmcli_io_pt dmcli_io: Reference to dmcli_io struct.

    @retval true: If enable succeeded.
    @retval false: If enable failed.
*/
bool _dmcli_helper_enterm_rawmode(dmcli_io_pt dmcli_io){
    // Check references and current mode:
    if (!dmcli_io || dmcli_io->is_raw) return false;

    // Temporary terminal raw settings:
    struct termios temp_raw;

    // Copy of old terminal settings into dmcli_io:
    if (tcgetattr(STDIN_FILENO, &dmcli_io->orig_termios) < 0) return false;
    temp_raw = dmcli_io->orig_termios;

    // Disable canonical mode and echo, apply modified settings to enter raw mode:
    temp_raw.c_lflag &= ~(ICANON | ECHO);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &temp_raw) < 0) return false;

    // Set raw flag:
    dmcli_io->is_raw = true;
    return true;
}

/*
    @brief Function to disable terminal raw mode.

    @param dmcli_io_pt dmcli_io: Reference to structure dmcli_io.

    @retval true: If disable succeeded.
    @retval false: If disable failed
*/
bool _dmcli_helper_disterm_rawmode(dmcli_io_pt dmcli_io){
    // Check references and current mode:
    if (!dmcli_io || !dmcli_io->is_raw) return false;

    // Set the original terminal input output stream flags:
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &dmcli_io->orig_termios) < 0) return false;

    // Unset raw flag:
    dmcli_io->is_raw = false;
    return true;
}

// ==== Input ctl:
/*

*/