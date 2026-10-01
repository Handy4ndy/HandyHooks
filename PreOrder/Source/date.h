// credit for date contribution algorithm: https://stackoverflow.com/a/42936293 (Howard Hinnant)
#define SETUP_CURRENT_MONTH()\
uint16_t current_month = 0;\
{\
    int64_t s = ledger_last_time() + 946684800;\
    int64_t z = s / 86400 + 719468;\
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;\
    uint64_t doe = (uint64_t)(z - era * 146097);\
    uint64_t yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;\
    int64_t y = (int64_t)(yoe) + era * 400;\
    uint64_t doy = doe - (365*yoe + yoe/4 - yoe/100);\
    uint64_t mp = (5*doy + 2)/153;\
    uint64_t d = doy - (153*mp+2)/5 + 1;\
    uint64_t m = mp + (mp < 10 ? 3 : -9);\
    y += (m <= 2);\
    current_month = y * 12 + m;\
    if (DEBUG) \
    {\
        TRACEVAR(y);\
        TRACEVAR(m);\
        TRACEVAR(d);\
        TRACEVAR(current_month);\
    }\
}