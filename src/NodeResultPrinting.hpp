#ifndef NODERESULTPRINTING_HPP
#define NODERESULTPRINTING_HPP
#include <ctime>
#include "OutputValidation.hpp"
#include <stdint.h>

namespace t2
{
struct ExecResult;
struct NodeData;
struct BuildQueue;

void InitNodeResultPrinting();
void PrintNodeResult(ExecResult* result, const NodeData* node_data, const char* cmd_line, BuildQueue* queue, bool always_verbose, time_t exec_start_time, ValidationResult validationResult);
int PrintNodeInProgress(const NodeData* node_data, uint64_t time_of_start, const BuildQueue* queue);
}
#endif