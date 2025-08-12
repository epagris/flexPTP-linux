#include <stdio.h>

#include "cli/cli.h"
#include "cli/term_colors.h"

#include "flexptp/logging.h"
#include "flexptp/profiles.h"
#include "flexptp/ptp_profile_presets.h"
#include "flexptp/settings_interface.h"
#include "flexptp/task_ptp.h"

#include <signal.h>
#include <ctype.h>
#include <getopt.h>
#include <inttypes.h>
#include <net/if.h>
#include <signal.h>
#include <stdlib.h>

// ------------------------------

char ptp_if_name[IFNAMSIZ] = {0};
static char config_fname[FILENAME_MAX] = {0};

void flexptp_user_event_cb(PtpUserEventCode uev) {
    switch (uev) {
    case PTP_UEV_INIT_DONE:
        // some initial settings
        ptp_load_profile(ptp_profile_preset_get("gPTP"));
        ptp_log_enable(PTP_LOG_DEF, true);
        ptp_log_enable(PTP_LOG_BMCA, true);

        // load config file if defined
        if (config_fname[0] != '\0') {
            FILE *fp = fopen(config_fname, "r");
            if (fp != NULL) {
                printf(ANSI_ITALIC "Loading config...\n\n" ANSI_COLOR_RESET);
                char * lineBuf = NULL;
                size_t lineLen = 0;
                do {
                    getline(&lineBuf, &lineLen, fp);
                    size_t offset = 0;
                    while ((lineBuf[offset] != '\0') && isspace(lineBuf[offset])) {
                        offset++;
                    }
                    char * lineStart = lineBuf + offset;
                    if (lineStart[0] != '#') {
                        printf(ANSI_ITALIC ANSI_COLOR_GREEN "\n> %s\n" ANSI_COLOR_RESET, lineStart);
                        process_cli_line(lineStart);
                    }
                } while (!feof(fp));
                free(lineBuf);
                fclose(fp);
                printf(ANSI_ITALIC "\n...done!\n" ANSI_COLOR_RESET);
            }
        }

        break;
    default:
        break;
    }
}

// ------------------------------

static void print_help() {
    printf("-i, --interface=<interface> (required)\n"
           "-c, --config=<config file>\n");
};

static struct option interface_opt[] = {
    {"interface", required_argument, 0, 'i'},
    {"config", required_argument, 0, 'c'},
    {0}};

void parse_interface(int argc, char *argv[]) {
    // get options
    int option_index = 0;

    // iterate over parameters
    int c;
    while ((c = getopt_long(argc, argv, "i:c:", interface_opt, &option_index)) != -1) {
        switch (c) {
        case 'i':
            strncpy(ptp_if_name, optarg, IFNAMSIZ);
            break;
        case 'c':
            strncpy(config_fname, optarg, FILENAME_MAX);
            break;
        default:
            break;
        }
    }

    // verify required options
    if (ptp_if_name[0] == '\0') {
        print_help();
        exit(0);
    }
}

static CMD_FUNCTION(exit_application) {
    printf("Exiting...\n\n");
    cli_exit();
    return 0;
}

static void sighandler(int signum) {
    cli_exit();
}

int main(int argc, char *argv[]) {
    // register SIGINT signal handler
    struct sigaction sa;
    sa.sa_handler = sighandler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    // parse options
    parse_interface(argc, argv);

    // initialize the CLI
    cli_init();

    // register exit command
    cli_register_command("exit \t\t\t Exit application", 1, 0, exit_application);

    // register PTP task
    reg_task_ptp();

    // start the CLI
    cli_start();

    // unregister PTP task
    unreg_task_ptp();

    return 0;
}

// -------------------

