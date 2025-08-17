/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 77d2b9f07e468ef99328deddc0fef328c5dc0c51 */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_phreakscope_start, 0, 0, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, interval_usec, IS_LONG, 0, "10000")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, allocated_bytes, IS_LONG, 0, "1024 * 1024")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_phreakscope_end, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_phreakscope_get_data, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()


ZEND_FUNCTION(phreakscope_start);
ZEND_FUNCTION(phreakscope_end);
ZEND_FUNCTION(phreakscope_get_data);


static const zend_function_entry ext_functions[] = {
	ZEND_FE(phreakscope_start, arginfo_phreakscope_start)
	ZEND_FE(phreakscope_end, arginfo_phreakscope_end)
	ZEND_FE(phreakscope_get_data, arginfo_phreakscope_get_data)
	ZEND_FE_END
};
