#ifndef _EXCEPTIONS_H_
#define _EXCEPTIONS_H_

	#include "k_stdtype.h"
	#include "fixedmemoryaddress.h"

	typedef struct _ExceptionLog {
		unsigned int _excep_code;
		unsigned int _excep_addr;
		unsigned int magicWord;
	} ExceptionLog;

	extern volatile ExceptionLog exceptionLog[16] __attribute__ ((persistent, address(ADDRESS_exceptionLog)));

	extern void exceptions_clearException(void);
	extern uint32 exceptions_getException(void);

	extern void init_exception(void);
	extern void do_exception_examples(void);

	#ifdef EXCEPTION_TESTING_ENABLED
		extern void test_exception(void);
	#endif


#endif
