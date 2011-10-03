//CL61v00 (p)Marie

        
	RSEG	SWILIB_FUNC2EE_2F5:CODE
	EXTERN	sub_elfclose
	EXTERN	dlopen
	EXTERN	dlsym
        EXTERN	dlclose
        EXTERN	setenv
        EXTERN	unsetenv
        EXTERN	getenv
        EXTERN	clearenv
        EXTERN  environ
	EXTERN  dlerror
        EXTERN  dlclean_cache
        EXTERN  SHARED_TOP
        
	DCD	sub_elfclose
	DCD	dlopen
	DCD	dlsym
        DCD     dlclose
        DCD	setenv
	DCD	unsetenv
        DCD     getenv
        DCD     clearenv
        DCD	environ
        DCD	dlerror
        DCD	dlclean_cache
	DCD	SHARED_TOP


	RSEG	DATA_N
	RSEG	SWILIB_FUNC1B8_1BB:CODE
	EXTERN	EXT2_AREA
	EXTERN	pngtop
	EXTERN	pLIB_TOP
	DCD	EXT2_AREA
	DCD	pngtop
	DCD	pLIB_TOP
	DCD	SFE(DATA_N)
        


defadr	MACRO	a,b
	PUBLIC	a
a	EQU	b
	ENDM

        RSEG	CODE:CODE


	PUBLIC	OldOnClose
OldOnClose:
	DCD	0xA07E3740+1
	
	PUBLIC	OldOnCreate
OldOnCreate:
	DCD	0xA07E34C8+1
	
	PUBLIC	OldShowMsg
OldShowMsg:
	DCD	0xA0AE321C+1

	PUBLIC	PITgetN
PITgetN:
	DCD	0xA0B6E5C4
	
	PUBLIC	PITret
PITret:
	DCD	0xA046B654+1
	
	defadr	StoreErrInfoAndAbort,0xA0737838
	defadr	StoreErrString,0xA0737704


	END