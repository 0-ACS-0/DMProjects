/* ---- Library --------------------------------------------------- */
#include "dmlogger.h"
#include <stdbool.h>


/* ---- Static function prototypes -------------------------------- */
static void * _dmlogger_logger(void * args);

// Internal output functions:
static bool __dmlogger_logger_write(dmlogger_pt dmlogger, struct dmlogger_entry * entry);
static void __dmlogger_gen_file_fullname(dmlogger_pt dmlogger, char * ffullname, size_t ffullname_len, struct tm * lt);
static bool __dmlogger_rotate_file_bydate(dmlogger_pt dmlogger);
static bool __dmlogger_rotate_file_bysize(dmlogger_pt dmlogger, size_t log_fullmsg_size);

/* ---- Function implementation ----------------------------------- */
// ======== Initialization / Run / Deinitialization:
/*
    @brief Function to initialize the dmlogger.
    Handle memory allocation and sets all configurable values to default. Starts directly
    the consumer thread if nothing fails.

    @param dmlogger_pt * dmlogger: Double reference to dmlogger.
    @param dmlogger_conf_pt dmlogger_conf: Reference to the configuration of dmlogger.
*/
void dmlogger_init(dmlogger_pt * dmlogger){
    // Pointer check:
    if (!dmlogger) return;

    // Dmlogger structure allocation:
    (*dmlogger) = calloc(1, sizeof(dmlogger_t));
    if (!(*dmlogger)) return;

    // Queue allocation and configuration:
    (*dmlogger)->queue.capacity = DEFAULT_QUEUE_CAPACITY;
    (*dmlogger)->queue.equeue = calloc((*dmlogger)->queue.capacity, sizeof(struct dmlogger_entry));
    if (!(*dmlogger)->queue.equeue) {dmlogger_deinit(dmlogger); return;}

    if (pthread_mutex_init(&(*dmlogger)->queue.prod_mutex, NULL)) {dmlogger_deinit(dmlogger); return;}
    if (pthread_cond_init(&(*dmlogger)->queue.prod_cond, NULL)) {dmlogger_deinit(dmlogger); return;}
    if (pthread_mutex_init(&(*dmlogger)->queue.cons_mutex, NULL)) {dmlogger_deinit(dmlogger); return;}
    if (pthread_cond_init(&(*dmlogger)->queue.cons_cond, NULL)) {dmlogger_deinit(dmlogger); return;}

    (*dmlogger)->queue.of_policy = DEFAULT_QUEUE_OVERFLOW_POLICY;
    (*dmlogger)->queue.wait_timeout = DEFAULT_QUEUE_WAIT_TIMEOUT;

    // Output allocation and configuration - default(as stdio):
    if (pthread_mutex_init(&(*dmlogger)->output.mutex, NULL)) {dmlogger_deinit(dmlogger); return;}
    (*dmlogger)->output.osel = DEFAULT_OUTPUT_OSEL;
    (*dmlogger)->output.stdio.stream = stdout;

    // Minimum level configuration:
    (*dmlogger)->min_level = DEFAULT_LOG_MINLVL;

    // State change to running:
    (*dmlogger)->state = DMLOGGER_STATE_STOPPED;

}

/*
    @brief Function to start the consumer thread and write logic of the logger.

    @param dmlogger_pt dmlogger: Reference to dmlogger struct.

    @retval false: If errors occurred.
    @retval true: If is running correctly.
*/
bool dmlogger_run(dmlogger_pt dmlogger){
    // Reference check:
    if (!dmlogger || (dmlogger->state == DMLOGGER_STATE_RUNNING)) return false;

    // Logger thread launch:
    dmlogger->state = DMLOGGER_STATE_RUNNING;
    if (pthread_create(&dmlogger->logger_th, NULL, _dmlogger_logger, dmlogger)) {dmlogger->state = DMLOGGER_STATE_STOPPED; return false;}
    return true;
}

/*
    @brief Function to deinitialize the dmlogger.
    Handle memory deallocation. Stops directly the consumer thread for safely deallocate
    all the memory used.

    @param dmlogger_pt * dmlogger: Double reference to dmlogger.

    @retval None.
*/
void dmlogger_deinit(dmlogger_pt * dmlogger){
    // References check:
    if (!dmlogger || !(*dmlogger)) return;

    // Stops logger thread if running and set dmlogger state:
    if ((*dmlogger)->state == DMLOGGER_STATE_RUNNING) {(*dmlogger)->state = DMLOGGER_STATE_STOPPING; pthread_cond_signal(&(*dmlogger)->queue.cons_cond); pthread_join((*dmlogger)->logger_th, NULL);}
    (*dmlogger)->state = DMLOGGER_STATE_STOPPED;

    // Output free:
    if (((*dmlogger)->output.osel == DMLOGGER_OUTPUT_FILE) && ((*dmlogger)->output.file.fd)) {fclose((*dmlogger)->output.file.fd); (*dmlogger)->output.file.fd = NULL;}
    pthread_mutex_destroy(&(*dmlogger)->output.mutex);

    // Queue free:
    pthread_cond_destroy(&(*dmlogger)->queue.cons_cond);
    pthread_mutex_destroy(&(*dmlogger)->queue.cons_mutex);
    pthread_cond_destroy(&(*dmlogger)->queue.prod_cond);
    pthread_mutex_destroy(&(*dmlogger)->queue.prod_mutex);

    if ((*dmlogger)->queue.equeue) {free((*dmlogger)->queue.equeue); (*dmlogger)->queue.equeue = NULL;}

    // Dmlogger complete free:
    free((*dmlogger));
    (*dmlogger) = NULL;
}


// ======== CONFIGURATION - OUTPUT:
/*
    @brief Function to configure a file as an output of loggin messages.

    @param dmlogger_pt dmlogger: Reference to dmlogger struct.
    @param const char * file_path: String with the log file path.
    @param const char * file_basename: String with the log file basename.

    @retval false: If errors occurred.
    @retval true: If configuration succeeded.
*/
bool dmlogger_conf_output_file(dmlogger_pt dmlogger, const char * file_path, const char * file_basename, bool file_rotate_bydate, bool file_rotate_bysize, size_t file_rotate_maxsize){
    // References check:
    if (!dmlogger || !file_path || !file_basename) return false;

    // Strings length check:
    if ((strlen(file_path) > DEFAULT_OUTPUT_FILE_PATHLEN) || (strlen(file_basename) > DEFAULT_OUTPUT_FILE_BASENAMELEN)) return false;

    // Thread-safe on the fly configuration:
    pthread_mutex_lock(&dmlogger->output.mutex);

    // Close previous file (if any):
    if ((dmlogger->output.osel == DMLOGGER_OUTPUT_FILE) && dmlogger->output.file.fd) {fclose(dmlogger->output.file.fd); dmlogger->output.file.fd = NULL;}

    // Set the file-rotate flags to the output:
    dmlogger->output.file.date_rot = file_rotate_bydate;
    dmlogger->output.file.size_rot = file_rotate_bysize;
    dmlogger->output.file.max_size = (file_rotate_maxsize != 0) ? file_rotate_maxsize : DEFAULT_OUTPUT_FILE_ROTATE_MAXSIZE; 

    // Path and base name copy to the output.file structure:
    strncpy(dmlogger->output.file.path, file_path, DEFAULT_OUTPUT_FILE_PATHLEN - 1);
    strncpy(dmlogger->output.file.basename, file_basename, DEFAULT_OUTPUT_FILE_BASENAMELEN - 1);
    dmlogger->output.file.path[DEFAULT_OUTPUT_FILE_PATHLEN - 1] = '\0';
    dmlogger->output.file.basename[DEFAULT_OUTPUT_FILE_BASENAMELEN - 1] = '\0';
   
    // Local date:
    time_t t = time(NULL);
    struct tm lt;
    localtime_r(&t, &lt);

    // Check dates and upgrade the file date and index if is a new day:
    struct tm prev_lt;
    localtime_r(&dmlogger->output.file.date, &prev_lt);

    if ((lt.tm_year != prev_lt.tm_year) || (lt.tm_mon != prev_lt.tm_mon) || (lt.tm_mday != prev_lt.tm_mday)) {dmlogger->output.file.date = t; dmlogger->output.file.index = 0;}

    // Loop to create complete file name, open file, and check size:
    while (1){
        // Complete file name (path + basename + date + file index):
        char filename[DEFAULT_OUTPUT_FILE_BASENAMELEN + DEFAULT_OUTPUT_FILE_PATHLEN + DEFAULT_OUTPUT_FILE_EXTRALEN] = {0};
        __dmlogger_gen_file_fullname(dmlogger, filename, sizeof(filename), &lt);

        // Open file:
        dmlogger->output.file.fd = fopen(filename, "a");
        if (!dmlogger->output.file.fd) {pthread_mutex_unlock(&dmlogger->output.mutex); return false;}

        // Check file size:
        fseek(dmlogger->output.file.fd, 0, SEEK_END);
        dmlogger->output.file.size = ftell(dmlogger->output.file.fd);

        if (dmlogger->output.file.size == -1L) {fclose(dmlogger->output.file.fd); dmlogger->output.file.fd = NULL; pthread_mutex_unlock(&dmlogger->output.mutex); return false;}
        if (dmlogger->output.file.size == 0){dmlogger->output.file.index = 0; dmlogger->output.file.size = 0; break;}
        if (dmlogger->output.file.size >= dmlogger->output.file.max_size) {fclose(dmlogger->output.file.fd); dmlogger->output.file.index++;}
    }
    
    dmlogger->output.osel = DMLOGGER_OUTPUT_FILE;

    // Thread-safe on the fly configuration:
    pthread_mutex_unlock(&dmlogger->output.mutex);

    return true;
}

/*
    @brief Function to set the log output to the stdout stream.

    @param dmlogger_pt dmlogger: Reference to the dmlogger structure.

    @retval false: If errors occurred.
    @retval true: If configuration succeeded.
*/
bool dmlogger_conf_output_stdout(dmlogger_pt dmlogger){
    // Check reference:
    if (!dmlogger) return false;

    // Thread-safe on the fly configuration:
    pthread_mutex_lock(&dmlogger->output.mutex);

    // If there is any file opened as output, close:
    if ((dmlogger->output.osel == DMLOGGER_OUTPUT_FILE) && (dmlogger->output.file.fd)) {fclose(dmlogger->output.file.fd); dmlogger->output.file.fd = NULL;}

    // Set stream to stdout and output select to stdio:
    dmlogger->output.stdio.stream = stdout;
    dmlogger->output.osel = DMLOGGER_OUTPUT_STDOUT;

    // Thread-safe on the fly configuration:
    pthread_mutex_unlock(&dmlogger->output.mutex);

    return true;
}

/*
    @brief Function to set the log output to the stderr stream.

    @param dmlogger_pt dmlogger: Reference to the dmlogger structure.

    @retval false: If errors ocurred.
    @retval true: If configuration succeeded.
*/
bool dmlogger_conf_output_stderr(dmlogger_pt dmlogger){
    // Check reference:
    if (!dmlogger) return false;

    // Thread-safe on the fly configuration:
    pthread_mutex_lock(&dmlogger->output.mutex);

    // If there is any file opened as output, close:
    if ((dmlogger->output.osel == DMLOGGER_OUTPUT_FILE) && (dmlogger->output.file.fd)) {fclose(dmlogger->output.file.fd); dmlogger->output.file.fd = NULL;}

    // Set stream to stderr and output select to stdio:
    dmlogger->output.stdio.stream = stderr;
    dmlogger->output.osel = DMLOGGER_OUTPUT_STDERR;

    // Thread-safe on the fly configuration:
    pthread_mutex_unlock(&dmlogger->output.mutex);

    return true;
}

/*
    @brief Function to set a custom log output method for the logger.

    @param dmlogger_pt dmlogger: Reference to the dmlogger structure.
    @param void (*cwrite_fn)(const char *, void *): Function pointer to the custom write output function,
    which takes the log message generated as first argument and a custom data pointer (user given) as second
    position.
    @param void * cdata: Reference to user defined custom data.

    @retval false: If errors occurred.
    @retval true: If configuration succeeded.
*/
bool dmlogger_conf_output_custom(dmlogger_pt dmlogger, void (*cwrite_fn)(const char *, void *), void * cdata){
    // References check:
    if (!dmlogger || !cwrite_fn || !cdata) return false;
    
    // Thread-safe on the fly configuration:
    pthread_mutex_lock(&dmlogger->output.mutex);

    // If there is any file opened as output, close:
    if ((dmlogger->output.osel == DMLOGGER_OUTPUT_FILE) && (dmlogger->output.file.fd)) {fclose(dmlogger->output.file.fd); dmlogger->output.file.fd = NULL;}

    // Link custom function and custom data to the custom output selection:
    dmlogger->output.custom.cwrite_fn = cwrite_fn;
    dmlogger->output.custom.cdata = cdata;
    dmlogger->output.osel = DMLOGGER_OUTPUT_CUSTOM;

    // Thread-safe on the fly configuration:
    pthread_mutex_unlock(&dmlogger->output.mutex);

    return true;
}

// ======== CONFIGURATION - QUEUE:
/*
    @brief Function to configure the queue overflow policy.

    @param dmlogger_pt dmlogger: Reference to dmlogger struct.
    @param enum dmlogger_queue_ofpolicy queue_ofpolicy: Queue overflow policy.
    @param unsigned int wait_timeout: Timeout in seconds used when policy is wait with timeout.

    @retval false: If errors ocurred.
    @retval true: If configuration succeeded.
*/
bool dmlogger_conf_queue_ofpolicy(dmlogger_pt dmlogger, enum dmlogger_queue_ofpolicy queue_ofpolicy, unsigned int wait_timeout){
    // Reference check:
    if (!dmlogger) return false;

    // Thread-safe policy update mutex:
    pthread_mutex_lock(&dmlogger->queue.prod_mutex);

    // Policy and timeout write in queue data structure:
    dmlogger->queue.of_policy = queue_ofpolicy;
    dmlogger->queue.wait_timeout = (wait_timeout > 0) ? wait_timeout : DEFAULT_QUEUE_WAIT_TIMEOUT;

    // Thread-safe policy update mutex:
    pthread_mutex_unlock(&dmlogger->queue.prod_mutex);
    return true;
}

/*
    @brief Function to configurate and manage the capacity of the logger queue.

    @param dmlogger_pt dmlogger: Reference to dmlogger struct.
    @param size_t queue_capacity: Capacity of the queue in number of entries.

    @retval false: If errors ocurred.
    @retval true: If configuration succeeded.
*/
bool dmlogger_conf_queue_capacity(dmlogger_pt dmlogger, size_t queue_capacity){
    // Reference check:
    if (!dmlogger) return false;

    // Thread-safe queue config mutex:
    pthread_mutex_lock(&dmlogger->queue.prod_mutex);
    pthread_mutex_lock(&dmlogger->queue.cons_mutex);

    // Capacity update:
    size_t temp_s = (queue_capacity > 1) ? queue_capacity : DEFAULT_QUEUE_CAPACITY;
    void * temp_eq = realloc(dmlogger->queue.equeue, temp_s * sizeof(struct dmlogger_entry));
    if (!temp_eq){pthread_mutex_unlock(&dmlogger->queue.prod_mutex); pthread_mutex_unlock(&dmlogger->queue.cons_mutex); return false;}

    dmlogger->queue.equeue = temp_eq;
    dmlogger->queue.capacity = temp_s;
    // Thread-safe queue config mutex:
    pthread_mutex_unlock(&dmlogger->queue.prod_mutex);
    pthread_mutex_unlock(&dmlogger->queue.cons_mutex);
    return true;
}

// ======== CONFIGURATION - LEVEL:
/*
    @brief Function to set the minimum log level to write.

    @param dmlogger_pt dmlogger: Reference to dmlogger struct.
    @param enum dmlogger_level min_level: Minimum level that the log will write.

    @retval false: If errors ocurred.
    @retval true: If configuration succeeded.
*/
bool dmlogger_conf_logger_minlvl(dmlogger_pt dmlogger, enum dmlogger_level min_level){
    // References check:
    if (!dmlogger) return false;

    // Set the minimum log level:
    dmlogger->min_level = min_level;

    return true;
}


// ======== Log (pritnf-style formatting):
/*
    @brief: Function that logs a message. It includes the entry with the data provided
    into the queue head. The log output write is handled by the consumer thread.

    @param dmlogger_pt dmlogger: Reference to dmlogger.
    @param enum dmlogger_level level: Level of the log.
    @param const char * log_fmsg: String format of the log message. (printf-style formatting)
    @param ...: Arguments used to create log_fmsg. (printf-style formatting)

    @retval None.
*/
void dmlogger_log(dmlogger_pt dmlogger, enum dmlogger_level level, const char * log_fmsg, ...){
    // Parameters pointers check:
    if (!dmlogger || !log_fmsg) return;

    // Minimum level check:
    if (level < dmlogger->min_level) return;

    // Local entry copy:
    struct dmlogger_entry entry;

    // Extract the timestamp formatted string:
    struct timespec ts_info;
    clock_gettime(CLOCK_REALTIME, &ts_info);
    struct tm * tm_info;
    tm_info = localtime(&ts_info.tv_sec);

    strftime(entry.timestamp, DEFAULT_ENTRY_TIMESTAMPLEN, "%Y-%m-%d %H:%M:%S", tm_info);
    sprintf(entry.timestamp + strlen(entry.timestamp), ".%09ld", ts_info.tv_nsec);
    entry.timestamp[DEFAULT_ENTRY_TIMESTAMPLEN - 1] = '\0';

    // Extract level string:
    strncpy(entry.level, DMLOGGER_LEVEL2STRING(level), DEFAULT_ENTRY_LEVELLEN);
    entry.level[DEFAULT_ENTRY_LEVELLEN - 1] = '\0';

    // Extract message formatted string:
    va_list args;
    va_start(args, log_fmsg);
    vsnprintf(entry.message, DEFAULT_ENTRY_MESSAGELEN, log_fmsg, args);
    va_end(args);
    entry.message[DEFAULT_ENTRY_MESSAGELEN - 1] = '\0';

    // Queue management and dump of entry to it:
    pthread_mutex_lock(&dmlogger->queue.prod_mutex);

    if (dmlogger->queue.head == ((dmlogger->queue.tail + 1)%dmlogger->queue.capacity)){
        switch (dmlogger->queue.of_policy){
            case DMLOGGER_OFPOLICY_DROP:
                pthread_mutex_unlock(&dmlogger->queue.prod_mutex); 
                return;
                break;

            case DMLOGGER_OFPOLICY_OVERWRITE:
                pthread_mutex_lock(&dmlogger->queue.cons_mutex);
                dmlogger->queue.head++;
                dmlogger->queue.head %= dmlogger->queue.capacity;                
                pthread_mutex_unlock(&dmlogger->queue.cons_mutex);
                break;

            case DMLOGGER_OFPOLICY_WAIT:
                while (dmlogger->queue.head == ((dmlogger->queue.tail + 1)%dmlogger->queue.capacity)){
                    pthread_cond_wait(&dmlogger->queue.prod_cond, &dmlogger->queue.prod_mutex);
                }
                break;

            case DMLOGGER_OFPOLICY_WAIT_TIMEOUT:
                {
                struct timespec timeout;
                clock_gettime(CLOCK_REALTIME, &timeout);
                timeout.tv_sec += 1;

                while (dmlogger->queue.head == ((dmlogger->queue.tail + 1)%dmlogger->queue.capacity)){
                   if(pthread_cond_timedwait(&dmlogger->queue.prod_cond, &dmlogger->queue.prod_mutex, &timeout)) {pthread_mutex_unlock(&dmlogger->queue.prod_mutex); return;}
                }                
                break;
                }
        }

    }
    dmlogger->queue.equeue[dmlogger->queue.tail++] = entry;
    dmlogger->queue.tail %= dmlogger->queue.capacity;

    pthread_mutex_unlock(&dmlogger->queue.prod_mutex);

    // Signal the consumer thread:
    pthread_cond_signal(&dmlogger->queue.cons_cond);
}

/* ---- Static function implementation ---------------------------- */
/*
    @brief Function that acts as a consumer of the entry queue, single thread logger.

    @param void * args: Reference to dmlogger_pt.

    @retval NULL.
*/
static void * _dmlogger_logger(void * args){
    // Pointer check and dmlogger cast:
    if (!args) return NULL;
    dmlogger_pt dmlogger = (dmlogger_pt)args;

    // Local entry copy:
    struct dmlogger_entry entry;

    // Logger infinite loop:
    while (1){
        // Sync with producers logic:
        pthread_mutex_lock(&dmlogger->queue.cons_mutex);
        while ((dmlogger->queue.head == dmlogger->queue.tail) && (dmlogger->state == DMLOGGER_STATE_RUNNING)){
            pthread_cond_wait(&dmlogger->queue.cons_cond, &dmlogger->queue.cons_mutex);
        }

        if ((dmlogger->state != DMLOGGER_STATE_RUNNING) && (dmlogger->queue.head == dmlogger->queue.tail)) {pthread_mutex_unlock(&dmlogger->queue.cons_mutex); break;}

        // Queue entry management:
        entry = dmlogger->queue.equeue[dmlogger->queue.head++];
        dmlogger->queue.head %= dmlogger->queue.capacity;
        pthread_mutex_unlock(&dmlogger->queue.cons_mutex);

        // Notify producers:
        pthread_cond_signal(&dmlogger->queue.prod_cond);

        // Output file write:
        __dmlogger_logger_write(dmlogger, &entry);

    }

    return NULL;
}

// ======== INTERNAL - OUTPUT:
/*
    @brief Function that writes to the output selected.

    @param dmlogger_pt dmlogger: Reference to dmlogger struct.

    @retval None.
*/
static bool __dmlogger_logger_write(dmlogger_pt dmlogger, struct dmlogger_entry * entry){
    // Check references:
    if (!dmlogger || !entry) return false;

    // Formed log entry:
    char fullmsg[DEFAULT_ENTRY_TIMESTAMPLEN + DEFAULT_ENTRY_LEVELLEN + DEFAULT_ENTRY_MESSAGELEN + DEFAULT_ENTRY_EXTRALEN];
    snprintf(fullmsg, sizeof(fullmsg), "%s | [%s]: %s", entry->timestamp, entry->level, entry->message);

    // Output selection:
    switch (dmlogger->output.osel) {
        case DMLOGGER_OUTPUT_FILE:
            pthread_mutex_lock(&dmlogger->output.mutex);

            // File check:
            if (!dmlogger->output.file.fd) {pthread_mutex_unlock(&dmlogger->output.mutex); return false;}

            // Date check:
            __dmlogger_rotate_file_bydate(dmlogger);

            // Size check:
            __dmlogger_rotate_file_bysize(dmlogger, strlen(fullmsg));

            // File check (also detects file rotation errors):
            if (!dmlogger->output.file.fd) {pthread_mutex_unlock(&dmlogger->output.mutex); return false;}

            // Write to file:
            fprintf(dmlogger->output.file.fd, "%s\n", fullmsg);
            fflush(dmlogger->output.file.fd);
            dmlogger->output.file.size += strlen(fullmsg) + 1;
            pthread_mutex_unlock(&dmlogger->output.mutex);
            break;

        case DMLOGGER_OUTPUT_STDOUT:
        case DMLOGGER_OUTPUT_STDERR:
            pthread_mutex_lock(&dmlogger->output.mutex);
            fprintf(dmlogger->output.stdio.stream, "%s\n", fullmsg);
            fflush(dmlogger->output.stdio.stream);
            pthread_mutex_unlock(&dmlogger->output.mutex);
            break;

        case DMLOGGER_OUTPUT_CUSTOM:
            pthread_mutex_lock(&dmlogger->output.mutex);
            dmlogger->output.custom.cwrite_fn(fullmsg, dmlogger->output.custom.cdata);
            pthread_mutex_unlock(&dmlogger->output.mutex);
            break;
    }

    return true;
}

/*
    @brief Function to generate the log file fullname when using a file as output.

    @param dmlogger_pt dmlogger: Reference to dmlogger struct.
    @param char * ffullname: Reference to the file fullname external buffer.
    @param size_t ffullname_len: Length of the external buffer of file fullname.
    @param struct tm * lt: Reference to local time structure.
*/
static void __dmlogger_gen_file_fullname(dmlogger_pt dmlogger, char * ffullname, size_t ffullname_len, struct tm * lt){
    // Complete file name (path + basename + date + file index):
    char * sep = (dmlogger->output.file.path[strlen(dmlogger->output.file.path) - 1] == '/') ? "" : "/";
    snprintf(ffullname, ffullname_len, "%s%s%s_%04d%02d%02d_%ld.log", dmlogger->output.file.path, sep, dmlogger->output.file.basename, lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, dmlogger->output.file.index);
}

/*
    @brief Function to rotate a log file by date.

    @param dmlogger_pt dmlogger: Reference to dmlogger structure.

    @retval false: If an error ocurred.
    @retval true: If everything is ok.
*/
static bool __dmlogger_rotate_file_bydate(dmlogger_pt dmlogger){
    // Reference check:
    if (!dmlogger) return false;

    // Output file selected check:
    if (dmlogger->output.osel != DMLOGGER_OUTPUT_FILE) return false;

    // Rotate file by date flag set check (if is not set, return as no error!):
    if (!dmlogger->output.file.date_rot) return true;

    // Dates comparison & output update:
    time_t t = time(NULL);
    struct tm lt;
    localtime_r(&t, &lt);

    struct tm prev_lt;
    localtime_r(&dmlogger->output.file.date, &prev_lt);

    if ((lt.tm_year == prev_lt.tm_year) && (lt.tm_mon == prev_lt.tm_mon) && (lt.tm_mday == prev_lt.tm_mday)) return false;
    dmlogger->output.file.date = t; 
    dmlogger->output.file.index = 0;

    // Close current file:
    if (dmlogger->output.file.fd) fclose(dmlogger->output.file.fd);

    // Complete file name (path + basename + date + file index):
    char filename[DEFAULT_OUTPUT_FILE_BASENAMELEN + DEFAULT_OUTPUT_FILE_PATHLEN + DEFAULT_OUTPUT_FILE_EXTRALEN] = {0};
    __dmlogger_gen_file_fullname(dmlogger, filename, sizeof(filename), &lt);

    // Open file:
    dmlogger->output.file.fd = fopen(filename, "a");
    if (!dmlogger->output.file.fd) {return false;}

    return true;
}

/*
    @brief Function to rotate the log file by size.

    @param dmlogger_pt dmlogger: Reference to dmlogger struct.
    @param size_t log_fullmsg_size: Length (in bytes) of the new log message to write into file.

    @retval false: If an error ocurred.
    @retval true: If everything is ok.
*/
static bool __dmlogger_rotate_file_bysize(dmlogger_pt dmlogger, size_t log_fullmsg_size){
    // Reference check:
    if (!dmlogger) return false;

    // Output file selected check:
    if (dmlogger->output.osel != DMLOGGER_OUTPUT_FILE) return false;

    // Rotate file by date flag set check (if is not set, return as no error!):
    if (!dmlogger->output.file.size_rot) return false;

    // Size comparison:
    if ((dmlogger->output.file.size + log_fullmsg_size) <= dmlogger->output.file.max_size) return false;
    dmlogger->output.file.index++;
    dmlogger->output.file.size = 0;

    // Close current file:
    if (dmlogger->output.file.fd) fclose(dmlogger->output.file.fd);

    // Get the current date:
    struct tm lt;
    localtime_r(&dmlogger->output.file.date, &lt);

    // Complete file name (path + basename + date + file index):
    char filename[DEFAULT_OUTPUT_FILE_BASENAMELEN + DEFAULT_OUTPUT_FILE_PATHLEN + DEFAULT_OUTPUT_FILE_EXTRALEN] = {0};
    __dmlogger_gen_file_fullname(dmlogger, filename, sizeof(filename), &lt);

    // Open file:
    dmlogger->output.file.fd = fopen(filename, "a");
    if (!dmlogger->output.file.fd) {return false;}

    return true;
}