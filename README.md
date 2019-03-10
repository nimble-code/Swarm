# Swarm

Swarm is a verification script generator front-end for the formal verification tool (logic model checker) Spin.

The swarm tool can be used to generate a script that performs many small and randomly different Spin verification jobs in parallel, in an attempt to increase the problem coverage for very large verification problems.
The tool is meant to be used on models for which a traditional run using exhaustive, bitstate, hash-compaction etc. verification either runs out of memory, or takes more time than available (e.g., days or weeks).
Swarm uses parallelism and search diversification to reach its objectives.

The user can use a configuration file that defines how many CPUs are available, how much memory can be used, and how much time is available, among a range of other optional parameter settings.
Based on these settings, swarm generates the script that runs as many independent jobs as possible, without exceeding the user-defined constraints.

### BASIC USAGE
By default swarm writes a verification script into a file with the extension _.swarm_ and with the basename equal to that of the Spin model being verified.
For instance:

	$ swarm -f model.pml	# generate the script
	$ ./model.swarm		# execute it

The script accepts a small number of options that are detailed below.

In the example above, we have set only one required parameter: the name of the model file.  A number of other options is available to define the details of the swarm verification. The easiest is to use a configuration file to record all these options.
A default configuration file can be generated with the -l parameter:

	$ swarm -l > swarm.lib	# configuration file -- which can be edited
	$ swarm swarm.lib	# use the settings from this file to generate the script

Options set on the command-line before the -l parameter appears (e.g., the number of CPUs to be used, or the maximum runtime) are used to define the corresponding settings in the configuration file that is generated.
The default configuration file may look as follows.


	# Swarm Version 2.2 -- 15 October 2009
	#
	# Default Swarm configuration file
	#
	# there are four main parts to this file:
	#       ranges, limits, compilation options, and runtime options
	# the default settings for each are shown below -- edit as needed
	# comments start with a # symbol
	# This version of swarm requires Spin Version 5.2 or later
	
	# See the documentation for the additional use of
	# environment variables CCOMMON, RCOMMON, and SCOMMON
	# http://spinroot.com/swarm/
	
	# range
	k       1       4       # optional: to restrict min and max nr of hash functions
	
	# limits
	d       10000           # optional: to restrict the max search depth
	cpus    2               # nr available cpus (exact)
	memory  512M            # max memory per run; M=megabytes, G=gigabytes
	time    60m             # max time for all runs; h=hours, m=min, s=sec, d=days
	hash    1.5             # hash-factor (estimate)
	vector  512             # nr of bytes per state (estimate)
	speed   250000          # nr states explored per second (estimate)
	file    model.pml       # file with the spin model to be verified
	
	# compilation options (each line defines one complete search mode)

	-DBITSTATE -DPUTPID		# basic dfs
	-DBITSTATE -DPUTPID -DREVERSE	# reversed process ordering
	-DBITSTATE -DPUTPID -DT_REVERSE	# reversed transition ordering
	-DBITSTATE -DPUTPID -DREVERSE -DT_REVERSE	# both

	-DBITSTATE -DPUTPID -DP_RAND -DT_RAND	# same series with randomization
	-DBITSTATE -DPUTPID -DP_RAND -DT_RAND -DT_REVERSE
	-DBITSTATE -DPUTPID -DP_RAND -DT_RAND -DREVERSE
	-DBITSTATE -DPUTPID -DP_RAND -DT_RAND -DREVERSE -DT_REVERSE

	-DBITSTATE -DPUTPID -DBCS	# with bounded context switching
	-DBITSTATE -DPUTPID -DBCS -DREVERSE
	-DBITSTATE -DPUTPID -DBCS -DT_REVERSE
	-DBITSTATE -DPUTPID -DBCS -DREVERSE -DT_REVERSE

	# runtime options (one line)
	-c1 -x -n


Remote executions can be defined by prefixing the number of available cpus in the configuration file with a machine name. For instance:

	cpus	3	nada:4	niks:5

defines that 3 local cpus can be used, 4 cpus on the machine nada, and 5 additional cpus on the machine niks. It is assumed that connections are setup that allow for password-less ssh access to all named machines.

Some (though not all) options can be set on the command line. Options are used in the order in which they are encountered. This means that command line options can be used to override settings in a configuration file or vice versa, if both are used.

### OPTIONS

The following command line options are recognized:

* _file_ reads the configuration parameters from the named _file_.

* Option -bN sets the number of bytes per state to N (default -b512).

* Option -cN sets the number of cpus available to N (default -c2).

* Option -dN sets the maximum search depth to N (default -d10000).

* Option -e allow for early termination when at least one trail file was written.

* Option -f _model.pml_ specifies the name of the file with the spin model to be verified.

* Option -hN sets the estimated hash-factor (default -h4).

* Option -l write a default configuration file to stdout.

* Option -mN sets the maximum amount of memory to use per run to N, with optional suffix M or G, (default -m512M).

* Option -nN sets the seed value for the random number generated to N (default -n123).

* Option -sN sets the number of states stored per second to N (default -s250000).

* Option -tN sets the number of hours available for all runs combined to N (default -t1).

* Option -uN sets the minimum search depth to be used to N (default -u128).

* Option -v verbose mode.

* Option -V print the version number and exit.

The shell variable CCOMMON can be used to define standard compile time directives that should be used for all runs (the default setting is CCOMMON="-O2 -DSAFETY").
The shell variable RCOMMON can be used to define run time parameters that should be used for all runs (the default is RCOMMON="").
Shell variable SCOMMON, finally, can be used to define additional run time parameters for Spin itself for model generation (e.g., -m or -D...).

### EXAMPLES

A typical run of `swarm` looks as follows:

	$ swarm config_file
	swarm: 95 runs, avg time per cpu 3599.5 sec
	$ ./model.swarm


Without a config file, with the spin model in file model.pml, and executing on a quad-core machine with at least 16 GB of memory, with a time-bound of 2 hours for the verification:


	$ swarm -c4 -m16G -t2 -f model.pml	# note -m16G not -m166
	swarm: 63 runs, avg time per cpu 7199.9 sec
	swarm: script written to model.swarm
	$ ./model.swarm


When executed, the verification script will create four additional shell scripts, which by default are named script0 .. script3, one for each available cpu core, and populates each script with the required commands.
Before executing those commands, the main script calls `spin` to generate pan.c and related files, and compiles them for a range of different search methods. The execution of a script either stops when an error trail is found, or when the time limit is reached. By default, swarm assumes that states are analyzed at an average rate of 250,000 states per second. If this estimate is too high, the execution of the script will take
proportionally more time than planned, if the estimate is too low then it will take less time. Once the rate is known (which if important can easily be determined with a small separate run in bitstate mode), it can be specified on the command line to improve accuracy. For instance, if the verification is known to execute at a rate of 100,000 states per second, the last `swarm` command can be made more precise, as:

	$ swarm -c4 -m16G -t2 -s100000 -f model.pml
	swarm: 57 runs, avg time per cpu 7199.9 sec
	swarm: script written to model.swarm

Other default settings that can affect the accuracy of the runtime estimates
used by `swarm` in generating the script include the hash-factor
(the -h parameter or the _hash_ field in a configuration file).

The verification script can recognize the following three parameters when invoked:
_prep_, _exec_, and _base_ name.
Assuming that the script is stored in an executable file called _model.swarm_:

	$ ./model.swarm base xsw	# change basename of subsidiary scripts

This makes the script use xsw0, ..., xswN for the subsidiary script names, instead of the default script0, ..., scriptN.

	$ ./model.swarm prep		# perform setup only

performs the compilation of all required executables, and generates all the subsidiary execution scripts required (one per cpu used), but does not perform the actual execution of those scripts.

	$ ./model.swarm exec		# perform execution, but no setup

performs the execution of all scripts, assuming the setup was done before, including the execution of all scripts to be executed remotely (they are copied to the remote machines with `scp` in the _prep_ phase.
The _prep_ and _base_ parameters can be used in combination, but it is not useful to use _base_ in combination with _exec_.
