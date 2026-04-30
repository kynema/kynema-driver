#ifndef MPIUTILITIES_H
#define MPIUTILITIES_H
#include "mpi.h"

namespace driver {

//! Create a subcommunicator
MPI_Comm
create_subcomm(MPI_Comm comm, const int num_ranks, const int start_rank = 0)
{
    int mpi_size;
    MPI_Comm_size(comm, &mpi_size);
    if ((start_rank + num_ranks) > mpi_size)
        throw std::runtime_error(
            "Number of MPI ranks requested is greater than available ranks: "
            "MPI size = " +
            std::to_string(mpi_size) +
            "; Num ranks requested = " + std::to_string(num_ranks));

    MPI_Group world_group, sub_group;
    MPI_Comm_group(comm, &world_group);

    int sub_range[1][3] = {{start_rank, start_rank + num_ranks - 1, 1}};
    MPI_Group_range_incl(world_group, 1, sub_range, &sub_group);

    MPI_Comm sub_comm;
    MPI_Comm_create(comm, sub_group, &sub_comm);
    return sub_comm;
}

} // namespace driver
#endif /* MPIUTILITIES_H */
