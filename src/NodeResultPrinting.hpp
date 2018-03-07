#ifndef NODERESULTPRINTING_HPP
#define NODERESULTPRINTING_HPP
#include <ctime>
#include "OutputValidation.hpp"

namespace t2
{
struct ExecResult;
struct NodeData;
struct BuildQueue;

void InitNodeResultPrinting();
void PrintNodeResult(ExecResult* result, const NodeData* node_data, const char* cmd_line, BuildQueue* queue, bool always_verbose, time_t exec_start_time, ValidationResult validationResult);
int PrintNodeInProgress(const NodeData* node_data, time_t time_of_start, const BuildQueue* queue);
}
#endif