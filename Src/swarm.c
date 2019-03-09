/***** spin: warm.c *****/

/* Copyright (c) 2009-2011 --swarm is a support tool for the Spin verifier*/
/* All Rights Reserved.  This software is for educational purposes only.  */
/* No guarantee whatsoever is expressed or implied by the distribution of */
/* this code.  Permission is given to distribute this code provided that  */
/* this introductory message is not removed and no monies are exchanged.  */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
/* #include <malloc.h> */
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "swarm_seeds.h"	/* long seed_val[] = {...}; */

#define Version		"Swarm Version 3.2 -- 5 June 2017"
#define max(a,b)	(((a)>=(b))?(a):(b))
#define min(a,b)	(((a)<=(b))?(a):(b))
#define MAXLINE		(2048)

int cntr, just_once;

double maxmem = 512.*1024.*1024;/* defailt memory limit for bitstate, per run 512 MB */
int r_seed = 123;		/* seed for random nr generator */
int max_w;			/* derived from maxmem setting later */

int min_k = 1;
int max_k = 4;

long max_d = 10000;		/* max depth */
long min_d = 128;		/* min depth */

int maxcpu = 2;			/* nr of cpus available */
int maxremote = 0;		/* cfg file only */

int d_increment;		/* computed */
int script_nr;
int verbose;
int early_term;
int no_bitstate;

long states_sec  = 250000;	/* default nr states stored/second */
long e_vector_sz = 512;		/* estimate for size of statevector */
long sec_available;		/* time limit - derived later */

float hash_factor = 1.5;	/* rough estimate for large statespaces */

double sum_time = 0.0;
double hours_available = 1.0;	/* -tN  -- hours available for runs */

char *ccommon = "-O2";		/* default compile time params */
char *rcommon = "";			/* default runtime parameters */
char *scommon = "";			/* default spin parameters */
char *acommon = "";			/* extra spin params */
char model_name[1024];
char script_name[1024];
char has_rand[1024];	/* this puts a max on the nr of compilation modes */

typedef struct Mode {
	char *cc;		/* a compilation option */
	struct Mode *nxt;	/* linked list */
} Mode;

typedef struct Remote {
	char *machine;		/* name of remote machine */
	int cores;
	struct Remote *nxt;
} Remote;

Mode *modes;
int max_mode;
Remote *remotes;

static char *Swarm_Lib_top[] = {
	"# Default Swarm configuration file",
	"#",
	"# there are four main parts to this configuration file:",
	"#	ranges, limits, compilation options, and runtime options",
	"# the default settings for each are shown below -- edit as needed",
	"# comments start with a # symbol",
	"# This version of swarm requires Spin Version 5.2 or later",
	"",
	"# See the documentation for the additional use of",
	"# environment variables CCOMMON, RCOMMON, and SCOMMON",
	"# http://spinroot.com/swarm/",
	"",
	0,
};

static char *Swarm_Lib_bot[]  = {
	"",
	"# compilation options (each line defines one complete search mode)",
	"# each option currently must include -DBITSTATE and -DPUTPID",
	"",
	"-DBITSTATE -DPUTPID		# basic dfs",
	"-DBITSTATE -DPUTPID -DREVERSE	# reversed process ordering",
	"-DBITSTATE -DPUTPID -DT_REVERSE	# reversed transition ordering",
	"-DBITSTATE -DPUTPID -DREVERSE -DT_REVERSE	# both",

	"-DBITSTATE -DPUTPID -DP_RAND -DT_RAND	# same series with randomization",
	"-DBITSTATE -DPUTPID -DP_RAND -DT_RAND -DT_REVERSE",
	"-DBITSTATE -DPUTPID -DP_RAND -DT_RAND -DREVERSE",
	"-DBITSTATE -DPUTPID -DP_RAND -DT_RAND -DREVERSE -DT_REVERSE",

	"-DBITSTATE -DPUTPID -DBCS	# with bounded context switching",
	"-DBITSTATE -DPUTPID -DBCS -DREVERSE",
	"-DBITSTATE -DPUTPID -DBCS -DT_REVERSE",
	"-DBITSTATE -DPUTPID -DBCS -DREVERSE -DT_REVERSE",

	"",
	"# runtime options (one line)",
	"-c1 -x -n",
		/* -c1 stops at first trail generated */
		/* -n do not generate reachability reports */
		/* -x: do not overwrite existing trail files */
	"",
	"# spin options other than -a (one line)",
	"",
	0,
};

void
usage(void)
{
	fprintf(stderr, "usage: swarm [options]*\n");
	fprintf(stderr, "	file = read configuration parameters from file\n");
	fprintf(stderr, "	-bN = nr of bytes/state in exhaustive mode (default -b%ld)\n", e_vector_sz);
	fprintf(stderr, "	-cN = nr of cpus available (default -c%d)\n", maxcpu);
	fprintf(stderr, "	-dN = max search depth (default -d%ld)\n", max_d);
	fprintf(stderr, "	-e  = allow for early termination when a first error is found\n");
	fprintf(stderr, "	-f model_name = a required argument, no default\n");
	fprintf(stderr, "	-hN = estimated hash-factor (default %f)\n", hash_factor);
	fprintf(stderr, "	-l  = write default configuration file to stdout\n");
	fprintf(stderr, "	-mN = max amount of mem to use per run, optional suffix M or G, (default -m%dM)\n",
				(int) (maxmem/(1024.*1024.)));
	fprintf(stderr, "	-nN = set seed value for random number generator to N (default %d)\n", r_seed);
	fprintf(stderr, "	-sN = estimated nr of states/sec (default -s%ld)\n", states_sec);
	fprintf(stderr, "	-tN = nr of hours available (default -t%.1f)\n", hours_available);
	fprintf(stderr, "	-uN = set minimum search depth to use to N (default %ld)\n", min_d);
	fprintf(stderr, "	-V  = print version number and exit\n");
	fprintf(stderr, "	-v  = verbose (default is non-verbose)\n\n");
	fprintf(stderr, "	define shell variable CCOMMON for compile time params (default CCOMMON=\"%s\")\n", ccommon);
	fprintf(stderr, "	define shell variable RCOMMON for run time parameters (default RCOMMON=\"%s\")\n", rcommon);
	exit(1);
}

#define GB	((double) (1024 * 1024 * 1024))
#define MB	((double) (1024 * 1024))

#define DY	((long) (3600 * 24))
#define HR	((long) (3600))
#define MN	((long) (60))

void
put_lib_file(FILE *fd)
{	int i;

	fprintf(fd, "## %s\n#\n", Version);

	for (i = 0; Swarm_Lib_top[i]; i++)
	{	fprintf(fd, "%s\n", Swarm_Lib_top[i]);
	}

	fprintf(fd, "# range\n");
	fprintf(fd, "k	%d	%d	# optional: to restrict min and max nr of hash functions\n", min_k, max_k);
	fprintf(fd, "\n");
	fprintf(fd, "# limits\n");
	fprintf(fd, "d	%ld		# optional: to restrict the max search depth\n", max_d);
	fprintf(fd, "cpus	%d		# nr available cpus (exact)\n", maxcpu);

	if (maxmem > GB)
	{	fprintf(fd, "memory	%dG\t\t# max memory per run; M=megabytes, G=gigabytes\n", (int) (maxmem/(GB)));
	} else if (maxmem > MB)
	{	fprintf(fd, "memory	%dM\t\t# max memory per run; M=megabytes, G=gigabytes\n", (int) (maxmem/(MB)));
	} else
	{	fprintf(fd, "memory	%ld\t\t# max memory per run; M=megabytes, G=gigabytes\n", (long) maxmem);
	}

	sec_available = (long) (3600.0 * hours_available);
	if (sec_available > DY)
	{	fprintf(fd, "time	%dd\t\t# max time for all runs; h=hours, m=min, s=sec, d=days\n", (int) (sec_available / (DY)));
	} else if (sec_available > HR)
	{	fprintf(fd, "time	%dh\t\t# max time for all runs; h=hours, m=min, s=sec, d=days\n", (int) (sec_available / (HR)));
	} else if (sec_available > MN)
	{	fprintf(fd, "time	%dm\t\t# max time for all runs; h=hours, m=min, s=sec, d=days\n", (int) (sec_available / (MN)));
	} else
	{	fprintf(fd, "time	%ds\t\t# max time for all runs; h=hours, m=min, s=sec, d=days\n", (int) (sec_available / (HR)));
	}

	fprintf(fd, "hash	%.1f		# hash-factor (estimate)\n", hash_factor);
	fprintf(fd, "vector	%ld		# nr of bytes per state (estimate)\n", e_vector_sz);
	fprintf(fd, "speed	%ld		# nr states explored per second (estimate)\n", states_sec);

	if (strlen(model_name) > 0)
	{	fprintf(fd, "file	%s	# file with the spin model to be verified\n", model_name);
	} else
	{	fprintf(fd, "file	model.pml	# file with the spin model to be verified\n");
	}

	for (i = 0; Swarm_Lib_bot[i]; i++)
	{	fprintf(fd, "%s\n", Swarm_Lib_bot[i]);
	}
}

double
width_mem(int w)	/* bits power */
{
	return pow((double) 2.0, (double) (w - 3)); /* bytes of memory */
}

double
width_time(int w)
{	double m;

	/* a hash-factor = #bits / #states */

	m = width_mem(w + 3);		/* get hash-arena size as nr of **bits** */
	m /= (double) hash_factor;	/* estimated #states stored */
	m /= (double) states_sec;	/* how much time this will take in seconds */

	return m;	/* estimated time in seconds */
}

void
configure(void)
{	int nr_steps;

	/* sanity check: */
	if (no_bitstate > 0
	&&  no_bitstate != max_mode)
	{	fprintf(stderr, "error: either all directives should include -DBITSTATE or none should\n");
		exit(1);
	}

	sec_available = (long) (3600.0 * hours_available);

	if (max_d <= min_d)	/* max_d is set by user, adjust default min_d if larger */
	{	min_d = max_d/2;
		if (min_d <= 0)
		{	min_d = max_d;	/* too shallow for depth variations */
	}	}

	/* default maxmem 512 MB gives -w32 */
	for (max_w = (int) (log2(maxmem - 1) + 3.0); max_w > 15; max_w--)	/* -1 MB for stackuse etc */
	{
		if (verbose)	/* debugging */
		{	fprintf(stderr, "-w%d -m%ld..-m%ld\n", max_w, min_d, max_d);
		}

		if (min_d == max_d)
		{	if (width_time(max_w) * max_mode <= maxcpu * sec_available)
			{	d_increment = 1;
				if (verbose)
				{	fprintf(stderr, "nr_steps: 1\n\n");
				}
				return;	/* we found a usable setting for max_w and d_increment */
			}
		} else	/* use either 3, 2, or 1 intermediate depth-constraint */
		for (nr_steps = 3; nr_steps >= 1; nr_steps--)
		{	if (width_time(max_w) * max_mode * (nr_steps+1) <= maxcpu * sec_available)
			{	d_increment = (max_d-min_d)/nr_steps;
				d_increment = max(1, d_increment);
				if (verbose)
				{	fprintf(stderr, "nr_steps: %d incr %d\n\n", nr_steps+1, d_increment);
				}
				return;	/* we found a usable setting for max_w and d_increment */
	}	}	}

	fprintf(stderr, "swarm: cannot schedule jobs within given time bound\n");
	exit(1);
}

void
trail_test(FILE *fd, char *s)
{
#if 0
	fprintf(fd, "echo %s*.trail > _foo_ \n", model_name);
	fprintf(fd, "grep -q \\* _foo_\n");
	fprintf(fd, "if [ $? -ne 0 ]; then echo \"%s\"; rm -f _foo_; exit 1; fi\n", s);
	fprintf(fd, "rm -f _foo_\n\n");
#else
	fprintf(fd, "wc -l *.trail > /dev/null 2>&1\n");
	fprintf(fd, "if [ $? -eq 0 ]\n");
	fprintf(fd, "then echo \"%s\";\n", s);
	fprintf(fd, "     exit 1;\n");
	fprintf(fd, "fi\n");
#endif
}

void
early_termination(FILE *fd)
{
	if (early_term)
	{
#if 0
		fprintf(fd, "\techo \"echo %s*.trail > _foo_\" >>  ${S}%d\n",
			model_name, script_nr);
		fprintf(fd, "\techo \"grep -q \\* _foo_\" >>  ${S}%d\n", script_nr);
		fprintf(fd, "\techo \"if [ \\$? -ne 0 ] || [ -f swarm_times_up ] ; then rm -f _foo_; exit 1; fi\" >> ${S}%d\n",
			script_nr);
		fprintf(fd, "\techo \"rm -f _foo_\" >> ${S}%d\n", script_nr);
#else
		fprintf(fd, "\techo \"wc -l *.trail > /dev/null 2>&1\" >> ${S}%d\n", script_nr);
		fprintf(fd, "\techo \"if [ $? -eq 0 ] || [ -f swarm_times_up ]\" >> ${S}%d\n", script_nr);
		fprintf(fd, "\techo \"then exit 1;\" >> ${S}%d\n", script_nr);
		fprintf(fd, "\techo \"fi\">> ${S}%d\n", script_nr);
#endif

	} else
	{	fprintf(fd, "\techo \"if [ -f swarm_times_up ] ; then exit 1; fi\" >> ${S}%d\n",
			script_nr);
	}
}

void
post_termination(FILE *fd)
{
	if (early_term)
	{
#if 0
		fprintf(fd, "\n\t\techo %s*.trail > _foo_\n", model_name);
		fprintf(fd, "\t\tgrep -q \\* _foo_\n");
		fprintf(fd, "\t\tif [ $? -ne 0 ]\n");
		fprintf(fd, "\t\tthen\n");
		fprintf(fd, "\t\t	rm -f _foo_\n");
		fprintf(fd, "\t\t	break\n");
		fprintf(fd, "\t\telse\n");
		fprintf(fd, "\t\t	rm -f _foo_\n");
		fprintf(fd, "\t\tfi\n\n");
#else
		fprintf(fd, "\n\t\twc -l *.trail > /dev/null 2>&1\n");
		fprintf(fd, "\n\t\tif [ $? -eq 0 ]\n");
		fprintf(fd, "\t\tthen	break\n");
		fprintf(fd, "\t\tfi\n");
#endif
	}
}

void
prelude(FILE *fd)
{	int i;
	Mode *m;
	Remote *rm;

	fprintf(fd, "#!/bin/sh\n\n");
	fprintf(fd, "## %s\n", Version);
	fprintf(fd, "## source: http://www.spinroot.com/swarm\n");
	fprintf(fd, "## ./swarm -c%d -m%.0fM -t%.1f -s%ld -u%ld -d%ld %s -f %s\n\n",
		maxcpu, maxmem/(1024.*1024.), hours_available, states_sec, min_d, max_d,
		verbose?"-v":"", model_name);
	fprintf(fd, "# set -v\n\n");

	fprintf(fd, "PREP=1\n");	/* default values */
	fprintf(fd, "XEC=1\n");
	fprintf(fd, "S=\"script\"\n\n");

	fprintf(fd, "while [ $# -gt 0 ]\n");
	fprintf(fd, "do\n");
	fprintf(fd, "	case \"$1\" in\n");
	fprintf(fd, "	prep)\n");
	fprintf(fd, "		echo \"compiles and setup only\"\n");
	fprintf(fd, "		XEC=0\n");
	fprintf(fd, "		;;\n");
	fprintf(fd, "	exec)\n");
	fprintf(fd, "		echo \"script execution only\"\n");
	fprintf(fd, "		PREP=0\n");
	fprintf(fd, "		;;\n");
	fprintf(fd, "	base)\n");
	fprintf(fd, "		if [ $# -gt 1 ]\n");
	fprintf(fd, "		then\n");
	fprintf(fd, "			S=$2\n");
	fprintf(fd, "			echo \"using $S as script basename\"\n");
	fprintf(fd, "			shift\n");
	fprintf(fd, "		else\n");
	fprintf(fd, "			echo \"missing arg in: base name\"\n");
	fprintf(fd, "			exit 1\n");
	fprintf(fd, "		fi\n");
	fprintf(fd, "		;;\n");
	fprintf(fd, "	*)	echo \"bad arg $1, should be prep or exec\"\n");
	fprintf(fd, "		exit 1\n");
	fprintf(fd, "		;;\n");
	fprintf(fd, "	esac\n");
	fprintf(fd, "	shift\n");
	fprintf(fd, "done\n");
	fprintf(fd, "\n");

	fprintf(fd, "if [ $PREP -eq 0 ] && [ $XEC -eq 0 ]\n");
	fprintf(fd, "then	echo \"error: choose 'exec' or 'prep' (default is to do both)\"\n");
	fprintf(fd, "	exit 0\n");
	fprintf(fd, "fi\n");
	fprintf(fd, "# exit 0\n\n");

	trail_test(fd, "remove .trail file first");

	if (remotes)
	{	fprintf(fd, "# start up the remote executions:\n");
		fprintf(fd, "case `hostname` in\n");
		for (rm = remotes; rm; rm = rm->nxt)
		{	fprintf(fd, "	%s)\n", rm->machine);
			fprintf(fd, "	\t;;\n");
		}
		fprintf(fd, "	*)\n");
		for (rm = remotes; rm; rm = rm->nxt)
		{	fprintf(fd, "	\tscp %s %s:%s\n", model_name, rm->machine, model_name);
			fprintf(fd, "	\tscp %s %s:%s\n", script_name, rm->machine, script_name);

			fprintf(fd, "	\tif [ $XEC -eq 0 ]\n");
			fprintf(fd, "	\tthen\n");
			fprintf(fd, "	\t\tssh %s \"sh ./%s prep\" &\n", rm->machine, script_name);
			fprintf(fd, "	\telse\n");
			fprintf(fd, "	\t\tssh %s \"sh ./%s\" &\n", rm->machine, script_name);
			fprintf(fd, "	\tfi\n");
		}
		fprintf(fd, "		;;\n");
		fprintf(fd, "esac\n\n");
	}

	fprintf(fd, "if [ $PREP -eq 1 ]\n");
	fprintf(fd, "then\n");

	fprintf(fd, "	if [ -f %s ]\n", model_name);
	fprintf(fd, "	then	spin %s %s -a %s\n", scommon, acommon, model_name);
	fprintf(fd, "		if [ $? -ne 0 ] ; then exit 1; fi\n");
	fprintf(fd, "		if [ -f pan.c ] ; then true; else echo \"error: no pan.c\"; exit 1; fi\n\n");
	fprintf(fd, "	fi\n");
	fprintf(fd, "	if [ -f pan.c ]\n");
	fprintf(fd, "	then\n");

	for (i = 0, m = modes; m; m = m->nxt)
	{	fprintf(fd, "\t\tcc ");
		if (strstr(rcommon, "-a") == NULL)
		{	fprintf(fd, "-DSAFETY ");
		}
		if (no_bitstate > 0)
		{	fprintf(fd, " -DMEMLIM=%d %s %s -o pan%d",
				(int) (maxmem/(1024.*1024.)),
				m->cc, ccommon, ++i);
		} else
		{	fprintf(fd, " %s %s -o pan%d",
				m->cc, ccommon, ++i);
		}
		if (strstr(ccommon, "pan.c") == NULL)
		{	fprintf(fd, " pan.c");
		}
		fprintf(fd, "\n");
	}

	fprintf(fd, "	else\n");
	fprintf(fd, "		echo \"swarm: no pan.c; cannot proceed\"\n");
	fprintf(fd, "		exit 1\n");
	fprintf(fd, "	fi\n");

	for (i = 0; i < maxcpu; i++)
	{	fprintf(fd, "	echo \"#!/bin/sh\" > ${S}%d\n", i);
	}
}

int
gen_runs(FILE *fd, int w, long d, long k)
{	static int x = 0;	/* mode: number of ways that pan was compiled */
	double next_t = width_time(w);
	int h = rand()%100;	/* pick hashfunction at random */

	if (verbose)
	{	fprintf(stderr, "pan_%d gen_runs -w%d -m%ld -h%d -k%ld\t-- have: %ld used: %.2f next: %.2f\n",
			x, w, d, h, k, sec_available, sum_time / ((double) maxcpu), next_t);
	}

	if (k > 4)	/* wont happen within the default range of 1..4 */
	{	next_t *= ((double) k)/4.0;
	}

	if (sec_available - (sum_time / (double) maxcpu) < next_t)
	{	return 0;	/* additional run would exceed time limit */
	}

	sum_time += next_t;			/* total time used */
	cntr++;					/* total number of runs */
	x = (x >= max_mode) ? 1 : (x+1);	/* pan executable to use */

	if (no_bitstate == max_mode)	/* all jobs are non-bitstate -- should use different next_t assumption */
	{	fprintf(fd, "\n\techo \"./pan%d -w%d -m%ld -h%d %s\t# %.1f sec (+ %.1f s, %.1f MB)\" >> ${S}%d\n",
			x, w, d, h, rcommon, sum_time, next_t,
			width_mem(w)/(1024.0*1024.0), script_nr);
	} else
	{	fprintf(fd, "\n\techo \"./pan%d -k%ld -w%d -m%ld -h%d ", x, k, w, d, h);
		if (has_rand[x-1] > 0)
		{	fprintf(fd, "-RS%ld ", seed_val[just_once]);
			just_once = (just_once + 1) % 1000; /* use each value once */
		}
		fprintf(fd, "%s\t# %.1f sec (+ %.1f s, %.1f MB)\" >> ${S}%d\n",
			rcommon, sum_time, next_t,
			width_mem(w)/(1024.0*1024.0), script_nr);
	}

	early_termination(fd);

	script_nr = (script_nr + 1) % maxcpu;

	return 1;
}

void
postlude(FILE *fd)
{	int i, j;
	double time_left;
	Remote *rm;

	fprintf(stderr, "swarm: %d runs, avg time per cpu %.1f sec\n",
		cntr, sum_time / (float) maxcpu);

	time_left = ((maxcpu * sec_available) - sum_time)/(double) maxcpu; /* per cpu */

	if (verbose)
	{	fprintf(stderr, "swarm: an exhaustive dfs or bfs run could cover:\n");
		fprintf(stderr, "swarm: up to %.0fM states of %ld bytes in %.0f seconds\n",
			maxmem / ((double) e_vector_sz* (1024.*1024.)), e_vector_sz,
			maxmem / ((double) (states_sec*e_vector_sz)) );

		fprintf(stderr, "swarm: CPUs %d, Mem %.1f MB, Time %ld sec Depth %ld-%ld Width %d-%d\n",
			maxcpu, maxmem/(1024.*1024.), sec_available, min_d, max_d, max_w-1, max_w);
		fprintf(stderr, "swarm: time remaining %.1f seconds per cpu ", time_left);
		fprintf(stderr, "(%2.1f%% of available time)\n",
			(100.0*time_left)/sec_available);
		fprintf(stderr, "swarm: time remaining %5.1f of %5.1f minutes per cpu\n",
			(time_left / 60.0), 60.0*hours_available);
		fprintf(stderr, "swarm: time remaining %5.1f of %5.1f hours   per cpu\n",
			(time_left / 3600.0), hours_available);
	}

	for (i = 0; i < maxcpu; i++)
	{	fprintf(fd, "\n");
		fprintf(fd, "\techo \"date > swarm_done_s%d\" >> ${S}%d\n", i, i);
		fprintf(fd, "\techo \"exit 1\" >> ${S}%d\n", i);
	}

	fprintf(fd, "fi\n# end of preparation\n\n");
	fprintf(fd, "if [ $XEC -eq 1 ]\n");
	fprintf(fd, "then\n");

	fprintf(fd, "	rm -f swarm_done_s*\n");
	fprintf(fd, "	rm -f swarm_times_up\n");

	fprintf(fd, "\n\tcase `hostname` in\n");
	j = 0;
	for (rm = remotes; rm; rm = rm->nxt)
	{	fprintf(fd, "\t%s)\n", rm->machine);
		for (i = 0; i < rm->cores; i++, j++)
		{	fprintf(fd, "\t\tsh ./${S}%d > ${S}%d.out", j, j);
			if (i+1 < rm->cores)
			{	fprintf(fd, " &");
			}
			fprintf(fd, "\n");
		}
		fprintf(fd, "\t\t;;\n");
	}
	fprintf(fd, "	*)\n");
	for ( ; j < maxcpu; j++)
	{	fprintf(fd, "\t\tsh ./${S}%d > ${S}%d.out", j, j);
		if (j+1 < maxcpu)
		{	fprintf(fd, " &");
		}
		fprintf(fd, "\n");
	}
	fprintf(fd, "		;;\n");
	fprintf(fd, "	esac\n\n");

	fprintf(fd, "	tt=0\n");
	fprintf(fd, "	sd=0\n");
	fprintf(fd, "	while [ $sd -eq 0 ]\n");
	fprintf(fd, "	do\n");

	fprintf(fd, "\t\tcase `hostname` in\n");
	j = 0;
	for (rm = remotes; rm; rm = rm->nxt)
	{	fprintf(fd, "\t\t%s)\n", rm->machine);
		fprintf(fd, "\t\t\tif");
		for (i = 0; i < rm->cores; i++, j++)
		{	fprintf(fd, " [ -f swarm_done_s%d ] ", j);
			if (i+1 < rm->cores) fprintf(fd, "&&");
		}
		fprintf(fd, "\n\t\t\tthen\tsd=1; break\n");
		fprintf(fd, "\t\t\tfi\n");
		fprintf(fd, "\t\t\t;;\n");
	}
	fprintf(fd, "\t	*)\n");
		fprintf(fd, "\t\t\tif");
		for ( ; j < maxcpu; j++)
		{	fprintf(fd, " [ -f swarm_done_s%d ] ", j);
			if (j+1 < maxcpu) fprintf(fd, "&&");
		}
		fprintf(fd, "\n\t\t\tthen\tsd=1; break\n");
		fprintf(fd, "\t\t\tfi\n");
		fprintf(fd, "\t\t\t;;\n");
	fprintf(fd, "\t	esac\n\n");

	post_termination(fd);
	fprintf(fd, "		if [ $tt -le %ld ]\n", sec_available);	/* time exceeded? */
	fprintf(fd, "		then\n");
	fprintf(fd, "			tt=`expr $tt + 10`\n");
	fprintf(fd, "			sleep 10\n");	/* wait some more */
	fprintf(fd, "		else\n");
	fprintf(fd, "			date > swarm_times_up\n");
	fprintf(fd, "			wait\n");
	fprintf(fd, "			rm -f swarm_times_up\n");
	fprintf(fd, "			break\n");
	fprintf(fd, "		fi\n");
	fprintf(fd, "	done\n");
	fprintf(fd, "	rm -f swarm_done_s* pan.? pan[0-9]*\n\n");

	if (remotes)
	{	fprintf(fd, "\techo \"if needed: retrieve remote results:\"\n\n");
		for (rm = remotes; rm; rm = rm->nxt)
		{	fprintf(fd, "\techo \"scp %s:*.trail .\"\n", rm->machine);
		}
	}

	fprintf(fd, "fi\n");		/* execute */
}

char *
skip_white(char *b)
{	char *p;

	for (p = b; *p != '\0'; p++)
	{	if (*p != ' ' && *p != '\t')
		{	break;
	}	}

	return p;
}

char *
skip_nonwhite(char *b)
{	char *p;

	for (p = b; *p != '\0'; p++)
	{	if (*p == ' ' || *p == '\t')
		{	break;
	}	}

	return p;
}

void
add_mode(char *s)
{	Mode *m = (Mode *) malloc(sizeof(Mode));
	int i;

	m->cc = (char *) malloc(strlen(s)+1);
	strcpy(m->cc, s);
	m->nxt = modes;
	modes = m;

	if (strstr(s, "-DBITSTATE") == NULL)
	{	no_bitstate++;
	}
	max_mode++;

	for (m = modes, i = 0; m; m = m->nxt, i++)
	{	/* recompute, because the order is reversed, sigh */
		if (i < 1024
		&&  (strstr(m->cc, "-DP_RAND") != NULL
		||   strstr(m->cc, "-DT_RAND") != NULL
		||   strstr(m->cc, "-DRANDSTOR") != NULL))
		{	has_rand[i] = 1;	/* means pan$i uses randomization */
/* printf("%d has rand: %s\n", i, m->cc); */
		} else
		{	has_rand[i] = 0;
/* printf("%d does NOT have rand: %s\n", i, m->cc); */
	}	}
/* printf("\n"); */
}

void
get_cpus(char *ptr)
{	char rname[512];
	int n = 0;
	Remote *nr;

	maxcpu = 0;	/* override default */
	maxremote = 0;

	ptr = skip_nonwhite(ptr);
	ptr = skip_white(ptr);

	while (*ptr != 0 )
	{
		if (isdigit((int) *ptr))
		{	n = atoi(ptr);	/* local cpu-cores */
			maxcpu += n;
			if (verbose) fprintf(stderr, "swarm debug: local with %d cpus\n", n);
		} else if (sscanf(ptr, "%[^:]:%d", rname, &n) == 2)
		{	maxcpu += n;	/* all available cpus */
			maxremote += n;
			nr = (Remote *) malloc(sizeof(Remote));
			nr->machine = (char *) malloc(strlen(rname)+1);
			strcpy(nr->machine, rname);
			nr->cores = n;
			nr->nxt = remotes;
			remotes = nr;
			if (verbose) fprintf(stderr, "swarm debug: remote '%s' with %d cpus\n", rname, n);
		} else
		{	fprintf(stderr, "swarm: unrecognized specifier '%s'\n", ptr);
			exit(1);
		}
		if (n <= 0)
		{	fprintf(stderr, "swarm: nr of cpus must be positive\n");
			exit(1);
		}
		ptr = skip_nonwhite(ptr);
		ptr = skip_white(ptr);
	}
}

void
parse_file(char *f)
{	FILE *fd;
	char buf[MAXLINE];
	char *ptr;
	int omode = 0;

	if ((fd = fopen(f, "r")) == NULL)
	{	fprintf(stderr, "swarm: cannot open configuration file '%s'\n", f);
		exit(1);
	}

	if (!fgets(buf, MAXLINE, fd))
	{	fprintf(stderr, "swarm: empty configuration file '%s'\n", f);
		exit(1);
	}

	if (strstr(buf, "## Swarm Version") == NULL)
	{	fprintf(stderr, "swarm: saw: '%s'\n", buf);
		fprintf(stderr, "swarm: outdated config file '%s', upgrade to %s\n", f, Version);
		exit(1);
	}

	fprintf(stderr, "swarm: reading settings from '%s'\n", f);

	while (fgets(buf, MAXLINE, fd))
	{	if (buf[0] == '#')
		{	if (strstr(buf, "compilation options"))
			{	omode = 1;
			} else if (strstr(buf, "runtime options"))
			{	omode = 2;
			} else if (strstr(buf, "spin options"))
			{	omode = 3;
			}
			continue;	/* comment */
		}
		if ((ptr = strchr(buf, '#')) != NULL)
		{	*ptr = '\0';
		}
		if ((ptr = strchr(buf, '\r')) != NULL)
		{	*ptr = '\0';
		}
		if ((ptr = strchr(buf, '\n')) != NULL)
		{	*ptr = '\0';
		}
		ptr = &buf[strlen(buf)-1];
		while (*ptr == ' ' || *ptr == '\t')
		{	*ptr = '\0';
			ptr--;
		}

		ptr = strchr(&buf[0], '#');
		if (ptr)
		{	*ptr = '\0'; /* strip comments */
		}

		ptr = skip_white(&buf[0]);

		switch (*ptr) {
		case 'c': /* cpus */
			get_cpus(ptr);
			break;

		case 'd': /* depth */
			ptr = skip_nonwhite(ptr);
			ptr = skip_white(ptr);
			max_d = atoi(ptr);
			break;

		case 'h': /* hash-factor */
			ptr = skip_nonwhite(ptr);
			ptr = skip_white(ptr);
			hash_factor = (float) atof(ptr);
			break;

		case 'k': /* hash functions */
			ptr = skip_nonwhite(ptr);
			ptr = skip_white(ptr);
			min_k = atoi(ptr);
			ptr = skip_nonwhite(ptr);
			ptr = skip_white(ptr);
			max_k = atoi(ptr);
			break;

		case 'm': /* memory */
			ptr = skip_nonwhite(ptr);
			ptr = skip_white(ptr);
			maxmem = (double) atoi(ptr);
			if (strchr(ptr, 'M'))
			{	maxmem *= 1024.*1024.;		/* megabytes */
			} else if (strchr(ptr, 'G'))
			{	maxmem *= 1024.*1024.*1024.;	/* gigabytes */
			}
			break;

		case 't': /* time */
			ptr = skip_nonwhite(ptr);
			ptr = skip_white(ptr);
			hours_available = (double) atof(ptr);
			if (strchr(ptr, 's') != NULL)	/* seconds */
			{	hours_available /= 3600.0;
			} else if (strchr(ptr, 'm') != NULL)	/* minutes */
			{	hours_available /= 60.0;
			} else if (strchr(ptr, 'd') != NULL)	/* days */
			{	hours_available *= 24.0;
			}	/* default is hours */
			break;

		case 'b': /* bytes per state */
		case 'v': /* vector (synonym) */
			ptr = skip_nonwhite(ptr);
			ptr = skip_white(ptr);
			e_vector_sz = atoi(ptr);
			break;

		case 's': /* speed */
			ptr = skip_nonwhite(ptr);
			ptr = skip_white(ptr+1);
			states_sec = (long) atoi(ptr);
			break;

		case 'f': /* model */
			ptr = skip_nonwhite(ptr);
			ptr = skip_white(ptr);
			strcpy(model_name, ptr);
			break;

		case '-': /* compilation or runtime option */
			ptr++;
			if (omode == 1) /* compilation options */
			{	add_mode(buf);
			} else if (omode == 2)	/* runtime options */
			{	rcommon = malloc(strlen(buf)+1);
				strcpy(rcommon, buf);
			} else if (omode == 3)
			{	acommon = malloc(strlen(buf)+1);
				strcpy(acommon, buf);
			} else
			{	fprintf(stderr, "swarm: unrecognized field %s\n", buf);
			}
			break;
		case '\0':
			break;
		default:
			fprintf(stderr, "swarm: unknown option %s\n", buf);
			break;
	}	}

	fclose(fd);
}

int
main(int argc, char *argv[])
{	char *ptr;
	int k, y;
	long d;
	FILE *fd = stdout;

	argc--;
	argv++;
	while (argc > 0)
	{	if (argv[0][0] != '-')
		{	parse_file(argv[0]);
		} else
		switch(argv[0][1]) {
		case 'b':	e_vector_sz = atoi(&argv[0][2]);
				break;

		case 'c':	maxcpu = atoi(&argv[0][2]);
				break;

		case 'd':	max_d = atoi(&argv[0][2]);
				break;

		case 'e':	early_term = 1;
				break;

		case 'f':	strcpy(model_name, argv[1]);
				argc--; argv++;
				break;

		case 'h':	hash_factor = atof(&argv[0][2]);
				break;

		case '1':	/* likely confusion */
		case 'l':	put_lib_file(stdout);
				exit(0);
				break;	/* keeps uno happy */

		case 'm':	maxmem = (double) atoi(&argv[0][2]);
				if (strchr(&argv[0][2], 'M'))
				{	maxmem *= 1024.*1024.;		/* megabytes */
				} else if (strchr(&argv[0][2], 'G'))
				{	maxmem *= 1024.*1024.*1024.;	/* gigabytes */
				}
				break;

		case 'n':	r_seed = atoi(&argv[0][2]);
				break;

		case 's':	states_sec = (long) atoi(&argv[0][2]);
				break;

		case 't':	hours_available = (double) atof(&argv[0][2]);
				break;

		case 'u':	min_d = atoi(&argv[0][2]);
				break;

		case 'v':	verbose = 1;
				break;

		case 'V':	printf("%s\n", Version);
				exit(0);
				break;	/* keeps uno happy */

		default:	fprintf(stderr, "swarm: error, unrecognized parameter '%s'\n", argv[0]);
				/* fallthru */
		case '-':	/* e.g., --help */
				usage();
				break;
		}
		argc--;
		argv++;
	}

	srand(r_seed);

	if (!modes)
	{	add_mode("-DBITSTATE -DPUTPID -DP_RAND -DT_RAND");
		add_mode("-DBITSTATE -DPUTPID -DP_RAND -DT_RAND -DT_REVERSE");
		add_mode("-DBITSTATE -DPUTPID -DP_RAND -DT_RAND -DREVERSE");
		add_mode("-DBITSTATE -DPUTPID -DP_RAND -DT_RAND -DREVERSE -DT_REVERSE");
	}

	if (maxcpu < 1)
	{	fprintf(stderr, "swarm: error, nr of cpus (-c%d) must be >= 1\n", maxcpu);
		usage();
	}
	if (maxmem <= 2.)
	{	fprintf(stderr, "swarm: error, maxmem (-m%g) has to be >= 2.0\n", maxmem);
		usage();
	}
	if (min_d > max_d)
	{	fprintf(stderr, "swarm: error, min_d (-u%ld) has to be <= max_d (-d%ld)\n", min_d, max_d);
		usage();
	}
	if (strcmp(model_name, "") == 0)
	{	fprintf(stderr, "swarm: no model name specified\n");
		usage();
	}

	strncpy(script_name, model_name, sizeof(script_name) - (strlen(".swarm") + 1));
#if 0
	ptr = strchr(script_name, '.');
	if (ptr != NULL)
	{	*ptr = '\0';
	}
#endif
	strcat(script_name, ".swarm");
	if ((fd = fopen(script_name, "w")) == NULL)
	{	fprintf(stderr, "swarm: cannot create %s\n", script_name);
		exit(1);
	}

	ptr = getenv("CCOMMON"); if (ptr) ccommon = ptr;
	ptr = getenv("RCOMMON"); if (ptr) rcommon = ptr;
	ptr = getenv("SCOMMON"); if (ptr) scommon = ptr;

	configure();	/* compute values for max_w and d_increment */

	prelude(fd);

	/* the basic sequence */
	for (k = min_k; ; )
	for (d = max_d; d >= min_d; d -= d_increment)
	for (y = 0; y < max_mode; y++)	/* once for each compilation mode */
	{	if (gen_runs(fd, max_w, d, k) == 0)	/* exceeding time-limit */
		{	goto done;	/* the only way to exit the loops */
		}
		k = (k == max_k) ? min_k : (k+1);
	}

done:
	fprintf(fd, "\n# fill up rest of time %ld sec\n",
		sec_available - (long) (sum_time / (double) maxcpu));

	if (verbose)
	{	fprintf(stderr, "\nfill up remainder\n\n");
	}

	while (max_w > 18)	/* to fill up any left-over time */
	{	if (gen_runs(fd, max_w, max_d, /* fixed k: */ 1) == 0)
		{	max_w--;	/* faster smaller runs */
	}	}

	postlude(fd);

	chmod(script_name, S_IXUSR | S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
	fprintf(stderr, "swarm: script written to %s\n", script_name);

	if (verbose)
	{	fprintf(stderr, "swarm: unused time %ld sec\n",
			sec_available - (long) (sum_time / (double) maxcpu));
	}
	fclose(fd);

	return 0;
}
