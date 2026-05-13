// predictor
#include "pin.H"
#include <iostream>
#include <unordered_map>
#include <iomanip>
#include <fstream>

using namespace std;

unordered_map<ADDRINT, UINT8> BHT;

UINT8 GHR = 0;

UINT64 totalBranches = 0;
UINT64 correctPredictions = 0;
UINT64 takenBranches = 0;
UINT64 notTakenBranches = 0;
ADDRINT predictedTarget = 0;

ADDRINT mainLow = 0;
ADDRINT mainHigh = 0;
ofstream logFile;
ofstream instTraceFile;
ofstream pcAccuracy;


struct LoopInfo
{
    UINT64 tripCount = 0;
    UINT64 currentIter = 0;
    BOOL valid = FALSE;
    LoopInfo()
    {
        tripCount = 0;
        currentIter = 0;
        valid = FALSE;
    }
};

unordered_map<ADDRINT, ADDRINT> BTB;
unordered_map<ADDRINT, LoopInfo> LoopTable;
unordered_map<ADDRINT, double> taken1;
unordered_map<ADDRINT, double> total;

string GetStateName(UINT8 state)
{
    switch (state)
    {
    case 0:
        return "SN";
    case 1:
        return "WN";
    case 2:
        return "WT";
    case 3:
        return "ST";
    }

    return "??";
}

VOID Image(IMG img, VOID *v)
{
    if (IMG_IsMainExecutable(img))
    {
        mainLow = IMG_LowAddress(img);
        mainHigh = IMG_HighAddress(img);
    }
}
VOID TraceInstruction(ADDRINT pc, string *disasm)
{
    instTraceFile << hex << pc << " " << *disasm << endl;
}

VOID Trace(INS ins, VOID *v)
{
    ADDRINT pc = INS_Address(ins);

    if (pc < mainLow || pc >= mainHigh)
        return;

    string *disasm = new string(INS_Disassemble(ins));

    INS_InsertCall(
        ins,
        IPOINT_BEFORE,
        (AFUNPTR)TraceInstruction,

        IARG_INST_PTR,
        IARG_PTR,
        disasm,

        IARG_END);
}

VOID BranchPredictor(ADDRINT pc,
                     ADDRINT target,
                     BOOL taken)
{
    BOOL isLoop = (target < pc);

    totalBranches++;


    if (taken)
        takenBranches++;
    else    
        notTakenBranches++;

    ADDRINT index = pc ^ GHR;

    if (BHT.find(index) == BHT.end())
    {
        BHT[index] = 2;
    }

    UINT8 state = BHT[index];

    BOOL prediction;

    if (isLoop)
    {
        auto it = LoopTable.find(pc);

        if (it == LoopTable.end())
        {
            LoopTable.emplace(pc, LoopInfo());
            it = LoopTable.find(pc);
        }

        LoopInfo &loop = it->second;

        if (loop.valid)
        {
            if (loop.currentIter == loop.tripCount)
            {
                prediction = FALSE;
            }
            else
            {
                prediction = TRUE;
            }
        }
        else
        {

            prediction = (state >= 2);
        }
    }
    else
    {

        prediction = (state >= 2);
    }

    logFile << hex << pc
            << " -> " << target
            << " | "
            << (isLoop ? "LOOP" : "IF  ")
            << " | Pred: "
            << (prediction ? "T" : "N")
            << " | Actual: "
            << (taken ? "T" : "N");
            
    total[pc]++;
    if (prediction == taken)
    {
        correctPredictions++;
        taken1[pc]++;
    }
    if(prediction == true){
        BTB[pc] = target;
    }
    if (prediction && BTB.find(pc) != BTB.end())
    {
    predictedTarget = BTB[pc];
    logFile << " | BTB Target: "
            << BTB[pc]<<endl;
    }
    else{logFile<<endl;}   

    if (taken)
    {
        if (state < 3)
            state++;
    }
    else
    {
        if (state > 0)
            state--;
    }

    BHT[index] = state;

    if (isLoop)
    {
        auto it = LoopTable.find(pc);

        if (it == LoopTable.end())
        {
            LoopTable.emplace(pc, LoopInfo());
            it = LoopTable.find(pc);
        }

        LoopInfo &loop = it->second;

        if (taken)
        {
            loop.currentIter++;
        }
        else
        {

            loop.tripCount = loop.currentIter;
            loop.currentIter = 0;
            loop.valid = TRUE;
        }
    }
    GHR = ((GHR << 1) | taken) & 0xF;
}

VOID Instruction(INS ins, VOID *v)
{
    ADDRINT pc = INS_Address(ins);

    if (pc < mainLow || pc >= mainHigh)
        return;

    if (INS_IsBranch(ins))
    {
        INS_InsertCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR)BranchPredictor,

            IARG_INST_PTR,
            IARG_BRANCH_TARGET_ADDR,
            IARG_BRANCH_TAKEN,

            IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v)
{
    cout << endl;
    cout << "Final Stats" << endl;
    cout << "Total Branches      : " << dec << totalBranches << endl;
    cout << "Taken Branches      : " << takenBranches << endl;
    cout << "Not Taken Branches  : " << notTakenBranches << endl;
    cout << "Correct Predictions : " << correctPredictions << endl;

    double accuracy = 0.0;

    if (totalBranches != 0)
    {
        accuracy =
            ((double)correctPredictions / (double)totalBranches) * 100.0;
    }
    cout << fixed << setprecision(2);
    cout << "Prediction Accuracy : " << accuracy << "%" << endl;
    cout << endl;
    for(auto i : taken1){
        instTraceFile << hex << i.first <<double(i.second/total[i.first]) << endl;
    }
    logFile.close();
    instTraceFile.close();
    pcAccuracy.close();

    for(auto i : taken1){
        pcAccuracy <<"PC: "<< hex << i.first << " - " <<double(i.second*100/total[i.first])<<" %" << endl;
    }
}

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv))
    {
        cerr << "PIN INIT FAILED" << endl;
        return 1;
    }

    // INS_AddInstrumentFunction(Trace, 0);
    IMG_AddInstrumentFunction(Image, 0);

    INS_AddInstrumentFunction(Instruction, 0);

    PIN_AddFiniFunction(Fini, 0);

    logFile.open("branch_trace.log");
    instTraceFile.open("instruction_trace.log");
    pcAccuracy.open("pc_wise_accuracy.log");
    PIN_StartProgram();

    return 0;
}