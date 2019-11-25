# Copyright (c) 2020 ETH Zurich
#
# SPDX-License-Identifier: BSL-1.0
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

module purge
module load cmake
module load llvm/13
module load boost/1.78.0-${build_type,,}
module load hwloc
module load openmpi

export HPXRUN_RUNWRAPPER=srun
export CXX_STD="20"

configure_extra_options+=" -DCMAKE_BUILD_TYPE=${build_type}"
configure_extra_options+=" -DHPX_WITH_CXX_STANDARD=${CXX_STD}"
configure_extra_options+=" -DHPX_WITH_MALLOC=system"
configure_extra_options+=" -DHPX_WITH_FETCH_ASIO=ON"
configure_extra_options+=" -DHPX_WITH_COMPILER_WARNINGS=ON"
configure_extra_options+=" -DHPX_WITH_COMPILER_WARNINGS_AS_ERRORS=ON"
configure_extra_options+=" -DHPX_WITH_PARCELPORT_MPI=ON"
configure_extra_options+=" -DHPX_WITH_PARCELPORT_LCI=ON"
configure_extra_options+=" -DHPX_WITH_FETCH_LCI=ON"
configure_extra_options+=" -DHPX_WITH_LOGGING=OFF"
configure_extra_options+=" -DHPX_WITH_TESTS_COMMAND_LINE=--hpx:queuing=local-workrequesting-fifo"
configure_extra_options+=" -DLCI_SERVER=ibv"

# Make sure HWLOC does not report 'cores'. This is purely an option to enable
# testing the topology code under conditions close to those on FreeBSD.
configure_extra_options+=" -DHPX_TOPOLOGY_WITH_ADDITIONAL_HWLOC_TESTING=ON"
