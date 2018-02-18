#ifndef NODERESULTPRINTING_HPP
#define NODERESULTPRINTING_HPP

namespace t2
{
struct ExecResult;
struct NodeData;
struct BuildQueue;

void InitNodeResultPrinting();
void PrintNodeResult(ExecResult* result, const NodeData* node_data, const char* cmd_line, BuildQueue* queue, bool always_verbose);

}
#endif