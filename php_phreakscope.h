/* phreakscope extension for PHP */

#ifndef PHP_PHREAKSCOPE_H
# define PHP_PHREAKSCOPE_H

extern zend_module_entry phreakscope_module_entry;
# define phpext_phreakscope_ptr &phreakscope_module_entry

# define PHP_PHREAKSCOPE_VERSION "0.1.0"

#ifdef ZTS
#include "TSRM.h"
#endif

# if defined(ZTS) && defined(COMPILE_DL_PHREAKSCOPE)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

typedef struct __attribute__((packed)) {
    zend_function *func;
} trace_frame_t;

/* Size of trace_entry_t is 2 bytes + sizeof(trace_frame_t) * (depth - reuse_depth) */
typedef struct __attribute__((packed)) {
    uint8_t depth;
    uint8_t reuse_depth;
    /* trace_frame_t frames[(depth - reuse_depth)] */
} trace_entry_t;

ZEND_BEGIN_MODULE_GLOBALS(phreakscope)
    zend_bool enabled;
    uint32_t interval_usec;
    pthread_t thread_id;
    trace_entry_t *entries;
    size_t remaining_bytes;
    trace_entry_t *current_trace;
    trace_frame_t *traces_cache[2];
    uint8_t last_traces_index;
    uint8_t last_traces_depth;
ZEND_END_MODULE_GLOBALS(phreakscope)

#define PHREAKSCOPE_G ZEND_MODULE_GLOBALS_BULK(phreakscope)

#endif	/* PHP_PHREAKSCOPE_H */
