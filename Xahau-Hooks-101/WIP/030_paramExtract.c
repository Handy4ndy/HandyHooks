#include "hookapi.h"

// Buffer for parameter name (max 32 bytes as per Hook API)
#define PARAM_NAME_MAX 32
// Buffer for parameter value (arbitrary max size, adjust as needed)
#define PARAM_VALUE_MAX 20

int64_t hook(uint32_t reserved)
{
    TRACESTR("SimpleParamLog: Started.");

    // Buffer for parameter name and value
    uint8_t param_name[PARAM_NAME_MAX] = {'A', 'C'};
    uint8_t param_value[PARAM_VALUE_MAX];
    int64_t param_name_len = 2; // Length of "EMIT"
    int64_t param_value_len;

    // Retrieve parameter by name
    param_value_len = otxn_param(param_value, PARAM_VALUE_MAX, param_name, param_name_len);

    // Check if parameter exists
    if (param_value_len == DOESNT_EXIST)
    {
        TRACESTR("SimpleParamLog: No 'EMIT' parameter found.");
    }
    else if (param_value_len < 0)
    {
        TRACEVAR(param_value_len);
        NOPE("SimpleParamLog: Error: Failed to read parameter.");
    }
    else
    {
        // Log parameter name and value
        TRACESTR("SimpleParamLog: Parameter found:");
        TRACEHEX(param_name);   // Log parameter name
        TRACEHEX(param_value);  // Log parameter value
        TRACEVAR(param_name_len);
        TRACEVAR(param_value_len);
    }

    // Accept the transaction
    DONE("SimpleParamLog: Transaction accepted after logging parameters.");

    _g(1,1);
    return 0;
}