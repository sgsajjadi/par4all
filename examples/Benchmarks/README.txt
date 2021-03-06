Some Par4All benchmarks:
========================


You can build different versions of these test cases, the sequential
version, the OpenMP automatically parallelized version, the naïve CUDA
version, and the same CUDA kernels with optimized communications.  We ship
also PGI and HMPP versions for each of these cases.

The targets at top level are respectively "seq", "openmp", "cuda",
"cuda_opt", "pgi", "hmpp", and "all".

If you want only one, for instance polybenchs/2mm, then go in 2mm
subdirectory and run "make 2mm_seq" ; "make 2mm_openmp" ; ....

You can run the test cases using "make run_seq" ; "make run_openmp" ; ....
To run only one test, go to the subdirectory of the test and run the commands 
above. It generate the executable too if needed.

Execution time in milliseconds is recorded in a SQLite database (if
sqlite3 is available) or in a .csv file located at top level (in Benchmarks 
directory). The measure doesn't include array initialization (we suppose the 
main process is intended to be part of a larger framework) but will include 
all communications.

You can personalize the name of the version for a run (no space
allowed) with for instance:
VERSION="Cuda-optimized-12-august" make run_cuda_opt

The script scripts/generate_speedup.sh can generate a gnuplot script that
draws the speedup from the SQLite database. It can be parameterized with
environment variables, for instance :

dbfile=my.timing.sqlite tests="2mm hotspot99 correlation" ref_ver="The_reference_version_name" versions="Cuda-optimized-12-august Cuda-naive OpenMP OpenMP-Fine" ./scripts/generate_speedup.sh

Here is the list of possible params:

  dbfile: name of sqlite db, default to timing.sqlite

  tests: spaces separated list of testcase, default to all tests in the DB

  ref_ver: speedup will be computed taking this version as reference,
  default to "run_seq"

  versions: spaces separated list of version to display, default to all
  available in the DB

  exclude_tests: spaces separated list of testcase to exclude, default to
  "", cannot be used with 'tests' option

  disable_mean: disable geometric mean display in the histogram, default
  is "" (activated)

  title: set a title for the gnuplot graph

  labelfontsize: set fontsize for label


Once the script scripts/generate_speedup.sh has been run, a file named 
"histogram.gp" is generated. To draw the graphical output of the speedups, 
run the command "gnuplot histogram.gp". The output is drawn in a file 
"speedup.eps" 


Example with the test 2mm (step by step):

 Below are the different steps to generate a graphical output for the test 
2mm, using a sqlite database. In the different steps is explained how to 
create and run two versions of the test: openmp and sequential. However, 
it is possible to run only one of them or to run other versions for the 
test (cuda  for example)

 - Go to 2mm subdirectory
 - Create the sequential and/or openmp versions of the test 2mm: 
   "make 2mm_seq" and/or "make 2mm_openmp"
 - Run only the test 2mm: "make run_openmp" and/or "make run_seq". 
   It will also create the timing.sqlite db file located in "Benchmarks" 
   directory. All the results for all the tests that have been run are saved 
   in this db file
 - Generate the "histogram.gp" file containing the data for drawing the 
   graphical outputs:
	- To generate the file containing all results (sequential and openmp) 
          for the test 2mm: ' tests="2mm" ./scripts/generate_speedup.sh '
	- To generate the file containing only openmp results for the test 
          2mm: 'versions="run_openmp" tests="2mm" ./scripts/generate_speedup.sh'
 - Draw the graphical output: "gnuplot histogram.gp". The output is drawn in a 
   file "speedup.eps"




%%% Local Variables:
%%% mode: latex
%%% ispell-local-dictionary: "american"
%%% TeX-auto-untabify: t
%%% End:

% vim:spell spelllang=en
