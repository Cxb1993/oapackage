/** \file oapareto.cpp

 C++ program: oapareto

 oapareto: Calculate a set of Pareto optimal arrays from a list of arrays

 Author: Pieter Eendebak <pieter.eendebak@gmail.com>
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
//#include "oadevelop.h"

#include "pareto.h"



/**
 * @brief Analyse arrays
 * @param argc
 * @param argv[]
 * @return
 */
int main(int argc, char* argv[])
{
    AnyOption opt;

    /* parse command line options */
    opt.setFlag(  "help", 'h' );   /* a flag (takes no argument), supporting long and short form */
    opt.setOption("output", 'o');
    opt.setOption("verbose", 'v');
//    opt.setOption("jj", 'j');
    opt.setFlag(  "test", 't' );

    opt.addUsage( "Orthonal Array Analyse: analyse arrays" );
    opt.addUsage( "Usage: oapareto [OPTIONS] [FILE]" );
    opt.addUsage( "" );
    opt.addUsage( " -h --help  			Prints this help " );
    opt.addUsage( " -v --verbose  			Verbose level (default: 1) " );
    opt.addUsage( " -o [FILE] --output [FILE]	Output file for filtered arrays (default: no output) " );
    opt.processCommandArgs(argc, argv);

    print_copyright();

    /* parse options */
    if ( opt.getFlag( "help" ) || opt.getFlag( 'h' ) || opt.getArgc()==0 ) {
        opt.printUsage();
        exit(0);
    }

    const char *outputprefix = 0;
    if (opt.getValue("output")!=0)
        outputprefix = opt.getValue('o');

    int verbose = opt.getIntValue('v', 1);

    /* read in the arrays */
    arraylist_t *arraylist = new arraylist_t;
    if (verbose)
        std::cout << "oaanalyse: reading " << opt.getArgc() << " file(s)" << std::endl;

    int nr;
    int nc;
    int nn;
    for ( int i = 0 ; i < opt.getArgc() ; i++ ) {
        if (verbose)
            std::cout << "file " <<  opt.getArgv( i ) << std::endl ;
        int n = readarrayfile( opt.getArgv( i ), arraylist);

        arrayfileinfo( opt.getArgv( i ), nn, nr, nc);
        if (verbose)
            std::cout << "   read " << n << printfstring(" array(s) (size %d %d)", nr, nc) << std::endl;
    }


    // Note: generic function is not streaming....
    // Pareto<mvalue_t<long>,long> pset = parsePareto(arraylist, 1);

    Pareto<mvalue_t<long>,long> pset;
    pset.verbose=verbose;

    for(size_t i=0; i<arraylist->size(); i++) {
        if (verbose>=2 || ( (i%1000==0) && verbose>=1) ) {
            printf("oapareto: array %zu/%zu\n", i, arraylist->size() );
        }
        if ( ( (i%2000==0) && verbose>=1) ) {
            pset.show(1);
        }
        const array_link &al = arraylist->at(i);
        parseArrayPareto(al, (long)i, pset, verbose);

    }

    if (verbose)
        printf(" %zu arrays -> %d pareto values, %d pareto arrays \n", arraylist->size(), pset.number(),pset.numberindices() );
    pset.show(verbose);

    std::vector<long> idx = pset.allindices();

    arraylist_t pp = selectArrays(*arraylist, idx);
    //  printf(" %zu arrays -> %zu pareto \n", arraylist->size(), pp.size() );

    // write to disk
    if(outputprefix!=0) {
        if (verbose)
            printf("oapareto: writing arrays to file %s\n", outputprefix);
        writearrayfile(outputprefix, &pp, ABINARY);
    }

    /* free allocated structures */
    delete arraylist;

    return 0;
}