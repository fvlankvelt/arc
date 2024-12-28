#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <cjson/cJSON.h>

#include "io.h"
#include "image.h"
#include "task.h"

graph_t* read_graph(cJSON* json_grid) {
    int input_rows = cJSON_GetArraySize(json_grid);
    cJSON* first_row = cJSON_GetArrayItem(json_grid, 0);
    int input_cols = cJSON_GetArraySize(first_row);
    color_t grid[input_cols * input_rows];
    for (int row = 0; row < input_rows; row++) {
        cJSON* json_row = cJSON_GetArrayItem(json_grid, row);
        for (int col = 0; col < input_cols; col++) {
            cJSON* cell = cJSON_GetArrayItem(json_row, col);
            grid[row * input_cols + col] = cJSON_GetNumberValue(cell);
        }
    }
    return graph_from_grid(grid, input_rows, input_cols);
}

task_t* parse_task(const char* source) {
    task_t* task = new_task();

    cJSON* json = cJSON_Parse(source);
    cJSON* train = cJSON_GetObjectItem(json, "train");
    task->n_train = cJSON_GetArraySize(train);
    for (int i_train = 0; i_train < task->n_train; i_train++) {
        cJSON* train_io = cJSON_GetArrayItem(train, i_train);
        cJSON* input = cJSON_GetObjectItem(train_io, "input");
        task->train_input[i_train] = read_graph(input);

        cJSON* output = cJSON_GetObjectItem(train_io, "output");
        task->train_output[i_train] = read_graph(output);
    }

    cJSON* test = cJSON_GetObjectItem(json, "test");
    task->n_test = cJSON_GetArraySize(test);
    for (int i_test = 0; i_test < task->n_test; i_test++) {
        cJSON* test_io = cJSON_GetArrayItem(test, i_test);
        cJSON* input = cJSON_GetObjectItem(test_io, "input");
        task->test_input[i_test] = read_graph(input);

        cJSON* output = cJSON_GetObjectItem(test_io, "output");
        task->test_output[i_test] = read_graph(output);
    }

    cJSON_free(json);

    return task;
}

const char * read_task(const char * name) {
    char filename[128];
    if (strlen(name) < 100) {
        sprintf(filename, "data/%s", name);
    } else {
        return NULL;
    }

    char *source = NULL;
    FILE *fp = fopen(filename, "r");
    if (fp != NULL) {
        /* Go to the end of the file. */
        if (fseek(fp, 0L, SEEK_END) == 0) {
            /* Get the size of the file. */
            long bufsize = ftell(fp);
            if (bufsize == -1) { /* Error */
                exit(-1);
            }

            /* Allocate our buffer to that size. */
            source = malloc(sizeof(char) * (bufsize + 1));

            /* Go back to the start of the file. */
            if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error */
                exit(-1);
            }

            /* Read the entire file into memory. */
            size_t newLen = fread(source, sizeof(char), bufsize, fp);
            if (ferror(fp) != 0) {
                fputs("Error reading file", stderr);
            } else {
                source[newLen++] = '\0'; /* Just to be safe. */
            }
        }
        fclose(fp);
    }

    return source;
}

task_def_t* list_tasks() {
    task_def_t * result = NULL;
    DIR* d = opendir("data");
    if (d) {
        struct dirent* dir;
        while ((dir = readdir(d)) != NULL) {
            char * name = dir->d_name;
            if (strlen(name) < 6 || strncmp(".json", &name[strlen(name) - 5], 5)) {
                continue;
            }
            int len = strlen(name);
            task_def_t * entry = malloc(sizeof(task_def_t) + len + 1);
            entry->next = result;
            entry->name = (char *)(((void *) entry) + sizeof(task_def_t));
            memcpy((char *) entry->name, name, len + 1);
            result = entry;
        }
        closedir(d);
    }
    return result;
}

void free_task_list(task_def_t* list) {
    if (list->next) {
        free_task_list(list->next);
    }
    free(list);
}
