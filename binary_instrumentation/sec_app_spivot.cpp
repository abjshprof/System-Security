#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cctype>
#include <map>
#include "pin.H"
#include <signal.h>

PIN_LOCK pinLock;
// Virtual register we use to point to each thread's TINFO structure.
//
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "sec_app_spivot.out", "specify output file name");
static REG RegTinfo;
FILE * out;
// Information about each thread.
//

struct TINFO
{
    TINFO(ADDRINT base) : _stackBase(base), _max(0), _maxReported(0), _tid(-1) {}
    ADDRINT _stackBase;     // Base (highest address) of stack.
    size_t _max;            // Maximum stack usage so far.
    size_t _maxReported;    // Maximum stack usage reported at breakpoint.
    THREADID _tid;
    std::ostringstream _os; // Used to format messages.
};

typedef std::map<THREADID, TINFO *> TINFO_MAP;
static TINFO_MAP ThreadInfos;

static VOID OnThreadStart(THREADID, CONTEXT *, INT32, VOID *);
static VOID OnThreadEnd(THREADID, const CONTEXT *, INT32, VOID *);
static VOID Instruction(INS, VOID *);
static ADDRINT BeforeRet(ADDRINT sp, ADDRINT bp,  ADDRINT addrInfo, ADDRINT ip);
//static ADDRINT Aftersp_write(ADDRINT sp, ADDRINT bp,  ADDRINT addrInfo, ADDRINT ip,  char *name);
static ADDRINT BeforeCall(ADDRINT sp, ADDRINT bp,  ADDRINT addrInfo, ADDRINT ip);


/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool demonstrates the use of extended debugger commands" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}


VOID Fini(INT32 code, VOID *v)
{
    fprintf(out, "\n===DONE=====\n");
    fflush(out);
    fclose(out);
}


static BOOL Finikill(THREADID tid, INT32 sig, CONTEXT *, BOOL, const EXCEPTION_INFO *, VOID *)
{

    PIN_GetLock(&pinLock, tid+1);
    fprintf(out, "Got signal: thread %d ended with sig %d\n", tid, sig);
    //fprintf(out, "\nTotal Max stack_usage of all threads is 0x%x bytes\n", max_stack);
    fflush(out);
    PIN_ReleaseLock(&pinLock);
    fclose(out);
    return TRUE;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    out = fopen(KnobOutputFile.Value().c_str(), "w");
    PIN_InitLock(&pinLock);
    // Allocate a virtual register that each thread uses to point to its
    // TINFO data.  Threads can use this virtual register to quickly access
    // their own thread-private data.
    //
    RegTinfo = PIN_ClaimToolRegister();
    if (!REG_valid(RegTinfo))
    {
        std::cerr << "Cannot allocate a scratch register.\n";
        std::cerr << std::flush;
        return 1;
    }
    PIN_AddThreadStartFunction(OnThreadStart, 0);
    PIN_AddThreadFiniFunction(OnThreadEnd, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    for (int sig = 1;  sig < SIGRTMIN;  sig++) {
        PIN_InterceptSignal(sig, Finikill, 0);
        PIN_UnblockSignal(sig, TRUE);
    }

    PIN_StartProgram();
    return 0;
}

static VOID OnThreadStart(THREADID tid, CONTEXT *ctxt, INT32, VOID *)
{
    TINFO *tinfo = new TINFO(PIN_GetContextReg(ctxt, REG_STACK_PTR));
    ThreadInfos.insert(std::make_pair(tid, tinfo));
    tinfo->_tid=tid;
    PIN_SetContextReg(ctxt, RegTinfo, reinterpret_cast<ADDRINT>(tinfo));
    cout << "thread begin " << tid << "\n";
    PIN_GetLock(&pinLock, tid+1);
    fprintf(out, "thread begin %d\n",tid);
    fflush(out);
    PIN_ReleaseLock(&pinLock);
}

static VOID OnThreadEnd(THREADID tid, const CONTEXT *ctxt, INT32, VOID *)
{
    TINFO_MAP::iterator it = ThreadInfos.find(tid);
    fprintf(out, "thread end %d\n",tid);
    PIN_GetLock(&pinLock, tid+1);
    fflush(out);
    PIN_ReleaseLock(&pinLock);
    if (it != ThreadInfos.end())
    {
        delete it->second;
        ThreadInfos.erase(it);
    }
}

static VOID Instruction(INS ins, VOID *)
{
    if (INS_IsRet(ins))
    {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BeforeRet, IARG_REG_VALUE, REG_STACK_PTR, IARG_REG_VALUE, REG_EBP, IARG_REG_VALUE, RegTinfo, IARG_INST_PTR, IARG_END);
	return;
    }

    if (INS_IsCall(ins))
    {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BeforeCall, IARG_REG_VALUE, REG_STACK_PTR, IARG_REG_VALUE, REG_EBP, IARG_REG_VALUE, RegTinfo, IARG_INST_PTR, IARG_END);
	return;
    }
}

static ADDRINT BeforeCall(ADDRINT sp, ADDRINT bp,  ADDRINT addrInfo, ADDRINT ip)
{
    TINFO *tinfo = reinterpret_cast<TINFO *>(addrInfo);
    if (sp > tinfo->_stackBase)
        return 0;

    size_t size = tinfo->_stackBase - sp;
    if (size > tinfo->_max)
        tinfo->_max = size;
    return 0;

}
static ADDRINT BeforeRet(ADDRINT sp, ADDRINT bp,  ADDRINT addrInfo, ADDRINT ip)
{
    TINFO *tinfo = reinterpret_cast<TINFO *>(addrInfo);
    if (!((sp < tinfo->_stackBase) && (sp  > tinfo->_max)))
	fprintf(out, "Spivot Detected: Aftersp_write addr 0x%x bp 0x%x sp 0x%x\n", ip, bp, sp);

    return 0;
}
