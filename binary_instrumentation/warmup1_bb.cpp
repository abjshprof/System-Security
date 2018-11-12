#include <iostream>
#include <fstream>
#include "pin.H"
#include <signal.h>

ofstream OutFile;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
static UINT64 bbcount = 0;
PIN_LOCK pinLock;
static REG RegTinfo;
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

// This function is called before every block
VOID docount(ADDRINT addrInfo)
{
	TINFO *tinfo = reinterpret_cast<TINFO *>(addrInfo);
	PIN_GetLock(&pinLock, tinfo->_tid+1);
	bbcount++;
	PIN_ReleaseLock(&pinLock);
}
    
// Pin calls this function every time a new basic block is encountered
// It inserts a call to docount
VOID Trace(TRACE trace, VOID *v)
{
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to docount before every bbl, passing the number of instructions
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)docount, IARG_REG_VALUE, RegTinfo, IARG_END);
    }
}


static VOID OnThreadStart(THREADID tid, CONTEXT *ctxt, INT32, VOID *)
{
    TINFO *tinfo = new TINFO(PIN_GetContextReg(ctxt, REG_STACK_PTR));
    ThreadInfos.insert(std::make_pair(tid, tinfo));
    tinfo->_tid=tid;
    PIN_SetContextReg(ctxt, RegTinfo, reinterpret_cast<ADDRINT>(tinfo));
    cout << "thread begin " << tid << "\n";
    PIN_GetLock(&pinLock, tid+1);
    OutFile << "thread begin " << tid << "\n";
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

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "warmup1_bb.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    // Write to a file since cout and cerr maybe closed by the application
    OutFile.setf(ios::showbase);
    OutFile << "BB Count " << bbcount << endl;
    cout << "BB Count " << bbcount << "\n";
    OutFile.close();
}

static BOOL Finikill(THREADID tid, INT32 sig, CONTEXT *, BOOL, const EXCEPTION_INFO *, VOID *)
{

    PIN_GetLock(&pinLock, tid+1);
    OutFile << " Tid " << tid << " got sig "<< sig << " BB Count: " << bbcount << endl;
    PIN_ReleaseLock(&pinLock);
    OutFile.close();
    return TRUE;
}
/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());

    PIN_InitLock(&pinLock);

    RegTinfo = PIN_ClaimToolRegister();
    if (!REG_valid(RegTinfo))
    {
        std::cerr << "Cannot allocate a scratch register.\n";
        std::cerr << std::flush;
        return 1;
    }
    PIN_AddThreadStartFunction(OnThreadStart, 0);
    PIN_AddThreadFiniFunction(OnThreadEnd, 0);
    PIN_AddFiniFunction(Fini, 0);

    for (int sig = 1;  sig < SIGRTMIN;  sig++) {
        PIN_InterceptSignal(sig, Finikill, 0);
        PIN_UnblockSignal(sig, TRUE);
    }


    // Register Instruction to be called to instrument instructions
    TRACE_AddInstrumentFunction(Trace, 0);

    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
