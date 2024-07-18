#ifndef MODE_H
#define MODE_H

/**< Set the program in test mode. */
#define TEST

/**< Maximum number of arguments to main */
#define MAX_ARGS 32

/**< Used to distinguish user processes */
#define USERMAGIC 0x2f5e734b

/**< Maximum open file */
#define MAX_FILE 32

/**< Maximum length of file name */
#define MAX_FN_LEN 14

#undef TEST

#endif /**< userprog/mode.h */
