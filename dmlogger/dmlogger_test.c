#include "dmlogger.h"

int main(int argc, char ** argv){
    dmlogger_pt logger;
    dmlogger_init(&logger);
    dmlogger_run(logger);
    dmlogger_conf_logger_minlvl(logger, DMLOGGER_LEVEL_DEBUG);

    dmlogger_conf_queue_capacity(logger, 1);
    dmlogger_conf_queue_ofpolicy(logger, DMLOGGER_OFPOLICY_WAIT_TIMEOUT, 1);
    dmlogger_conf_output_file(logger, "./logs/", "log", false, true, 40);

    dmlogger_log(logger,DMLOGGER_LEVEL_DEBUG, "This is a log message, addres of logger: %p", logger);
    dmlogger_log(logger,DMLOGGER_LEVEL_INFO, "This is a log message, addres of logger: %p", logger);
    dmlogger_log(logger,DMLOGGER_LEVEL_NOTIFY, "This is a log message, addres of logger: %p", logger);
    dmlogger_log(logger,DMLOGGER_LEVEL_WARNING, "This is a log message, addres of logger: %p", logger);
    dmlogger_log(logger,DMLOGGER_LEVEL_ERROR, "This is a log message, addres of logger: %p", logger);
    dmlogger_log(logger,DMLOGGER_LEVEL_FATAL, "This is a log message, addres of logger: %p", logger);

    dmlogger_deinit(&logger);
    return 0;
}