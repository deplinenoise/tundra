#include "Exec.hpp"
#include "DagData.hpp"
#include "re.h"

#include "OutputValidation.hpp"

namespace t2
{

static bool HasAnyNonNewLine(const char* buffer)
{
    while (true)
    {
        char c = *buffer;
        if (c == 0)
            return false;
        if (c == 0xD || c == 0xA)
            buffer++;
        else
            return true;
    }
}

ValidationResult ValidateExecResultAgainstAllowedOutput(ExecResult* result, const NodeData* node_data)
{
    auto& allowed = node_data->m_AllowedOutputSubstrings;
    bool allowOutput = node_data->m_Flags & NodeData::kFlagAllowUnexpectedOutput;

    if (allowOutput && allowed.GetCount() == 0)
        return ValidationResult::Pass;

    const char* buffer = result->m_OutputBuffer.buffer;
    if (!HasAnyNonNewLine(buffer))
        return ValidationResult::Pass;

    for (int i=0; i!=allowed.GetCount(); i++)
    {
        const char* allowedSubstring = allowed[i];

        if (re_match(allowedSubstring, result->m_OutputBuffer.buffer) != -1)
        {
            ValidationResult returnValue = ValidationResult::SwallowStdout;
            //printf("MATCH %s against %s allowOutput %d returning %d\n", allowedSubstring, result->m_OutputBuffer.buffer, (int)allowOutput, returnValue);
            return returnValue;
        }
    }
    return allowOutput ? ValidationResult::Pass : ValidationResult::Fail;
}

}