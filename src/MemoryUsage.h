#ifndef MEMORYUSAGE_H
#define MEMORYUSAGE_H

#ifdef __linux__
#include <sys/resource.h>
#endif

namespace driver {

//! Get memory usage in MB
long memory_usage();

} // namespace driver
#endif /* MEMORYUSAGE_H */
