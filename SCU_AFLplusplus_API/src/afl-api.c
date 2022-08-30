#include "afl-api.h"

u8 *read_json_file(u8 *fname) {
  
  struct stat st;

  if (lstat(fname, &st) || access(fname, R_OK))
    PFATAL("Unable to access '%s'", fname);

  u32 len = st.st_size;

  u8 *json_text = malloc(len + 1);

  s32 fd = open(fname, O_RDONLY);

  if (fd < 0) PFATAL("Unable to open '%s'", fname);

  read(fd, json_text, len);

  close(fd);

  json_text[len] = 0;

  return json_text;
}

u8 check_format(const char *monitor) {
  cJSON *num = NULL;
  cJSON *argument = NULL;
  cJSON *arguments = NULL;
  cJSON *monitor_json = cJSON_Parse(monitor);
  if (!monitor_json) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr)
      PFATAL("There is something wrong with json file. Error before: %s",
             error_ptr);
    PFATAL("There is something wrong happened while parsing json");
  }
  num = cJSON_GetObjectItemCaseSensitive(monitor_json, "num");
  if (!cJSON_IsNumber(num) || !num->valueint) {
    PFATAL("Cannot resolve 'num' in json file");
  }
  int arg_num = num->valueint;
  arguments = cJSON_GetObjectItemCaseSensitive(monitor_json, "arguments");
  int cnt = 0;
  cJSON_ArrayForEach(argument, arguments) {
    cnt++;
    cJSON *type = cJSON_GetObjectItemCaseSensitive(argument, "type");
    cJSON *value = cJSON_GetObjectItemCaseSensitive(argument, "value");
    cJSON *context = cJSON_GetObjectItemCaseSensitive(argument, "context");
    cJSON *skip = cJSON_GetObjectItemCaseSensitive(argument, "skip");
    if (skip && !cJSON_IsBool(skip)) {
      PFATAL("Cannot resolve %dth argument of 'skip' in json file", cnt);
    }
    char *typeValue = type->valuestring;
    if (!cJSON_IsString(type) || typeValue == NULL ||
        (strcmp(typeValue, "unfixed") &&
         strstr(typeValue, "fixed_") != typeValue) &&
            strcmp(typeValue, "struct")) {
      PFATAL("Cannot resolve %dth argument of 'type' in json file", cnt);
    }
    if (!cJSON_IsBool(context)) {
      PFATAL("Cannot resolve %dth argument of 'context' in json file", cnt);
    }

    if (!strcmp(typeValue, "struct")) {
      /* do something.... */

    } else if (strstr(typeValue, "fixed_") == typeValue &&
               strlen(typeValue) < 9) {
      char *t = strchr(typeValue, '_') + 1;
      int   type_len = 0;
      while (*t != '\0') {
        type_len = type_len * 10 + *t - '0';
        t++;
      }
      if (type_len < 8 || (type_len & (type_len - 1)) != 0) {
        PFATAL("Cannot resolve %dth argument of 'type=fixed_%d' in json file",
               cnt, type_len);
      }
      char *type_value = value->valuestring;
      while (*type_value != '\0') {
        if (isdigit(*type_value) == 0) {
          PFATAL("Cannot resolve %dth argument of 'value=%s' in json file", cnt,
                 value->valuestring);
        }
        type_value++;
      }
    } else if (!strcmp(typeValue, "unfixed")) {
      if (!cJSON_IsString(value)) {
        PFATAL("Cannot resolve %dth argument of 'value' in json file", cnt);
      }
      if (strlen(typeValue) == 0) {
        PFATAL("The value of unfixed data should be not empty string");
      }
    } else {
      PFATAL("Cannot resolve %dth argument of 'type=%s' in json file", cnt,
             typeValue);
    }
  }
  if (cnt != arg_num) {
    PFATAL("The num=%d is not equal to arguments's length=%d", arg_num, cnt);
  }

  cJSON_Delete(monitor_json);
  return 0;
}

s32 parse_struct(cJSON *args, struct api_entry *api, s32 cnt, s32 g_skip) {
  s32    i = 0;
  cJSON *arg = NULL;

  /* some duplication with the function parseJSON(), whatever~ */
  cJSON_ArrayForEach(arg, args) {
    if (i != 0) {
      api->args_info = (struct argument_entry *)ck_realloc(
          api->args_info,
          sizeof(struct argument_entry) * (++api->argument_num));
    }
    cJSON *value = cJSON_GetObjectItemCaseSensitive(arg, "value");
    char * typeValue =
        cJSON_GetObjectItemCaseSensitive(arg, "type")->valuestring;
    cJSON *skip = cJSON_GetObjectItemCaseSensitive(arg, "skip");
    const s8 *name = cJSON_GetObjectItemCaseSensitive(arg, "name")->valuestring;
    if (!skip) {
      /* If the key skip is not defined, define it with the value of outer
       * structure. */
      cJSON *skipped = (g_skip == 0) ? cJSON_CreateFalse() : cJSON_CreateTrue();
      cJSON_AddItemToObject(arg, "skip", skipped);
    }
    skip = cJSON_GetObjectItemCaseSensitive(arg, "skip");

    if (name) {
      strncpy(api->args_info[cnt].name, name,
              sizeof(api->args_info[cnt].name) - 1);
      api->args_info[cnt].name[sizeof(api->args_info[cnt].name) - 1] = 0;
    } else {
      api->args_info[cnt].name[0] = 0;
    }

    if (!strcmp(typeValue, "struct")) {
      cnt = parse_struct(value, api, cnt, skip->type == 1 ? 0 : 1);
    } else if (strstr(typeValue, "fixed_") == typeValue &&
               strlen(typeValue) < 9) {
      char *t = typeValue + 6;

      u8 type_len = 0;
      while (*t != '\0') {
        type_len = type_len * 10 + *t - '0';
        t++;
      }

      api->args_info[cnt].type = FIXED_DATA;
      api->args_info[cnt].len = type_len / 8;
      api->args_info[cnt].value = value;

    } else {
      api->args_info[cnt].type = UNFIXED_DATA;
      char *str_t = value->valuestring;

      api->args_info[cnt].len = strlen(str_t);
      api->args_info[cnt].value = value;
    }
    cnt++;
    i++;
  }
  return cnt - 1;
}

void parse_json(const char *monitor, struct api_entry* api) {
  /* First, check if json file's format is legal. */
  if (!check_format(monitor)) {
    /* Save a copy of cjson structure */
    api->monitor_json = cJSON_Parse(monitor);
    cJSON *args =
        cJSON_GetObjectItemCaseSensitive(api->monitor_json, "arguments");
    cJSON *arg = NULL;

    api->argument_num =
        cJSON_GetObjectItemCaseSensitive(api->monitor_json, "num")->valueint;
    api->args_info = (struct argument_entry *)ck_alloc(
        sizeof(struct argument_entry) * api->argument_num);
    api->cur_arg_id = 0;

    s32 cnt = 0;
    cJSON_ArrayForEach(arg, args) {
      u8 *typeValue =
          cJSON_GetObjectItemCaseSensitive(arg, "type")->valuestring;
      cJSON *skip = cJSON_GetObjectItemCaseSensitive(arg, "skip");
      cJSON *value = cJSON_GetObjectItemCaseSensitive(arg, "value");
      const s8 *name =
          cJSON_GetObjectItemCaseSensitive(arg, "name")->valuestring;
      if (!skip) {
        /* If the key 'skip' is not defined, just define it with default value
         * false */
        cJSON *skipped = cJSON_CreateFalse();
        cJSON_AddItemToObject(arg, "skip", skipped);
      }

      skip = cJSON_GetObjectItemCaseSensitive(arg, "skip");
      api->args_info[cnt].skip_fuzz = skip->type == 1 ? 0 : 1;
      api->args_info[cnt].len = 0;

      if (name) {
        strncpy(api->args_info[cnt].name, name,
                sizeof(api->args_info[cnt].name) - 1);
        api->args_info[cnt].name[sizeof(api->args_info[cnt].name) - 1] = 0;
      } else {
        api->args_info[cnt].name[0] = 0;
      }

      if (!strcmp(typeValue, "struct")) {
        cnt = parse_struct(value, api, cnt, api->args_info[cnt].skip_fuzz);
      } else if (strstr(typeValue, "fixed_") == typeValue &&
                 strlen(typeValue) < 9) {
        char *t = typeValue + 6;

        u8 type_len = 0;
        while (*t != '\0') {
          type_len = type_len * 10 + *t - '0';
          t++;
        }

        api->args_info[cnt].type = FIXED_DATA;
        api->args_info[cnt].len = type_len / 8;
        api->args_info[cnt].value = value;

      } else {
        api->args_info[cnt].type = UNFIXED_DATA;
        char *str_t = value->valuestring;

        api->args_info[cnt].len = strlen(str_t);
        api->args_info[cnt].value = value;
      }
      cnt++;
    }
  }
}

struct input_data *construct_input(struct api_entry *api) {
  /* After use this function, please free the return value in time in case heap
   * overflow */
  if (!api->monitor_json) return NULL;
  /* Pre-read the length of string, automatically malloc enough space */
  u32 max_len = 0;
  for (s32 i = 0; i < api->argument_num; i++) {
    max_len += strlen(api->args_info[i].value->valuestring);
    max_len += strlen(api->args_info[i].name);
  }

  max_len = max_len + api->argument_num * 3; /* the length of "=\r\n" */

  u8 *res_data = ck_alloc(max_len + 1);

  struct input_data *res = ck_alloc(sizeof(struct input_data));
  res->data = res_data;
  res->len = max_len;

  u32 idx = 0;
  for (s32 i = 0; i < api->argument_num; i++) {
    u8 *name = api->args_info[i].name;
    for (s32 j = 0; j < strlen(name); j++) {
      res_data[idx++] = name[j];
    }
    res_data[idx++] = '=';

    u8 *tmp = api->args_info[i].value->valuestring;
    for (s32 j = 0; j < strlen(tmp); j++) {
      res_data[idx++] = tmp[j];
    }
    res_data[idx++] = '\r';
    res_data[idx++] = '\n';
  }

  res_data[max_len] = 0;
  return res;
}

void clean_entries(struct api_entry *api) {
  /* do some cleaning job~ */
  ck_free(api->args_info);

  cJSON_Delete(api->monitor_json);

  ck_free(api);
}

