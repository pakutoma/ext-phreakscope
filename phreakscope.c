#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_phreakscope.h"
#include "zend_exceptions.h"
#include "phreakscope_arginfo.h"

#include <pthread.h>

#define PHREAKSCOPE_DEFAULT_INTERVAL 10000 /* 10 ms */
#define PHREAKSCOPE_DEFAULT_ALLOCATED_BYTES (1024 * 1024) /* 1 MB */
#define PHREAKSCOPE_MAX_DEPTH 256

ZEND_DECLARE_MODULE_GLOBALS(phreakscope)

static zend_bool phreakscope_end(void) {
    if (!PHREAKSCOPE_G->enabled) {
        return 0;
    }

    pthread_cancel(PHREAKSCOPE_G->thread_id);

    PHREAKSCOPE_G->enabled = 0;
    return 1;
}

static void *phreakscope_handler(void *data) {
    zend_phreakscope_globals *globals = PHREAKSCOPE_G;
#ifdef ZTS
    volatile zend_executor_globals *eg = TSRMG_BULK(executor_globals_id, zend_executor_globals *);
#else
    volatile zend_executor_globals *eg = &executor_globals;
#endif

    while (1) {
        volatile zend_execute_data *ex = eg->current_execute_data;
        volatile zend_execute_data *start_ex = ex;
        bool is_ex_changed = 0;
        int trace_depth = 0;
        int current_index = globals->last_traces_index ^ 1;

        while (ex) {
            volatile zend_function *func;
            volatile zend_execute_data *prev;

            if (trace_depth >= PHREAKSCOPE_MAX_DEPTH) {
                /* stack is too deep, skip */
                trace_depth = 0;
                break;
            }

            func = ex->func;
            prev = ex->prev_execute_data;

            if (func == NULL) {
                /* skip */
                ex = prev;
                continue;
            }

            if (start_ex != eg->current_execute_data) {
                /* the execution context has changed, restart */
                is_ex_changed = 1;
                break;
            }

            globals->traces_cache[current_index][trace_depth].func = (zend_function *) func;

            trace_depth++;
            ex = prev;
        }
        if (is_ex_changed) {
            continue;
        }

        if (trace_depth == 0) {
            /* do nothing, or the stack is too deep, skip */
            usleep(globals->interval_usec);
            continue;
        }

        int reuse_depth = 0;
        while (reuse_depth < trace_depth && reuse_depth < globals->last_traces_depth) {
            trace_frame_t *current_frame = &globals->traces_cache[current_index][reuse_depth];
            trace_frame_t *last_frame = &globals->traces_cache[globals->last_traces_index][reuse_depth];
            if (current_frame->func != last_frame->func) {
                break;
            }
            reuse_depth++;
        }

        if (sizeof(trace_entry_t) + sizeof(trace_frame_t) * (trace_depth - reuse_depth) > globals->remaining_bytes) {
            /* Doing a realloc within a profiling thread is unsafe, end */
            globals->enabled = 0;
            break;
        }

        globals->current_trace->depth = trace_depth;
        globals->current_trace->reuse_depth = reuse_depth;
        trace_frame_t *current_frames = (trace_frame_t *) (globals->current_trace + 1);
        for (int i = 0; i < trace_depth - reuse_depth; i++) {
            current_frames[i] = globals->traces_cache[current_index][(trace_depth - reuse_depth) - (i + 1)];
        }

        globals->last_traces_depth = trace_depth;
        globals->last_traces_index ^= 1;
        globals->remaining_bytes -= sizeof(trace_entry_t) + sizeof(trace_frame_t) * (trace_depth - reuse_depth);
        globals->current_trace = (trace_entry_t *) (
            (uint8_t *) globals->current_trace
            + sizeof(trace_entry_t)
            + sizeof(trace_frame_t) * (trace_depth - reuse_depth)
        );

        usleep(globals->interval_usec);
    }

    pthread_exit(NULL);
}

static void phreakscope_start(long interval_usec, size_t allocated_bytes) {
    zend_phreakscope_globals *globals = PHREAKSCOPE_G;

    if (globals->enabled) {
        zend_throw_exception(NULL, "Phreakscope is already running", 0);
        return;
    }

    if (globals->entries) {
        efree(globals->entries);
        efree(globals->traces_cache[0]);
        efree(globals->traces_cache[1]);
        globals->entries = NULL;
        globals->traces_cache[0] = NULL;
        globals->traces_cache[1] = NULL;
    }

    globals->interval_usec = interval_usec;
    globals->entries = safe_emalloc(allocated_bytes, sizeof(uint8_t), 0);
    globals->remaining_bytes = allocated_bytes;
    globals->current_trace = globals->entries;
    globals->traces_cache[0] = safe_emalloc(PHREAKSCOPE_MAX_DEPTH, sizeof(trace_frame_t), 0);
    globals->traces_cache[1] = safe_emalloc(PHREAKSCOPE_MAX_DEPTH, sizeof(trace_frame_t), 0);
    globals->last_traces_index = 0;
    globals->last_traces_depth = 0;

    if (pthread_create(&globals->thread_id, NULL, phreakscope_handler, NULL)) {
        zend_throw_exception(NULL, "Could not create profiling thread", 0);
        return;
    }

    globals->enabled = 1;
}

PHP_FUNCTION(phreakscope_start) {
    zend_long interval_usec = 0;
    zend_long allocated_bytes = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|ll", &interval_usec, &allocated_bytes) == FAILURE) {
        return;
    }

    if (interval_usec < 0) {
        zend_throw_exception(NULL, "Number of microseconds can't be negative", 0);
        return;
    }
    if (interval_usec == 0) {
        interval_usec = PHREAKSCOPE_DEFAULT_INTERVAL;
    }

    if (allocated_bytes < 0) {
        zend_throw_exception(NULL, "Number of profiling can't be negative", 0);
        return;
    }
    if (allocated_bytes == 0) {
        allocated_bytes = PHREAKSCOPE_DEFAULT_ALLOCATED_BYTES;
    }

    phreakscope_start(interval_usec, allocated_bytes);
}

PHP_FUNCTION(phreakscope_end) {
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    RETURN_BOOL(phreakscope_end());
}

PHP_FUNCTION(phreakscope_get_data) {
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    if (!PHREAKSCOPE_G->entries) {
        return;
    }

    zend_phreakscope_globals *globals = PHREAKSCOPE_G;
    trace_entry_t *entry = globals->entries;
    trace_entry_t *end = globals->current_trace;
    trace_frame_t *prev_frames = safe_emalloc(PHREAKSCOPE_MAX_DEPTH, sizeof(trace_frame_t), 0);
    array_init(return_value);

    while (entry < end) {
        uint8_t depth = entry->depth;
        uint8_t reuse_depth = entry->reuse_depth;
        trace_frame_t *frames = (trace_frame_t *) (entry + 1);
        zval trace_array;
        array_init(&trace_array);

        for (int i = 0; i < depth; i++) {
            zval frame_array;
            array_init(&frame_array);
            trace_frame_t *frame;

            if (i < reuse_depth) {
                frame = &prev_frames[i];
            } else {
                frame = &frames[i - reuse_depth];
                prev_frames[i] = *frame;
            }

            uint8_t type = frame->func ? frame->func->type : 0;
            if (type == ZEND_USER_FUNCTION && frame->func->op_array.function_name) {
                add_next_index_str(&frame_array, zend_string_copy(frame->func->op_array.function_name));
            } else if (type == ZEND_INTERNAL_FUNCTION && frame->func->internal_function.function_name) {
                add_next_index_str(&frame_array, zend_string_copy(frame->func->internal_function.function_name));
            } else if (frame->func) {
                add_next_index_string(&frame_array, "{main}");
            } else {
                add_next_index_string(&frame_array, "{unknown function}");
            }

            if (type == ZEND_USER_FUNCTION && frame->func->op_array.filename) {
                add_next_index_str(&frame_array, zend_string_copy(frame->func->op_array.filename));
            } else if (type == ZEND_INTERNAL_FUNCTION) {
                add_next_index_string(&frame_array, "{internal}");
            } else {
                add_next_index_string(&frame_array, "{unknown file}");
            }

            if (type == ZEND_USER_FUNCTION) {
                add_next_index_long(&frame_array, frame->func->op_array.line_start);
            } else {
                add_next_index_long(&frame_array, 0);
            }

            add_next_index_zval(&trace_array, &frame_array);
        }

        add_next_index_zval(return_value, &trace_array);
        entry = (trace_entry_t *) (
            (uint8_t *) entry + sizeof(trace_entry_t) + sizeof(trace_frame_t) * (depth - reuse_depth)
        );
    }

    efree(prev_frames);
}

PHP_RINIT_FUNCTION(phreakscope) {
#if defined(ZTS) && defined(COMPILE_DL_PHREAKSCOPE)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    PHREAKSCOPE_G->enabled = 0;
    PHREAKSCOPE_G->entries = NULL;
    PHREAKSCOPE_G->thread_id = 0;
    PHREAKSCOPE_G->traces_cache[0] = NULL;
    PHREAKSCOPE_G->traces_cache[1] = NULL;
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(phreakscope) {
    phreakscope_end();
    if (PHREAKSCOPE_G->entries) {
        efree(PHREAKSCOPE_G->entries);
        efree(PHREAKSCOPE_G->traces_cache[0]);
        efree(PHREAKSCOPE_G->traces_cache[1]);
        PHREAKSCOPE_G->entries = NULL;
        PHREAKSCOPE_G->traces_cache[0] = NULL;
        PHREAKSCOPE_G->traces_cache[1] = NULL;
    }

    return SUCCESS;
}

PHP_MINFO_FUNCTION(phreakscope) {
    php_info_print_table_start();
    php_info_print_table_row(2, "phreakscope support", "enabled");
    php_info_print_table_end();
}

zend_module_entry phreakscope_module_entry = {
    STANDARD_MODULE_HEADER,
    "phreakscope",
    ext_functions,
    NULL,
    NULL,
    PHP_RINIT(phreakscope),
    PHP_RSHUTDOWN(phreakscope),
    PHP_MINFO(phreakscope),
    PHP_PHREAKSCOPE_VERSION,
    PHP_MODULE_GLOBALS(phreakscope),
    NULL,
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_PHREAKSCOPE
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(phreakscope)
#endif
