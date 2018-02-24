#include "Exec.hpp"
#include "DagData.hpp"

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

bool ValidateExecResultAgainstAllowedOutput(ExecResult* result, const NodeData* node_data)
{
    if (node_data->m_Flags & NodeData::kFlagAllowUnexpectedOutput)
        return true;

    const char* buffer = result->m_OutputBuffer.buffer;
    if (!HasAnyNonNewLine(buffer))
        return true;

    for (int i=0; i!=node_data->m_AllowedOutputSubstrings.GetCount(); i++)
    {
        const char* allowedSubstring = node_data->m_AllowedOutputSubstrings[i];
        if (strstr(buffer, allowedSubstring) != 0)
            return true;
    }
    return false;
}

}