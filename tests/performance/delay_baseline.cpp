//  Copyright (c) 2011 Bryce Adelstein-Lelbach
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_init.hpp>
#include <hpx/util/high_resolution_timer.hpp>
#include <hpx/include/iostreams.hpp>

#include <stdexcept>

#include <boost/format.hpp>
#include <boost/cstdint.hpp>

using boost::program_options::variables_map;
using boost::program_options::options_description;
using boost::program_options::value;

using hpx::init;
using hpx::finalize;

using hpx::util::high_resolution_timer;

using hpx::cout;
using hpx::flush;

///////////////////////////////////////////////////////////////////////////////
boost::uint64_t delay = 0;

///////////////////////////////////////////////////////////////////////////////
void null_thread()
{
    double volatile d = 0.;
    for (boost::uint64_t i = 0; i < delay; ++i)
        d += 1 / (2. * i + 1);
}

///////////////////////////////////////////////////////////////////////////////
void print_results(
    boost::uint64_t delay
  , double walltime
    )
{
    std::string const delay_str = boost::str(boost::format("%lu,") % delay);

    cout << (boost::format("%-21s %-08.8g\n") % delay_str % walltime);
}


///////////////////////////////////////////////////////////////////////////////
int hpx_main(
    variables_map& vm
    )
{
    {
        delay = vm["delay"].as<boost::uint64_t>();
        boost::uint64_t const tasks = vm["tasks"].as<boost::uint64_t>();

        if (0 == tasks)
            throw std::invalid_argument("error: count of 0 tasks specified\n");

        std::vector<double> results;
        results.reserve(tasks);

        for (boost::uint64_t i = 0; i < tasks; ++i)
        {
            // Start the clock.
            high_resolution_timer t;

            null_thread(); 

            results.push_back(t.elapsed());
        }

        // Print out the results.
        for (std::size_t i = 0; i < results.size(); ++i)
            print_results(delay, results[i]);

        cout << flush;
    }

    finalize();
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
int main(
    int argc
  , char* argv[]
    )
{
    // Configure application-specific options.
    options_description cmdline("usage: " HPX_APPLICATION_STRING " [options]");

    cmdline.add_options()
        ( "tasks"
        , value<boost::uint64_t>()->default_value(64)
        , "number of tasks (e.g. serial loop iterations) to invoke")

        ( "delay"
        , value<boost::uint64_t>()->default_value(0)
        , "number of iterations in the delay loop")
        ;

    // Initialize and run HPX.
    return init(cmdline, argc, argv);
}

