#include "cli.h"

#include "term_colors.h"

#include <ctype.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// structure for defining cli commands
struct CliCommand {
    CliToken_Type ppTok[MAX_TOKEN_N]; // tokens
    uint8_t tokCnt;                   // number of tokens in command without arguments
    uint8_t minArgCnt;                // minimal number of parameters
    char pHelp[MAX_HELP_LEN];         // help line
    char *pHint;                      // pointer to the hint part of the command (allocated on pHelp)
    uint8_t cmdLen;                   // length of the command string
    char *pPadding;                   // padding between command and hint parts (allocated on pHelp)
    fnCliCallback pCB;                // processing callback function
};

#define CLI_MAX_CMD_CNT (48) // limit on number of separate commands
static struct CliCommand spCliCmds[CLI_MAX_CMD_CNT];
static uint8_t sCliCmdCnt;
static bool sCmdsTidy = false;
static bool sInitialized = false; // CLI is initialized
static bool sRun = false;

// ---------------------------

// register and initialize task
void cli_init() {
    // in the beginning there are no registered commands
    sCliCmdCnt = 0;

    // CLI became initialized now!
    sInitialized = true;
}

// ---------------------------

#define CLI_LINEBUF_LENGTH (191)
#define TOK_ARR_LEN (16)

static void tokenize_cli_line(char *pLine, CliToken_Type ppTok[], uint32_t tokMaxLen, uint32_t tokMaxCnt, uint32_t *pTokCnt) {
    uint32_t len = strlen(pLine);

    // copy to prevent modifying original one
    static char pLineCpy[CLI_LINEBUF_LENGTH];
    strcpy(pLineCpy, pLine);

    *pTokCnt = 0;

    // prevent processing if input is empty
    if (len == 0 || tokMaxCnt == 0) {
        return;
    }

    // first token
    char *pTok = strtok(pLineCpy, " ");
    strncpy(&ppTok[0][0], pTok, tokMaxLen);
    (*pTokCnt)++;

    // further tokens
    while ((*pTokCnt < tokMaxCnt) && (pTok != NULL)) {
        pTok = strtok(NULL, " ");

        if (pTok != NULL) {
            strncpy(&ppTok[*pTokCnt][0], pTok, tokMaxLen); // store token
            (*pTokCnt)++;                                  // increment processed token count
        }
    }
}

#define HINT_DELIMITER ('\t')
#define MIN_GAP_IN_SPACES (3)
static uint32_t sHelpPadding = 0;

static void tidy_format_commands() {
    if (sCmdsTidy) {
        return;
    }

    // get maximal line length
    uint32_t i, max_line_length = 0;
    for (i = 0; i < sCliCmdCnt; i++) {
        // fetch command descriptor
        struct CliCommand *pCmd = &spCliCmds[i];

        // if this command is unprocessed
        if (pCmd->pHint == NULL) {
            // search for hint text
            char *pDelim = strchr(pCmd->pHelp, HINT_DELIMITER);

            if (pDelim == NULL) {
                pCmd->cmdLen = strlen(pCmd->pHelp);       // the whole line only contains the command, no hint
                pCmd->pHint = pCmd->pHelp + pCmd->cmdLen; // make the hint point to the terminating zero
            } else {
                // calculate the length of the command part
                pCmd->cmdLen = pDelim - pCmd->pHelp;

                // split help-line
                *pDelim = '\0';           // get command part
                pCmd->pHint = pDelim + 1; // get hint part

                // trim hint
                while (*(pCmd->pHint) <= ' ') {
                    pCmd->pHint++;
                }

                // allocate padding
                pCmd->pPadding = pCmd->pHint + strlen(pCmd->pHint) + 1;
            }
        }

        // update max line length
        max_line_length = MAX(max_line_length, pCmd->cmdLen);
    }

    // fill-in paddings
    for (i = 0; i < sCliCmdCnt; i++) {
        // fetch command descriptor
        struct CliCommand *pCmd = &spCliCmds[i];

        // calculate padding length
        uint8_t padLen = max_line_length - pCmd->cmdLen + MIN_GAP_IN_SPACES;

        // fill-in padding
        for (uint32_t k = 0; k < padLen; k++) {
            pCmd->pPadding[k] = ' ';
        }

        // terminating zero
        pCmd->pPadding[padLen] = '\0';
    }

    sCmdsTidy = true;

    // refresh help line padding
    sHelpPadding = max_line_length - 1 + MIN_GAP_IN_SPACES;
}

#define CLI_HISTORY_CMD "hist"

void process_cli_line(char *pLine) {
    CliToken_Type ppTok[TOK_ARR_LEN];
    uint32_t tokCnt = 0;

    // trim the input
    size_t sz = strlen(pLine);
    while ((sz > 0) && (isspace(pLine[sz - 1]))) {
        pLine[sz - 1] = '\0';
        sz--;
    }

    // tokenize line received from user input
    tokenize_cli_line(pLine, ppTok, MAX_TOK_LEN, TOK_ARR_LEN, &tokCnt);

    if (tokCnt == 0) {
        return;
    }

    int ret = -1;

    // print help
    if (!strcmp(ppTok[0], "?") || !strcmp(ppTok[0], "help")) {
        // tidy-up help if not formatted yet
        tidy_format_commands();

        // ---- BUILT-IN commands ----

        // print "?"
        printf("\n\n" ANSI_COLOR_BYELLOW "?" ANSI_COLOR_CYAN "%*cPrint this help (%d/%d)" ANSI_COLOR_RESET "\n", sHelpPadding, ' ', sCliCmdCnt, CLI_MAX_CMD_CNT);

        // --------------

        uint8_t i;
        for (i = 0; i < sCliCmdCnt; i++) {
            printf(ANSI_COLOR_BYELLOW "%s" ANSI_COLOR_RESET "%s" ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET "\n",
                   spCliCmds[i].pHelp, spCliCmds[i].pPadding, spCliCmds[i].pHint);
        }

        printf("\n\n");

        ret = 0;
    } else {
        // lookup command
        uint8_t i, k = 0, matchCnt = 0;
        int8_t n = -1;
        for (i = 0; i < sCliCmdCnt; i++) {
            matchCnt = 0;

            for (k = 0; k < spCliCmds[i].tokCnt && k < tokCnt; k++) {
                if (strcmp(ppTok[k], spCliCmds[i].ppTok[k]) != 0) {
                    break;
                } else {
                    matchCnt++;
                    if (matchCnt == spCliCmds[i].tokCnt) {
                        n = i;
                        break;
                    }
                }
            }

            if (n != -1) {
                break;
            }
        }

        // call command callback function
        if (n < 0) {
            ret = -1;
        } else {
            struct CliCommand *pCmd = &spCliCmds[n];
            uint8_t argc = tokCnt - pCmd->tokCnt;

            if (argc < pCmd->minArgCnt) {
                printf(ANSI_COLOR_BRED "Insufficient parameters, see help! (" ANSI_COLOR_BYELLOW "?" ANSI_COLOR_BRED ")\n" ANSI_COLOR_RESET);
            } else {
                ret = pCmd->pCB(&ppTok[pCmd->tokCnt], argc);
            }
        }
    }

    if (ret < 0) {
        printf(ANSI_COLOR_BRED "Unknown command or bad parameter: '" ANSI_COLOR_RESET "%s" ANSI_COLOR_BRED
                               "', see help! (" ANSI_COLOR_BYELLOW "?" ANSI_COLOR_BRED ")\n" ANSI_COLOR_RESET,
               pLine);
    }
}

// task routine function
void cli_start() {
    // CLI is running
    sRun = true;

    // notify the world, that CLI is ON!
    printf("Type '" ANSI_COLOR_BYELLOW "?" ANSI_COLOR_RESET "' to display help!\n");

    int len;
    char * lineBuf = NULL;
    size_t lineLen = 0;
    while (sRun) {
        struct pollfd pfd = { .fd = STDIN_FILENO, POLLIN, 0, };
        int pret = poll(&pfd, 1, -1);
        if (pret > 0) {
            getline(&lineBuf, &lineLen, stdin);
            process_cli_line(lineBuf);
        }
    }
    free(lineBuf);
}

int cli_register_command(char *pCmdParsHelp, uint8_t cmdTokCnt,
                         uint8_t minArgCnt, fnCliCallback pCB) {
    // if command storage is full, then return -1;
    if (sCliCmdCnt == CLI_MAX_CMD_CNT) {
        return -1;
    }

    // obtain pointer to first unused command space
    struct CliCommand *pCmd = &spCliCmds[sCliCmdCnt];

    // tokenize the first part of the line (run until cmkTokCnt tokens have been fetched)
    uint32_t tokCnt = 0;
    tokenize_cli_line(pCmdParsHelp, pCmd->ppTok, MAX_TOK_LEN,
                      (cmdTokCnt > TOK_ARR_LEN ? TOK_ARR_LEN : cmdTokCnt), &tokCnt);
    pCmd->tokCnt = (uint8_t)tokCnt;

    // store minimal argument count parameter
    pCmd->minArgCnt = minArgCnt;

    // copy help line
    strncpy(pCmd->pHelp, pCmdParsHelp, MAX_HELP_LEN);

    // zero out hint part (tidy() will fill it)
    pCmd->pHint = NULL;

    // store callback function pointer
    pCmd->pCB = pCB;

    // increase the amount of commands stored
    sCliCmdCnt++;

    // clean up if the same command registered before
    uint8_t i, t;
    int duplicate_idx = -1;
    for (i = 0; i < (sCliCmdCnt - 1); i++) {
        if (spCliCmds[i].tokCnt == cmdTokCnt) {
            for (t = 0; t < cmdTokCnt; t++) {
                if (strcmp(spCliCmds[i].ppTok[t], pCmd->ppTok[t])) {
                    break;
                }
            }

            if (t == cmdTokCnt) {
                duplicate_idx = i;
                break;
            }
        }
    }

    if (duplicate_idx > -1) {
        cli_remove_command(duplicate_idx);
    }

    sCmdsTidy = false; // commands are untidy

    return sCliCmdCnt - 1;
}

void cli_remove_command(int cmdIdx) {
    if (cmdIdx + 1 > sCliCmdCnt || cmdIdx < 0) {
        return;
    }

    uint8_t i;
    for (i = cmdIdx; i < sCliCmdCnt - 1; i++) {
        memcpy(&spCliCmds[i], &spCliCmds[i + 1], sizeof(struct CliCommand));
    }

    sCliCmdCnt--;
}

// removes a bunch of commands, terminated by -1
void cli_remove_command_array(int *pCmdHandle) {
    int *pIter = pCmdHandle;
    while (*pIter != -1) {
        cli_remove_command(*pIter);
        pIter++;
    }
}

void cli_exit() {
    sRun = false;
}

// -----------------------

bool get_param_value(const CliToken_Type *ppArgs, uint8_t argc,
                     const char *pKey, char *pVal) {
    size_t i;
    for (i = 0; i < argc; i++) {
        if (!strncmp(ppArgs[i], pKey, strlen(pKey))) {
            strcpy(pVal, ppArgs[i] + strlen(pKey));
            return true;
        }
    }
    return false;
}