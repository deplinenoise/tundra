#ifndef OUTPUT_VALIDATION_HPP
#define OUTPUT_VALIDATION_HPP

namespace t2
{
    struct ExecResult;
    struct NodeData;
    
    bool ValidateExecResultAgainstAllowedOutput(ExecResult* result, const NodeData* node_data);
}
#endif