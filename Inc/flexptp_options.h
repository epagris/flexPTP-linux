#ifndef FLEXPTP_OPTIONS_H
#define FLEXPTP_OPTIONS_H

// -------------------------------------------
// ------ DEFINES FOR FLEXPTP SETTINGS -------
// -------------------------------------------

#define FLEXPTP_LINUX
#define PTP_HLT_INTERFACE
#define PTP_HEARTBEAT_TICKRATE_MS (62)

#define ANNOUNCE_COLLECTION_WINDOW (2)

// -------------------------------------------
// --- DEFINES FOR PORTING IMPLEMENTATION ----
// -------------------------------------------

// Give a printf-like printing implementation MSG(...)
// Give a maskable printing implementation CLILOG(en,...)
// Provide an SPRINTF-implementation SPRINTF(str,n,fmt,...)

#include <stdio.h>

#define MSG(...) printf(__VA_ARGS__)

#define CLILOG(en, ...)       \
    {                         \
        if (en)               \
        printf(__VA_ARGS__); \
    }

#define FLEXPTP_SNPRINTF(...) snprintf(__VA_ARGS__)

// Include hardware port files and fill the defines below to port the PTP stack to a physical hardware:
// - PTP_HW_INIT(increment, addend): function initializing timestamping hardware
// - PTP_MAIN_OSCILLATOR_FREQ_HZ: clock frequency fed into the timestamp unit [Hz]
// - PTP_INCREMENT_NSEC: hardware clock increment [ns]
// - PTP_SET_ADDEND(addend): function writing hardware clock addend register

#include <flexptp/port/example_netstack_drivers/nsd_linux.h>

extern char ptp_if_name[];
#define PTP_HW_INIT() if (!linux_nsd_preinit(ptp_if_name)) { exit(0); }
#define PTP_SET_CLOCK(s, ns) linux_set_time((s), (ns))
#define PTP_SET_TUNING(tuning_ppb) linux_adjust_clock(tuning_ppb)
#define PTP_HW_GET_TIME(pt) linux_get_time(pt)

// Include the clock servo (controller) and define the following:
// - PTP_SERVO_INIT(): function initializing clock servo
// - PTP_SERVO_DEINIT(): function deinitializing clock servo
// - PTP_SERVO_RESET(): function reseting clock servo
// - PTP_SERVO_RUN(d): function running the servo, input: master-slave time difference (error), return: clock tuning value in PPB
//

#include <flexptp/servo/kalman_filter.h>

#define PTP_SERVO_INIT() kalman_filter_init()
#define PTP_SERVO_DEINIT() kalman_filter_deinit()
#define PTP_SERVO_RESET() kalman_filter_reset()
#define PTP_SERVO_RUN(d, pscd) kalman_filter_run(d, pscd)

//#include <flexptp/servo/pid_controller.h>
//
//#define PTP_SERVO_INIT() pid_ctrl_init()
//#define PTP_SERVO_DEINIT() pid_ctrl_deinit()
//#define PTP_SERVO_RESET() pid_ctrl_reset()
//#define PTP_SERVO_RUN(d, pscd) pid_ctrl_run(d, pscd)


// Optionally add interactive, tokenizing CLI-support
// - CLI_REG_CMD(cmd_hintline,n_cmd,n_min_arg,cb): function for registering CLI-commands
//      cmd_hintline: text line printed in the help beginning with the actual command, separated from help text by \t charaters
//      n_cmd: number of tokens (words) the command consists of
//      n_arg: minimal number of arguments must be passed with the command
//      cb: callback function cb(const CliToken_Type *ppArgs, uint8_t argc)
//  return: cmd id (can be null, if discarded)

#include <cli/cli.h>

#define CLI_REG_CMD(cmd_hintline, n_cmd, n_min_arg, cb) cli_register_command(cmd_hintline, n_cmd, n_min_arg, cb)

// -------------------------------------------

#define PTP_ENABLE_MASTER_OPERATION (1) // Enable or disable master operations

// Static fields of the Announce message
#define PTP_CLOCK_PRIORITY1 (128)                      // priority1 (0-255)
#define PTP_CLOCK_PRIORITY2 (128)                      // priority2 (0-255)
#define PTP_BEST_CLOCK_CLASS (PTP_CC_DEFAULT)          // best clock class of this device
#define PTP_WORST_ACCURACY (PTP_CA_UNKNOWN)            // worst accuracy of the clock
#define PTP_TIME_SOURCE (PTP_TSRC_INTERNAL_OSCILLATOR) // source of the distributed time

// -------------------------------------------

#include "flexptp/event.h"
extern void flexptp_user_event_cb(PtpUserEventCode uev);
#define PTP_USER_EVENT_CALLBACK flexptp_user_event_cb

#endif //FLEXPTP_OPTIONS_H
