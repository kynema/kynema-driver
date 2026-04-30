#include "MemoryUsage.h"

namespace driver {

#ifdef __linux__
long memory_usage()
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    // convert to MB
    return static_cast<long>(static_cast<double>(usage.ru_maxrss) / 1024.0);
}
#else
long memory_usage() { return -1; }
#endif

} // namespace driver
