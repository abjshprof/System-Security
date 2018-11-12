#include <stdio.h>
#include "pin.H"
#include <iostream>
#include <signal.h>

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "warmup2_malloc.out", "specify output file name");

//==============================================================
//  Analysis Routines
//==============================================================
// Note:  threadid+1 is used as an argument to the PIN_GetLock()
//        routine as a debugging aid.  This is the value that
//        the lock is set to, so it must be non-zero.

// lock serializes access to the output file.
FILE * out;
PIN_LOCK pinLock;
static UINT64 malloc_count = 0;
static UINT64 malloc_size = 0;
static BOOL Finikill(THREADID, INT32, CONTEXT *, BOOL, const EXCEPTION_INFO *, VOID *);

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    PIN_GetLock(&pinLock, threadid+1);
    fprintf(out, "thread begin %d\n",threadid);
    fflush(out);
    PIN_ReleaseLock(&pinLock);
}

// This routine is executed every time a thread is destroyed.
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    PIN_GetLock(&pinLock, threadid+1);
    fprintf(out, "thread end %d code %d\n",threadid, code);
    fprintf(out, "malloc_count  %llu malloc size %llu\n",malloc_count, malloc_size);
    cout << "  malloc calls " << malloc_count << "  total malloc size " << malloc_size <<"\n";
    fflush(out);
    PIN_ReleaseLock(&pinLock);
}

// This routine is executed each time malloc is called.
VOID BeforeMalloc( int size, THREADID threadid , ADDRINT ip)
{
    /*
    PIN_LockClient();
    IMG img = IMG_FindByAddress (ip);
    PIN_UnlockClient();
    */
    PIN_GetLock(&pinLock, threadid+1);
    /*
    if (!IMG_IsMainExecutable(img)){
        PIN_ReleaseLock(&pinLock);
	return;
    }
    */
    malloc_count++;
    malloc_size+=size;
    PIN_ReleaseLock(&pinLock);
}

//====================================================================
// Instrumentation Routines
//====================================================================

// This routine is executed for each image.
VOID ImageLoad(IMG img, VOID *)
{
    RTN rtn = RTN_FindByName(img, "malloc");
    
    //if ( RTN_Valid( rtn ) && (IMG_Name(img).find("ld-linux") == string::npos))
    if ( RTN_Valid( rtn ) && (IMG_Name(img).find("ld-linux") == string::npos))
    {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(BeforeMalloc),
                       IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                       IARG_THREAD_ID, IARG_RETURN_IP, IARG_END);

        RTN_Close(rtn);
    }
}


static BOOL Finikill(THREADID tid, INT32 sig, CONTEXT *, BOOL, const EXCEPTION_INFO *, VOID *)
{

    PIN_GetLock(&pinLock, tid+1);
    fprintf(out, "Got signal: thread %d ended with sig %d\n", tid, sig);
    fprintf(out, "malloc_count  %llu malloc size %llx\n",malloc_count, malloc_size);
    fflush(out);
    PIN_ReleaseLock(&pinLock);
    fclose(out);
    return TRUE;
}

// This routine is executed once at the end.

VOID Fini(INT32 code, VOID *v)
{
    fprintf(out, " malloc called (%llu) times and total size (%llx)\n", malloc_count, malloc_size);
    fclose(out);
}


/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("This Pintool prints a trace of malloc calls in the guest application\n"
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(INT32 argc, CHAR **argv)
{
    // Initialize the pin lock
    PIN_InitLock(&pinLock);
    
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();
    PIN_InitSymbols();
    
    out = fopen(KnobOutputFile.Value().c_str(), "w");

    // Register ImageLoad to be called when each image is loaded.
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Register Analysis routines to be called when a thread begins/ends
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    for (int sig = 1;  sig < SIGRTMIN;  sig++) {
        PIN_InterceptSignal(sig, Finikill, 0);
        PIN_UnblockSignal(sig, TRUE);
    }
 
    // Never returns
    PIN_StartProgram();
 
    return 0;
}
