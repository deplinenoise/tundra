#ifndef OUTPUT_VALIDATION_HPP
#define OUTPUT_VALIDATION_HPP

namespace t2
{
    struct ExecResult;
    struct NodeData;

    enum ValidationResult
    {
        Pass,
        SwallowStdout,
        Fail
    };

    ValidationResult ValidateExecResultAgainstAllowedOutput(ExecResult* result, const NodeData* node_data);
}
#endif