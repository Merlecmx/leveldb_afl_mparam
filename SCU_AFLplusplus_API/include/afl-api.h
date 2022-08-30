#include "types.h"
#include "debug.h"
#include "alloc-inl.h"
#include "config.h"

#include <stdio.h>
#include "cjson/cJSON.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* Structure of argument entry */
struct argument_entry {
  u8     type;      /* Record every argument's type     */
  s32    len;       /* Record length of init data       */
  cJSON *value;     /* The pointer to struct in cJSON   */
  u8     skip_fuzz; /* If this argument skip fuzzing    */
  s8     name[64];  /* argument's alias                 */
};

struct input_data {
  u8 *data; /* Constructed data                 */
  u32 len;  /* Constructed data length          */
};

/* 
   Structure of api entry which include 
   the number of arguments and their's types etc.
*/
struct api_entry {
  s32                    argument_num;   /* Total number of arguments    */
  cJSON *                monitor_json;   /* Global monitor of the json   */
  struct argument_entry *args_info;	     /* Global argument_entry array  */
  s32                    cur_arg_id;     /* Current arg_id while fuzzing */
};

/* Some arguments for parsing json text */
//s32    argument_num = 0;                 /* Total number of arguments        */
//s32    pre_argument_num = 0;             /* Arg-num of previous sort file    */
//cJSON *monitor_json = NULL;              /* Global monitor of the json       */
//struct argument_entry *args_info = NULL; /* Global argument_entry array      */
//u8 *queue_interest_fn = NULL;            /* Temp filename                    */
//s32 current_arg_id = 0;                  /* Current arg_id while fuzzing     */

/* Type of arguments */
enum {
  /* 00 */ FIXED_DATA,
  /* 01 */ UNFIXED_DATA,
  /* 02 */ STRUCT_DATA,
};
/* Read json text from file */
u8 *read_json_file(u8 *fname);

/* Check the format of json text */
u8 check_format(const char *monitor);

/* A helper function to parse structure data. */
s32 parse_struct(cJSON *args, struct api_entry *api, s32 cnt, s32 g_skip);

/* Parse json text and record some information about arguments */
void parse_json(const char *monitor, struct api_entry *api);

/* Construct input for harness. The input will be written in .cur_input by
 * default */
struct input_data *construct_input(struct api_entry *api);

/* Clean some memory created by api_entry */
void clean_entries(struct api_entry *api);
