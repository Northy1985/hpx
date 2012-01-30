//  Copyright (c) 2011-2012 Bryce Adelstein-Lelbach
//  Copyright (c) 2007-2012 Hartmut Kaiser
//  Copyright (c) 2012      Dylan Stark
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/util/high_resolution_timer.hpp>

#include <stdexcept>
#include <iostream>

#include <qthread/qthread.h>

#include <boost/assert.hpp>
#include <boost/atomic.hpp>
#include <boost/bind.hpp>
#include <boost/cstdint.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/random.hpp>
#include <boost/ref.hpp>

using boost::program_options::variables_map;
using boost::program_options::options_description;
using boost::program_options::value;
using boost::program_options::store;
using boost::program_options::command_line_parser;
using boost::program_options::notify;

using hpx::util::high_resolution_timer;

///////////////////////////////////////////////////////////////////////////////
boost::atomic<boost::uint64_t> donecount(0);

///////////////////////////////////////////////////////////////////////////////
extern "C" aligned_t null_thread(
    void* args
    )
{
    boost::uint64_t const delay = reinterpret_cast<boost::uint64_t>(args);

    double volatile d = 0.;
    for (boost::uint64_t i = 0; i < delay; ++i)
        d += 1 / (2. * i + 1);

    ++donecount;

    return aligned_t();
}

///////////////////////////////////////////////////////////////////////////////
void print_results(
    boost::uint64_t cores
  , boost::uint64_t seed
  , boost::uint64_t tasks
  , boost::uint64_t min_delay
  , boost::uint64_t max_delay
  , boost::uint64_t total_delay
  , double walltime
  , boost::uint64_t current_trial
  , boost::uint64_t total_trials
    )
{
    if (current_trial == 1)
    {
        std::string const cores_str = boost::str(boost::format("%lu,") % cores);
        std::string const seed_str  = boost::str(boost::format("%lu,") % seed);
        std::string const tasks_str = boost::str(boost::format("%lu,") % tasks);

        std::string const min_delay_str
            = boost::str(boost::format("%lu,") % min_delay);
        std::string const max_delay_str
            = boost::str(boost::format("%lu,") % max_delay);
        std::string const total_delay_str
            = boost::str(boost::format("%lu,") % total_delay);

        std::cout <<
            ( boost::format("%-21s %-21s %-21s %-21s %-21s %-21s %-08.8g")
            % cores_str % seed_str % tasks_str
            % min_delay_str % max_delay_str % total_delay_str
            % walltime);
    }

    else
        std::cout << (boost::format(", %-08.8g") % walltime);

    if (current_trial == total_trials)
        std::cout << "\n";
}

///////////////////////////////////////////////////////////////////////////////
boost::uint64_t shuffler(
    boost::random::mt19937_64& prng
  , boost::uint64_t high
    )
{
    if (high == 0)
        throw std::logic_error("high value was 0");

    // Our range is [0, x).
    boost::random::uniform_int_distribution<boost::uint64_t>
        dist(0, high - 1);

    return dist(prng); 
}

///////////////////////////////////////////////////////////////////////////////
int qthreads_main(
    variables_map& vm
    )
{
    {
        boost::uint64_t const min_delay = vm["min-delay"].as<boost::uint64_t>();
        boost::uint64_t const max_delay = vm["max-delay"].as<boost::uint64_t>();
        boost::uint64_t const total_delay
            = vm["total-delay"].as<boost::uint64_t>();

        boost::uint64_t const tasks = vm["tasks"].as<boost::uint64_t>();

        boost::uint64_t const current_trial
            = vm["current-trial"].as<boost::uint64_t>();
        boost::uint64_t const total_trials
            = vm["total-trials"].as<boost::uint64_t>();

        ///////////////////////////////////////////////////////////////////////
        // Initialize the PRNG seed. 
        boost::uint64_t seed = vm["seed"].as<boost::uint64_t>();

        if (!seed)
            seed = boost::uint64_t(std::time(0));

        ///////////////////////////////////////////////////////////////////////
        // Validate command-line arguments.
        if (0 == tasks)
            throw std::invalid_argument("count of 0 tasks specified\n");

        if (min_delay > max_delay)
            throw std::invalid_argument("minimum delay cannot be larger than "
                                        "maximum delay\n");

        if (min_delay > total_delay)
            throw std::invalid_argument("minimum delay cannot be larger than"
                                        "total delay\n");

        if (max_delay > total_delay)
            throw std::invalid_argument("maximum delay cannot be larger than "
                                        "total delay\n");

        if ((min_delay * tasks) > total_delay)
            throw std::invalid_argument("minimum delay is too small for the "
                                        "specified total delay and number of "
                                        "tasks\n");

        if ((max_delay * tasks) < total_delay)
            throw std::invalid_argument("maximum delay is too small for the "
                                        "specified total delay and number of "
                                        "tasks\n");

        ///////////////////////////////////////////////////////////////////////
        // Randomly generate a description of the heterogeneous workload. 
        std::vector<boost::uint64_t> payloads;
        payloads.reserve(tasks);

        // For random numbers, we use a 64-bit specialization of Boost.Random's
        // mersenne twister engine (good uniform distribution up to 311
        // dimensions, cycle length 2 ^ 19937 - 1)
        boost::random::mt19937_64 prng(seed);

        boost::uint64_t current_sum = 0;

        for (boost::uint64_t i = 0; i < tasks; ++i)
        {
            // Credit to Spencer Ruport for putting this algorithm on
            // stackoverflow.
            boost::uint64_t const low_calc
                = (total_delay - current_sum) - (max_delay * (tasks - 1 - i));

            bool const negative
                = (total_delay - current_sum) < (max_delay * (tasks - 1 - i));

            boost::uint64_t const low
                = (negative || (low_calc < min_delay)) ? min_delay : low_calc;

            boost::uint64_t const high_calc
                = (total_delay - current_sum) - (min_delay * (tasks - 1 - i));

            boost::uint64_t const high
                = (high_calc > max_delay) ? max_delay : high_calc;

            // Our range is [low, high].
            boost::random::uniform_int_distribution<boost::uint64_t>
                dist(low, high);

            boost::uint64_t const payload = dist(prng);

            if (payload < min_delay)
                throw std::logic_error("task delay is below minimum"); 

            if (payload > max_delay)
                throw std::logic_error("task delay is above maximum"); 

            current_sum += payload;
            payloads.push_back(payload);
        }

        // Randomly shuffle the entire sequence to deal with drift.
        std::random_shuffle(payloads.begin(), payloads.end()
                          , boost::bind(&shuffler, boost::ref(prng), _1));

        ///////////////////////////////////////////////////////////////////////
        // Validate the payloads.
        if (payloads.size() != tasks)
            throw std::logic_error("incorrect number of tasks generated");

        boost::uint64_t const payloads_sum =
            std::accumulate(payloads.begin(), payloads.end(), 0LLU);
        if (payloads_sum != total_delay)
            throw std::logic_error("incorrect total delay generated");
 
        ///////////////////////////////////////////////////////////////////////
        // Initialize qthreads. 
        if (qthread_initialize() != 0)
            throw std::runtime_error("qthreads failed to initialize\n");

        ///////////////////////////////////////////////////////////////////////
        // Start the clock.
        high_resolution_timer t;

        ///////////////////////////////////////////////////////////////////////
        // Queue the tasks in a serial loop. 
	    for (boost::uint64_t i = 0; i < tasks; ++i)
        { 
            void* const ptr = reinterpret_cast<void*>(payloads[i]);
            qthread_fork(&null_thread, ptr, NULL);
        }

        ///////////////////////////////////////////////////////////////////////
        // Wait for the work to finish.
        do {
            // Yield until all our null qthreads are done.
    	    qthread_yield();
    	} while (donecount != tasks);

        ///////////////////////////////////////////////////////////////////////
        // Print the results.
        print_results(qthread_num_workers()
                    , seed
                    , tasks
                    , min_delay
                    , max_delay
                    , total_delay
                    , t.elapsed()
                    , current_trial
                    , total_trials);
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
int main(
    int argc
  , char* argv[]
    )
{
    ///////////////////////////////////////////////////////////////////////////
    // Parse command line.
    variables_map vm;

    options_description cmdline("Usage: " HPX_APPLICATION_STRING " [options]");

    cmdline.add_options()
        ( "help,h"
        , "print out program usage (this message)")
        
        ( "shepherds,s"
        , value<boost::uint64_t>()->default_value(1),
         "number of shepherds to use")

        ( "workers-per-shepherd,w"
        , value<boost::uint64_t>()->default_value(1),
         "number of worker OS-threads per shepherd")

        ( "tasks"
        , value<boost::uint64_t>()->default_value(500000)
        , "number of tasks (e.g. px-threads)")

        ( "min-delay"
        , value<boost::uint64_t>()->default_value(0)
        , "minimum number of iterations in the delay loop")

        ( "max-delay"
        , value<boost::uint64_t>()->default_value(0)
        , "maximum number of iterations in the delay loop")

        ( "total-delay"
        , value<boost::uint64_t>()->default_value(0)
        , "total number of delay iterations to be executed")
        
        ( "current-trial"
        , value<boost::uint64_t>()->default_value(1)
        , "current trial (must be greater than 0 and less than --total-trials)")

        ( "total-trials"
        , value<boost::uint64_t>()->default_value(1)
        , "total number of trial runs")

        ( "seed"
        , value<boost::uint64_t>()->default_value(0)
        , "seed for the pseudo random number generator (if 0, a seed is "
          "choosen based on the current system time)")
        ;

    store(command_line_parser(argc, argv).options(cmdline).run(), vm);

    notify(vm);

    // Print help screen.
    if (vm.count("help"))
    {
        std::cout << cmdline;
        return 0;
    }

    // Set qthreads environment variables.
    std::string const shepherds = boost::lexical_cast<std::string>
        (vm["shepherds"].as<boost::uint64_t>());
    std::string const workers = boost::lexical_cast<std::string>
        (vm["workers-per-shepherd"].as<boost::uint64_t>());

    setenv("QT_NUM_SHEPHERDS", shepherds.c_str(), 1);
    setenv("QT_NUM_WORKERS_PER_SHEPHERD", workers.c_str(), 1);

    return qthreads_main(vm);
}

