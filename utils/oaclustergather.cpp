/** \file oaclustergather.cpp

 C++ program: oaclustergather

 Dedicated tool to gather data on cluster

 Author: Pieter Eendebak <pieter.eendebak@gmail.com>, (C) 2013

 Copyright: See LICENSE.txt file that comes with this distribution
*/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <numeric>

#include "arraytools.h"
#include "anyoption.h"
#include "tools.h"
#include "arrayproperties.h"

#include "oadevelop.h"
#include "pareto.h"



/**
 * @brief Analyse arrays using Pareto optimality
 * @param argc
 * @param argv[]
 * @return
 */
int main ( int argc, char* argv[] )
{

	/* parse command line options */
	AnyOption opt;
	opt.setFlag ( "help", 'h' );   /* a flag (takes no argument), supporting long and short form */
	opt.setFlag ( "coptions" );	/* print compile time options */
	opt.setOption ( "output", 'o' );
	opt.setOption ( "verbose", 'v' );
	opt.setOption ( "basedir", 'b' );
	opt.setOption ( "split0" );
	opt.setOption ( "numbersfile" );
	opt.setOption ( "config", 'c' );
	opt.setOption ( "nsplit0" );
	opt.setOption ( "nsplit1" );
	opt.setOption ( "kmin" );
	opt.setOption ( "kmax" );

	opt.addUsage ( "Orthonal Array Cluster Gather: special tool" );
	opt.addUsage ( "Usage: oaclustergather [OPTIONS] [FILE]" );
	opt.addUsage ( "" );
	opt.addUsage ( " -h --help  			Prints this help " );
	opt.addUsage ( " -v --verbose  			Verbose level (default: 1) " );
	opt.addUsage ( " -o [FILE] --output [FILE]	Output prefix for filtered arrays (default: no output) " );
	opt.processCommandArgs ( argc, argv );

	print_copyright();

	/* parse options */
	if ( opt.getFlag ( "help" ) || opt.getFlag ( 'h' ) || opt.getArgc() ==0 || ( opt.getValue ( "coptions" ) != 0 ) ) {
		opt.printUsage();
		if ( opt.getValue ( "coptions" ) != 0 ) {
			print_options ( cout );
		}
		exit ( 0 );
	}

	const char *outputprefix = 0;
	if ( opt.getValue ( "output" ) !=0 )
		outputprefix = opt.getValue ( 'o' );

	const std::string basedir = opt.getStringValue ( "basedir", "" );
	const char *configfile = opt.getStringValue ( "config", "oaconfig.txt" );
	int verbose = opt.getIntValue ( 'v', 1 );
	int split0 = opt.getIntValue ( "split0", 0 );
	int nsplit0 = opt.getIntValue ( "nsplit0", -1 );
	int nsplit1 = opt.getIntValue ( "nsplit1", 1 );
	int kmin = opt.getIntValue ( "kmin", 9 );
	int kmax = opt.getIntValue ( "kmax", 24 );
	const char *numbersfile = opt.getStringValue ( "numbersfile", 0 );

	arraydata_t *adata = readConfigFile ( configfile );
	assert ( adata!=0 );

	if ( verbose ) {
		printf ( "oaclustergather: basedir %s, split0 %d, nsplit1 %d, kmin %d, kmax %d, verbose %d\n", basedir.c_str(), split0, nsplit1, kmin, kmax, verbose );
	}
	if ( verbose ) {
		std::cout << "#time start: "<< currenttime() << std::endl;
	}
	double time0=get_time_ms();

	/* Loop over all subdirectories for all number of columns */

	std::vector<long> na ( kmax+1 );	  /// total number of arrays
	std::vector<long> npareto ( kmax+1 ); /// total number of pareto arrays

	int cleanrun=1; /// indicates whether all necessary files have been found

	// loop over all columns
	for ( int k=kmin; k<=kmax; k++ ) {

		if ( verbose>=2 )
			printf ( "oaclustergather: %d columns (time %.1f [s])\n", k, get_time_ms()-time0 );
		Pareto<mvalue_t<long>,array_link> pset;

		arraydata_t adata0 ( adata, k );

		if ( nsplit0==-1 ) {
			if ( verbose )
				printf ( "oaclustergather: %d columns, gathering results for second stage: split0 %d (time %.1f [s])\n", k, split0, get_time_ms()-time0 );
			for ( int jj=0; jj<nsplit1; jj++ ) {
				std::string subdir =  printfstring ( "sp0-split-%d/sp1-split-%d/", split0, jj );
				std::string subfile = printfstring ( "sp0-split-%d-sp1-split-%d-extend-%s.oa", split0, jj, adata0.idstr().c_str() );
				std::string afile = basedir + "/" + subdir + subfile;
				int nn = nArrays ( afile.c_str() );
				// printf ( "file %s: %d arrays\n", afile.c_str(), nn );

				if ( verbose>=2 )
					printf ( "file %s: %d arrays\n", subfile.c_str(), nn );
				if ( nn>=0 )
					na[k] += nn;
				else {
					if (cleanrun) {	// only report error if we are in a clean run 
					fprintf ( stderr, "file %s: error\n", subfile.c_str() );
					}
					cleanrun=0;
				}

				const arraylist_t arraylist = readarrayfile ( afile.c_str(), 0 );

				#pragma omp parallel for
				for ( int i=0; i<(int)arraylist.size(); i++ ) {
					if ( verbose>=3 || ( ( i%5000==0 ) && verbose>=2 ) ) {
						printf ( "oaclustergather: file %d, array %d/%zu\n", jj, i, arraylist.size() );
						printf ( "  " );
						pset.show ( 1 );
//#ifdef OPENMP
						//printf(" openmp: %d\n",  omp_get_num_threads() );
//#endif

					}

					const array_link &al = arraylist.at ( i );

					//parseArrayPareto ( al, al, pset, verbose );
					 Pareto<mvalue_t<long>, array_link >::pValue p = calculateArrayPareto<array_link> ( al, verbose>=3 );
					//typename
					 
					#pragma omp critical
					{
						// add the new tuple to the Pareto set
						pset.addvalue ( p, al );
					}
				}

				if ( verbose>=2 || ( ( jj%20==0 || ( jj==nsplit1-1 ) ) && verbose>=1 ) ) {
					printf ( "oaclustergather: file %d/%d, %zu arrays: %d Pareto values, %d Pareto elements\n", jj, nsplit1, arraylist.size(), pset.number(), pset.numberindices() );
					//printf ( "  " ); pset.show ( 1 );
				}
			}
		} else {
			//   verbose=2; // @pte
			if ( verbose )
				printf ( "oaclustergather: gathering results for first stage\n" );


			for ( int jj=0; jj<nsplit0; jj++ ) {
				std::string subdir =  printfstring ( "sp0-split-%d/", jj );
				std::string subfile = printfstring ( "sp0-split-%d-extend-%s.oa", jj, adata0.idstr().c_str() );
				std::string afile = basedir + "/" + subdir + subfile;
				int nn = nArrays ( afile.c_str() );
				// printf ( "file %s: %d arrays\n", afile.c_str(), nn );

				if ( verbose>=2 )
					printf ( "file %s: %d arrays\n", subfile.c_str(), nn );
				if ( nn>=0 ) {
					na[k] += nn;
					arraylist_t arraylist = readarrayfile ( afile.c_str(), 0 );
					for ( size_t i=0; i<arraylist.size(); i++ ) {
						if ( verbose>=2 || ( ( i%10000==0 ) && verbose>=1 ) ) {
							printf ( "oaclustergather: file %d/%d %s, array %zu/%zu: %d Pareto values, %d Pareto elements\n", jj, nsplit0, subfile.c_str(), i, arraylist.size(), pset.number(), pset.numberindices() );
							//printf ( "  " ); pset.show ( 1 );
						}

						const array_link &al = arraylist.at ( i );
						parseArrayPareto ( al, al, pset, verbose );
					}
				} else {
					fprintf ( stderr, "file %s: error\n", subfile.c_str() );
					cleanrun=0;
				}
			}
		}

		if ( verbose ) {
			printf ( "final pareto set %d cols: ", k );
			pset.show ( 2 );
		}

		//           verbose=2; // @pte

		// write pareto set to disk

		arraylist_t pp = pset.allindicesdeque();
		npareto[k] =  pset.number();

		if ( cleanrun ) {
			if ( nsplit0==-1 ) {
				std::string cdir =  printfstring ( "sp0-split-%d/", split0 );
				std::string pfile = printfstring ( "sp0-split-%d-pareto-%s.oa", split0, adata0.idstr().c_str() );
				std::string afile = basedir + "/" + cdir + pfile;

				if ( verbose )
					printf ( "  writing pareto file %s\n", pfile.c_str() );
				writearrayfile ( afile.c_str(), &pp, ABINARY, adata->N, k );
			} else {
				std::string cdir =  printfstring ( "" );
				std::string pfile = printfstring ( "results-j5evenodd-pareto-%s.oa", adata0.idstr().c_str() );
				std::string afile = basedir + "/" + cdir + pfile;

				if ( verbose )
					printf ( "  writing pareto file %s\n", pfile.c_str() );
				writearrayfile ( afile.c_str(), &pp, ABINARY, adata->N, k );

			}
		}
	}

	if ( verbose )
		printf ( "totals (cleanrun %d):\n", cleanrun );
	for ( int k=kmin; k<=kmax; k++ ) {
		if ( verbose ) {
			printf ( "%d columns: %ld arrays, %ld pareto\n", k, na[k], npareto[k] );
		}
	}


	if ( numbersfile && cleanrun ) {
		FILE *fid = fopen ( numbersfile, "wt" );
		for ( int k=kmin; k<=kmax; k++ ) {
			if ( verbose ) {
				fprintf ( fid, "k %d: %ld %ld\n", k, na[k], npareto[k] );
			}
		}

		fclose ( fid );
	}


	/* free allocated structures */
	delete adata;

	if ( verbose ) {
		std::cout << "#time end: "<< currenttime() << std::endl;
		std::cout << "#time total: " << printfstring ( "%.1f", get_time_ms()-time0 ) << " [s]" << std::endl;
	}
	return 0;
}
// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; 