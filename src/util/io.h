#ifndef __IO_H__
#define __IO_H__

#include "task.h"

typedef struct _task_def {
    struct _task_def* next;
    const char* name;
    task_t * task;
} task_def_t;

task_def_t* list_tasks();
void free_task_list(task_def_t * list);

const char * read_task(const char * name);
task_t* parse_task(const char* source);

#endif  // __IO_H__