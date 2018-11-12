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
    "o", "warmup3_ct.out", "specify output file name");
static REG RegTinfo;
FILE * out;
static UINT64 dir_bcount = 0;
static UINT64 indir_bcount = 0;

// Information about each thread.
//
struct TINFO
{
    TINFO(ADDRINT base) : _tid(-1) {}
    THREADID _tid;
    std::ostringstream _os; // Used to format messages.
};

typedef std::map<THREADID, TINFO *> TINFO_MAP;
static TINFO_MAP ThreadInfos;

static VOID OnThreadStart(THREADID, CONTEXT *, INT32, VOID *);
static VOID OnThreadEnd(THREADID, const CONTEXT *, INT32, VOID *);
static VOID Instruction(INS, VOID *);
//static ADDRINT OnStackChangeIf(ADDRINT, ADDRINT);


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
    fprintf(out, "\nTotal Direct br count %llu indir branch count is %llu\n", dir_bcount, indir_bcount);
    fflush(out);
    fclose(out);
}


static BOOL Finikill(THREADID tid, INT32 sig, CONTEXT *, BOOL, const EXCEPTION_INFO *, VOID *)
{

    PIN_GetLock(&pinLock, tid+1);
    fprintf(out, "Got signal: thread %d ended with sig %d\n", tid, sig);
    fprintf(out, "\nTotal Direct br count %llu indir branch count is %llu\n", dir_bcount, indir_bcount);
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
    //cout << "Thread %d "<< tid << " Max stack usage so far 0x" << hex << it->second->_max <<"\n";
    if (it != ThreadInfos.end())
    {
        delete it->second;
        ThreadInfos.erase(it);
    }
}

static VOID dir_br_count(ADDRINT addrInfo)
{
	TINFO *tinfo = reinterpret_cast<TINFO *>(addrInfo);
	PIN_GetLock(&pinLock, tinfo->_tid+1);
	dir_bcount++;
	PIN_ReleaseLock(&pinLock);
}


static VOID indir_br_count(ADDRINT addrInfo)
{
	TINFO *tinfo = reinterpret_cast<TINFO *>(addrInfo);
	PIN_GetLock(&pinLock, tinfo->_tid+1);
	indir_bcount++;
	PIN_ReleaseLock(&pinLock);
}

static VOID Instruction(INS ins, VOID *)
{
	if (INS_IsDirectBranchOrCall(ins)) {
		//fprintf(out, "Direct br addr 0x%x instr %s\n",INS_Address(ins), INS_Mnemonic(ins).c_str());
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)dir_br_count, 
				IARG_REG_VALUE, RegTinfo, IARG_END);
	}
	if (INS_IsIndirectBranchOrCall(ins)) {
		//fprintf(out, "InDirect br addr 0x%x instr %s\n", INS_Address(ins), INS_Mnemonic(ins).c_str());
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)indir_br_count,
			IARG_REG_VALUE, RegTinfo, IARG_END);
	}
}
