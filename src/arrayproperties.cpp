#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <algorithm>
#include <numeric>
#include <algorithm>

#include <Eigen/SVD>
#include <Eigen/Dense>
#include <Eigen/Core>

#ifdef _WIN32
#else
#include <stdbool.h>
#include <unistd.h>
#endif

#include "mathtools.h"
#include "tools.h"
#include "arraytools.h"
#include "arrayproperties.h"

#include "pareto.h"

using namespace Eigen;


template<class Type>
/// helper class: n-dimensional array
class ndarray {

public:
    Type *data;
    std::vector<int> dims;
    int k;
    int n;
    std::vector<int> cumdims;
    std::vector<int> cumprod;

private:
    std::vector<int> tmpidx;
public:
    ndarray(std::vector<int> dimsx, int verbose=0) {
        //verbose=2;
        k = dimsx.size();
        dims = dimsx;
        cumdims.resize(k+1);
        cumprod.resize(k+1);
        tmpidx.resize(k);
        n=1;
        for(int i=0; i<k; i++) n*= dims[i];
        cumdims[0]=0;
        for(int i=0; i<k; i++) cumdims[i+1] = cumdims[i]+dims[i];
        cumprod[0]=1;
        for(int i=0; i<k; i++) cumprod[i+1] = cumprod[i]*dims[i];

        if (verbose) {
            printf("ndarray: dimension %d, total %d\n", k, n);
            printf("  cumprod: ");
            printf_vector(cumprod, "%d ");
            printf("\n");
        }
        data = new Type[n];
        std::fill(data, data+n, 0);
    }

    std::string tmpidxstr(int i) const {
        std::string s = "";
        linear2idx(i);


        for(int i=0; i<k; i++) {
            s+= printfstring("[%d]", tmpidx[i]) ;
        }
        return s;
    }


    void show() const {
        for(int i=0; i<n; i++) {
            std::string idxstr=tmpidxstr(i);
            printf("B[%d] = B%s = %f\n", i, idxstr.c_str(), (double) data[i] );
        }
    }

    /// convert a linear index to normal indices
    inline void linear2idx(int ndx, int *nidx = 0) const {

        for(int i=k-1; i>=0; i--) {
            div_t xx = div ( ndx, cumprod[i] );
            int vi = xx.rem;
            int vj=xx.quot;

            std::vector<int> *p = (std::vector<int> *) &tmpidx;
            //tmpidx[i] = vj;
            p->at(i) = vj;
            ndx = vi;
        }
        if (nidx!=0) {
            for(int i=0; i<k; i++)
                nidx[i]=tmpidx[i];
        }

    }


    inline int getlinearidx(int *idx) const {
        int lidx=0;
        for(int i=0; i<k; i++) {
            //lidx = dims[i]*lidx+idx[i];
            lidx += idx[i]*cumprod[i];
        }
        return lidx;
    }

    /// set value at position
    void set(int *idx, Type val) {
        int lidx=getlinearidx(idx);
        data[lidx]=val;
    }

    /// set value using linear index
    inline void setlinear(int idx, Type val) {
        data[idx] = val;
    }
    /// get value using linear index
    inline void getlinear(int idx, Type val) const {
        return data[idx] ;
    }

    /// get value using n-dimensional index
    Type get(int *idx) const {
        int lidx=getlinearidx(idx);
        return data[lidx];
    }

    ~ndarray() {
        delete [] data;
    }
};


/// Hamming distance
inline int dH(const int N, int k, const array_link &al, int r1, int r2)
{
    int dh=0;
    carray_t *D = al.array;
    carray_t *d1 = D+r1;
    carray_t *d2 = D+r2;

    for(int c=0; c<k; c++) {
        dh += d1[c*N]!=d2[c*N];
    }
    return dh;
}

/// Hamming distance
inline void dHmixed(const int N, int k, const array_link &al, int r1, int r2, int *dh, int ncolgroups, const std::vector<int> colgroupindex)
{
    for(int i=0; i<ncolgroups; i++)  dh[i]=0;

    carray_t *D = al.array;
    carray_t *d1 = D+r1;
    carray_t *d2 = D+r2;

    for(int c=0; c<k; c++) {
        int ci = colgroupindex[c];
        dh[ci] += d1[c*N]!=d2[c*N];
    }
}

/// Hamming distance (transposed array)
inline int dHx(const int nr, int k, carray_t *data, int c1, int c2)
{
    // nr is the OA column variable

    //printf("nr %d, k %d, c1 c2 %d %d\n", nr, k, c1, c2);
    int dh=0;
    carray_t *d1 = data+c1*nr;
    carray_t *d2 = data+c2*nr;

    for(int c=0; c<nr; c++) {
        //printf("  c %d\n", c);
        dh += d1[c]!=d2[c];
    }
//  printf("nr %d, k %d, c1 c2 %d %d: dHx %d\n", nr, k, c1, c2, dh);
    return dh;
}

/// compare 2 GWPL sequences
int GWPcompare(const std::vector<double> &a, const std::vector<double> &b)
{
    for(size_t x=0; x<a.size(); x++) {
        if (a[x]!=b[x])
            return a[x]<b[x];
    }
    return 0;
}

/// calculate distance distrubution (array is transposed for speed)
std::vector<double> distance_distributionT(const array_link &al, int norm=1)
{
    int N = al.n_rows;
    int n = al.n_columns;

    // transpose array
    array_t *x = new array_t[N*n];
    array_t *xx = x;
    for(int i=0; i<N; i++)
        for(int j=0; j<n; j++) {
            (*xx) = al.array[i+j*N];
            xx++;
        }

    // calculate distance distribution
    //printf("distance_distribution\n");
    std::vector<double> dd(n+1);

    for(int r1=0; r1<N; r1++) {
        for(int r2=0; r2<r1; r2++) {
            int dh = dHx(n, N, x, r1, r2);
            dd[dh]+=2; 	// factor 2: dH is symmetric
        }
    }
    // along diagonal
    dd[0] += N;

    if (norm) {
        for(int x=0; x<=n; x++) {
            dd[x] /= N;
        }
    }

    delete [] x;
    return dd;
}

/// calculate distance distribution for mixed array
void distance_distribution_mixed(const array_link &al, ndarray<double> &B, int verbose=1)
{
    int N = al.n_rows;
    int n = al.n_columns;

    // calculate distance distribution

    //printf("distance_distribution\n");
    std::vector<double> dd(n+1);

    arraydata_t ad = arraylink2arraydata(al);

    symmetry_group sg(ad.getS(), false );


    int *dh = new int[sg.ngroups];

    std::vector<int> dims(ad.ncolgroups);
    for(size_t i=0; i<dims.size(); i++) dims[i] = ad.colgroupsize[i];
    if (verbose>=3) {
        printf("distance_distribution_mixed before: \n"); //display_vector(dd); std::cout << std::endl;
        B.show();
    }


    for(int r1=0; r1<N; r1++) {
        for(int r2=0; r2<r1; r2++) {
            dHmixed(N, n, al, r1, r2, dh, sg.ngroups, sg.gidx);

            if (verbose>=4) {
                printf("distance_distribution_mixed: rows %d %d: ", r1, r2);
                print_perm(dh, sg.ngroups);
            }
            int v = B.get(dh);
            B.set(dh, v+2);

            if (verbose>=3) {
                int w=B.getlinearidx(dh);
                if (w==0) {
                    printf(" r1 %d, r2 %d\n", r1, r2);
                }
            }
        }
    }
    if (verbose>=3) {
        printf("distance_distribution_mixed low: \n"); //display_vector(dd); std::cout << std::endl;
        B.show();
    }

    // along diagonal
    for(unsigned int i=0; i<dims.size(); i++) dh[i]=0;
    int v = B.get(dh);
    B.set(dh, v+N);

    if (verbose>=3) {
        printf("distance_distribution_mixed integer: \n"); //display_vector(dd); std::cout << std::endl;
        B.show();
    }

    for(int x=0; x<B.n; x++) {
        // printf("xxxx %d/ %d\n", x, B.n);
        B.data[x] /= N;
    }

    if (verbose) {
        printf("distance_distribution_mixed: \n"); //display_vector(dd); std::cout << std::endl;
        B.show();
    }

    delete [] dh;


    if (0) {
        printf("test: ");
        int tmp[100];
        int li0= 1;
        B.linear2idx(li0, tmp);
        int li = B.getlinearidx(tmp);
        print_perm(tmp, sg.ngroups);
        printf(" linear index: %d -> %d\n", li0, li);
    }

}

template <class Type>
/// calculate MacWilliams transform
std::vector<double> macwilliams_transform(std::vector<Type> B, int N, int s)
{
    int n = B.size()-1;
    // printf("macwilliams_transform: %d\n", n);
    std::vector<double> Bp(n+1);

    if(s==2) {
        for(int j=0; j<=n; j++) {
            Bp[j]=0;
            for(int i=0; i<=n; i++) {
                //printf("  B[i] %.1f krawtchouk(%d, %d, %d, %d) %ld \n", B[i], j,i,n,s, krawtchouk<long>(j, i, n, s));
                Bp[j] +=  B[i] * krawtchouks<long>(j, i, n); // pow(s, -n)
                //printf("--\n");
                //	printf("macwilliams_transform:  B[%d] += %.1f * %ld   (%d %d %d)\n", j , (double) B[i] , krawtchouk<long>(j, i, n, 2), j, i, n);

            }
            Bp[j] /= N;
        }

    } else {
        // NOTE: calculate krawtchouk with dynamic programming
        for(int j=0; j<=n; j++) {
            Bp[j]=0;
            for(int i=0; i<=n; i++) {
                //printf("  B[i] %.1f krawtchouk(%d, %d, %d, %d) %ld \n", B[i], j,i,n,s, krawtchouk<long>(j, i, n, s));
                Bp[j] +=  B[i] * krawtchouk<long>(j, i, n, s); // pow(s, -n)
                //printf("--\n");
            }
            Bp[j] /= N;
        }
    }
    // printf("macwilliams_transform: n %d, s %d: Bp ", n, s); display_vector(Bp); std::cout << std::endl;

    return Bp;
}

//#include <cassert>

#include <algorithm>


std::vector<double> distance_distribution(const array_link &al)
{
    int N = al.n_rows;
    int n = al.n_columns;

    // calculate distance distribution

    //printf("distance_distribution\n");
    std::vector<double> dd(n+1);


    for(int r1=0; r1<N; r1++) {
        for(int r2=0; r2<r1; r2++) {
            int dh = dH(N, n, al, r1, r2);
            dd[dh]+=2;
        }
    }
    // along diagonal
    dd[0] += N;

    for(int x=0; x<=n; x++) {
        dd[x] /= N;
    }
    //printf("distance_distribution: "); display_vector(dd); std::cout << std::endl;

    return dd;
}



std::vector<double> macwilliams_transform_mixed(const ndarray<double> &B, const symmetry_group &sg, std::vector<int> sx, int N, ndarray<double> &Bout, int verbose=0)
{
// TODO: calculation is not very efficient
    if (verbose) {
        printf("macwilliams_transform_mixed:\n");
        printf("sx: ");
        display_vector(sx);
        printf("\n");
    }

    //int v = sg.n;
//sg.gsize;

    int jprod = B.n; // std::accumulate(sg.gsize.begin(), sg.gsize.begin()+sg.ngroups, 1, multiplies<int>());
    //printf("jprod: %d/%d\n", jprod, B.n);

    int jtotal = 0;

    int *bi = new int[sg.ngroups];
    int *iout = new int[sg.ngroups];

//--------------

    for(int j=0; j<Bout.n; j++) {
        //if (verbose)
//	printf("macwilliams_transform_mixed: Bout[%d]= %d\n", j, 0 );

        Bout.linear2idx(j, iout);
        //printf("  j %d: iout[0] %d\n", j, iout[0]);
        Bout.setlinear(j, 0); // [j]=0;

        for(int i=0; i<B.n; i++) {
            B.linear2idx(i, bi);

            long fac = 1 ;
            for(int f=0; f<B.k; f++) {
                long ji=iout[f];
                long ii = bi[f];
                long ni = B.dims[f]-1;
                long si = sx[f];
                //printf("  f %d ji %d, ni %ld, ni %ld, si %d\n", f, (int)ji, ii, ni, si);
                fac *= krawtchouk(ji,ii, ni, si);
                if (verbose>=4)
                    printf("  Bout[%d] += %.1f * %ld (%d %d %d)\n", j , (double)B.data[i] , fac, (int)ji, (int)ii, (int)ni);
            }
            Bout.data[j] += B.data[i] * fac;

        }
        Bout.data[j] /= N;
        if (verbose>=2)
            printf("macwilliams_transform_mixed: Bout[%d]=Bout%s= %f\n", j, Bout.tmpidxstr(j).c_str(), Bout.data[j]);
    }

    if (verbose>=1) {
        printf("Bout: \n");
        Bout.show();
    }

//----------------

    std::vector<double> A(sg.n+1);

    for(int i=0; i<jprod; i++) {
        //printf("   %s\n", B.tmpidxstr(i).c_str() );
        Bout.linear2idx(i, bi);
        int jsum=0;
        for(int j=0; j<Bout.k; j++) jsum += bi[j];
        if (verbose>=2)
            printf("   jsum %d/%d, i %d\n", jsum, (int)A.size(), i);
        A[jsum]+=Bout.data[i];

    }

    delete [] iout;
    delete [] bi;
    return A;
}

/** calculate GWLP (generalized wordlength pattern)
 *
 * Based on: "GENERALIZED MINIMUM ABERRATION FOR ASYMMETRICAL
FRACTIONAL FACTORIAL DESIGNS", Xu and Wu, 2001
 *
 */
std::vector<double> GWLPmixed(const array_link &al, int verbose, int truncate)
{
    arraydata_t adata = arraylink2arraydata(al);
    symmetry_group sg(adata.getS(), false );
    //printf(" adata.ncolgroups: %d/%d\n", adata.ncolgroups, sg.ngroups);

    std::vector<int> dims(adata.ncolgroups);
    for(unsigned int i=0; i<dims.size(); i++) dims[i] = adata.colgroupsize[i]+1;

    //          printf("  dims:        "); display_vector<int>(dims, "%d "); std::cout << endl;

    ndarray<double> B(dims);
    ndarray<double> Bout(dims);

    distance_distribution_mixed(al, B, verbose);
    if (verbose>=3)
    {
        printf("GWLPmixed: distance distrubution\n");
        B.show();
    }

    int N = adata.N;
    //int s = 0;
// calculate GWP
    std::vector<int> ss = adata.getS();

    std::vector<int> sx;
    for(int i=0; i<sg.ngroups; i++)
        sx.push_back(ss[sg.gstart[i]]);

    std::vector<double> gma = macwilliams_transform_mixed( B, sg, sx, N, Bout, verbose);

    if (truncate) {
        for(size_t i=0; i<gma.size(); i++) {
            gma[i]=round(N*N*gma[i])/(N*N);
            if (gma[i]==0) gma[i]=0;	 // fix minus zero
        }
    }
    return gma;


}

/// calculate GWLP (generalized wordlength pattern)
std::vector<double> GWLP(const array_link &al, int verbose, int truncate)
{
    int N = al.n_rows;
    int n = al.n_columns;

    array_t *me = std::max_element(al.array, al.array+(N*n));
    int s = *me+1;
    int k ;
    for(k=0; k<al.n_columns; k++) {
        array_t *mine = std::max_element(al.array+N*k, al.array+(N*(k+1)));
        // printf( "s %d, %d\n", s, *mine+1);
        if (*mine+1 != s) break;
    }
    int domixed= (k<al.n_columns);
    //domixed=1;
    if (verbose)
        printf("GWLP: N %d, s %d, domixed %d\n", N, s, domixed);


    if(domixed) {
        // see: "Algorithmic Construction of Efficient Fractional Factorial Designs With Large Run Sizes", Xu
        std::vector<double> gma = GWLPmixed( al, verbose, truncate);
        return gma;
    } else {
        // calculate distance distribution
        std::vector<double> B = distance_distributionT(al);
        if (verbose) {
            printf("distance_distributionT: ");
            display_vector(B);
            std::cout << std::endl;
        }
        //double sum_of_elems =std::accumulate( B.begin(), B.end(),0.); printf("   sum %.3f\n", sum_of_elems);

// calculate GWP
        std::vector<double> gma = macwilliams_transform( B, N, s);

        if (truncate) {
            for(size_t i=0; i<gma.size(); i++) {
                gma[i]=round(N*N*gma[i])/(N*N);
                if (gma[i]==0) gma[i]=0;	 // fix minus zero

            }
        }

        return gma;
    }
}

/// convert GWLP sequence to unique value
inline double GWPL2val ( GWLPvalue x )
{
    double r=0;
    //for ( size_t i=0; i<x.size(); i++ ) r = r/10 + x[i];
    for ( int i=x.size()-1; i>0; i-- ) r = r/10 + x.v[i];
    if (0) {
        r=0;
        for ( int i=x.size()-1; i>=0; i-- ) {
            //double fac=choose(x.size(),i);
            // r = r/fac + x[i];
        }
    }
    return r;
}

/// convert GWLP sequence to unique value
inline double GWPL2val ( std::vector<double> x )
{
    double r=0;
    //for ( size_t i=0; i<x.size(); i++ ) r = r/10 + x[i];
    for ( int i=x.size()-1; i>0; i-- ) r = r/10 + x[i];
    if (0) {
        r=0;
        for ( int i=x.size()-1; i>=0; i-- ) {
            //double fac=choose(x.size(),i);
            // r = r/fac + x[i];
        }
    }
    return r;
}

std::vector< GWLPvalue > sortGWLP ( const std::vector< GWLPvalue > in)
{
    std::vector< GWLPvalue > v = in;

    std::sort (v.begin(), v.end());
    return v;
}


std::vector< GWLPvalue > projectionGWLPs ( const array_link &al )
{
    int ncols=al.n_columns;

    std::vector< GWLPvalue > v ( ncols );
    for ( int i=0; i<ncols; i++ ) {
        array_link d = al.deleteColumn ( i );
        std::vector<double> gma = GWLP ( d );
        v[i]= gma;
    }
    return v;
}

std::vector<double> projectionGWLPvalues ( const array_link &al )
{
    int ncols=al.n_columns;

    std::vector<double> v ( ncols );
    for ( int i=0; i<ncols; i++ ) {
        array_link d = al.deleteColumn ( i );
        std::vector<double> gma = GWLP ( d );
        v[i]= GWPL2val ( gma );
    }
    return v;
}

/// convert array to Eigen matrix structure
Eigen::MatrixXd arraylink2eigen(const array_link &al)
{
    int k = al.n_columns;
    int n = al.n_rows;

    Eigen::MatrixXd mymatrix = Eigen::MatrixXd::Zero(n,k);

    for (int c=0; c<k; ++c) {
        int ci = c*n;
        for (int r=0; r<n; ++r) {
            mymatrix(r, c) = al.array[r+ci];
        }
    }
    return mymatrix;
}


int arrayrank(const array_link &al)
{
    Eigen::MatrixXd mymatrix = arraylink2eigen(al);
    Eigen::FullPivLU<Eigen::MatrixXd> lu_decomp(mymatrix);
    int rank = lu_decomp.rank();
    return rank;
}


void ABold(const Eigen::MatrixXd &mymatrix, double &A, double &B, int &rank, int verbose)
{
    Eigen::FullPivLU<Eigen::MatrixXd> lu_decomp(mymatrix);

    int m = mymatrix.cols();
    int N= mymatrix.rows();
    const int n = N;

    if (verbose>=4) {
        std::cout << "Matrix: size " << n << ", "<< m << std::endl;
        std::cout << mymatrix;
        std::cout << "\n-----" << std::endl;

    }

    rank= lu_decomp.rank();

    if (rank==m) {
        // full rank!

        Eigen::MatrixXd xxt = mymatrix.transpose()*mymatrix;
        Eigen::MatrixXd d = xxt.diagonal();
        Eigen::MatrixXd xtxi = xxt.inverse();

        if (verbose>=3) {
            std::cout << "Matrix: (x'x) " << std::endl << xxt << std::endl;
            std::cout << "Matrix: (xtxi) " << std::endl << xtxi << std::endl;
        }


        // numerically more stable
        Eigen::MatrixXd As=xxt/n;
        A = pow(As.determinant(), 1./m) ;
        double s = xtxi.diagonal().sum();
        //s=0; for(int i=0; i<k; i++) s+= xtxi(i);
        if (verbose>=2) {
            printf("n %d, m %d, s %f\n", n, m, s);
        }
        B = n * s / m;


    } else {
        // no full rank
        A=0;
        B=0;
    }
}

/// convret array to second order interaction matrix
inline void array2eigenxf(const array_link &al, Eigen::MatrixXd &mymatrix)
{
    int k = al.n_columns;
    int n = al.n_rows;
    int m = 1 + k + k*(k-1)/2;

    mymatrix = Eigen::MatrixXd::Zero(n,m);

    // init first column
    int ww=0;
    for (int r=0; r<n; ++r) {
        mymatrix(r, ww) = 1;
    }

    // init array
    ww=1;
    for (int c=0; c<k; ++c) {
        int ci = c*n;
        for (int r=0; r<n; ++r) {
            mymatrix(r, ww+c) = al.array[r+ci];
        }
    }

    // init interactions
    ww=k+1;
    for (int c=0; c<k; ++c) {
        for (int c2=0; c2<c; ++c2) {
            int ci = c*n;
            int ci2 = c2*n;

            for (int r=0; r<n; ++r) {
                mymatrix(r, ww) = (al.array[r+ci]+al.array[r+ci2])%2;
            }
            ww++;
        }
    }

    mymatrix.array() -= .5;
    mymatrix.array() *= 2;

}


array_link array2xf(const array_link &al)
{
    Eigen::MatrixXd mymatrix;
    array2eigenxf(al, mymatrix);

    array_link out(mymatrix.rows(), mymatrix.cols(),-1);
    for(int i=0; i<out.n_rows*out.n_columns; i++) {
        out.array[i] = mymatrix(i);
    }
    return out;
}

using namespace Eigen;

#include <Eigen/LU>
//#include <Eigen/FullPivLU.h>

// FIXME: restructure naming of Aefficiency (remove old B label) and VIF

void ABwithSVD(const Eigen::MatrixXd &x, double &Deff, double &vif, double &Eeff, int &rank, int verbose)
{
   // printfd("start\n");

    Eigen::FullPivLU<MatrixXd> lu_decomp(x);
    rank = lu_decomp.rank();

    JacobiSVD<Eigen::MatrixXd> svd(x);

//const Eigen::Matrix3f U = svd.matrixU();
//const Eigen::Matrix3f V = svd.matrixV();
    const Eigen::VectorXd S = svd.singularValues();
    int rank2 = svd.nonzeroSingularValues();
    if(rank2!=rank) {
        if (verbose>=3) {
            printf("ABwithSVD: rank calculations differ, unstable matrix: ranklu %d, ranksvd: %d\n", rank, rank2);
            if (verbose>=4) {
                std::cout << "Its singular values are:" << std::endl << S << std::endl;
            }
        }
    }
    int m = x.cols();
    int N= x.rows();

    if (m>N) {
        Deff=0;
        vif=0;
        Eeff=0;
        if (verbose>=3) {
            Eigen::MatrixXd Smat(S);
            Eigen::ArrayXd Sa=Smat.array();
            double Deff = exp(2*Sa.log().sum()/m)/N;

            std::cout << printfstring("  singular matrix: m (%d) > N (%d): rank ", m, N) << rank << std::endl;
	    int mx = std::min(m, N);
            printf("   Deff %e, smallest eigenvalue %e, rank lu %d\n", Deff, S[mx-1], rank);

        }

        return;
    }
    if (verbose>=3)
        printf("N %d, m %d\n", N, m);


    if (S[m-1]<1e-15 || rank < m) {
        if (verbose>=2) {
            printf("   array is singular, setting D-efficiency to zero\n");

            if (0)
            {
                Eigen::MatrixXd mymatrix = x;
                Eigen::MatrixXd mm = mymatrix.transpose() * mymatrix;
                SelfAdjointEigenSolver<Eigen::MatrixXd> es;
                es.compute(mm);
                const Eigen::VectorXd SS =  es.eigenvalues();
                Eigen::VectorXd S = SS; //sqrt(S);
                for(int j=0; j<m; j++) {
                    if (S[j]<1e-14) {
                        if (verbose>=3)
                            printf("  singular!\n");
                        S[j]=0;
                    }
                    else {

                        S[j]=sqrt(S[j]);
                    }
                }

                Eigen::MatrixXd Smat(S);
                Eigen::ArrayXd Sa=Smat.array();
                double DeffDirect = exp(2*Sa.log().sum()/m)/N;

                printf("  DeffDirect %f\n", DeffDirect);

            }


        }
        Deff=0;
        vif=0;
        Eeff=0;

        int rankold=rank2;
        //Eigen::FullPivLU<MatrixXd> lu(x);
        //rank=lu.rank();
        if (verbose>=3) {
            Eigen::MatrixXd Smat(S);
            Eigen::ArrayXd Sa=Smat.array();
            double Deff = exp(2*Sa.log().sum()/m)/N;

            std::cout << "  singular matrix: the rank of A is " << rank << std::endl;
            printf("   Deff %e, smallest eigenvalue %e, rankold %d, rank lu %d\n", Deff, S[m-1], rankold, rank);

        }
        if (verbose>=4)
            std::cout << "Its singular values are:" << std::endl << S << std::endl;
        return;
    }
    if (verbose>=3)
        std::cout << "Its singular values are:" << std::endl << S << std::endl;
    //cout << "x:\n"<< x << std::endl;

    Eeff = S[m-1]*S[m-1]/N;
    //cout << "Its singular values are:" << endl << S/sqrt(N) << endl;

    vif=0;
    for(int i=0; i<m; i++)
        vif+= 1/(S[i]*S[i]);
    vif = N*vif/m;

//Eigen::VectorXf Si= S.inverse();
//cout << "Its inverse singular values are:" << endl << Si << endl;

    Eigen::MatrixXd Smat(S);
    Eigen::ArrayXd Sa=Smat.array();
    Deff = exp(2*Sa.log().sum()/m)/N;

    /*
      A=1;
    for(int i=0; i<m; i++)
    	A *= S[i]*S[i];

    A= pow(A, 1./m);
    A/= N;
    */


    if (verbose>=2) {
        printf("ABwithSVD: Defficiency %.3f, Aefficiency %.3f (%.3f), Eefficiency %.3f\n", Deff, vif, vif*m, Eeff);

        Eigen::FullPivLU<MatrixXd> lu(x);
        int ranklu=lu.rank();

        printf("   Defficiency %e, smallest eigenvalue %e, rank %d, rank lu %d\n", Deff, S[m-1], rank, ranklu);

    }
}


/** Calculate the rank of an orthogonal array (rank of [I X X_2] )
 *
 * The vector ret is filled with the rank, Defficiency, VIF efficiency and Eefficiency
 */
int array_rank_D_B(const array_link &al, std::vector<double> *ret  , int verbose )
{
  //printfd("start\n");
    int k = al.n_columns;
    int n = al.n_rows;
    int m = 1 + k + k*(k-1)/2;

    Eigen::MatrixXd mymatrix(n, m);
    array2eigenxf(al, mymatrix);

    double Deff;
    double B;
    double Eeff;
    int rank;

    //ABold(mymatrix, A, B, rank, verbose);
    ABwithSVD(mymatrix, Deff, B, Eeff, rank, verbose);

    if (ret!=0) {
        ret->push_back( rank );
        ret->push_back( Deff );
        ret->push_back( B );
        ret->push_back( Eeff );
    }
    return rank;

//	   cout << "Here is the matrix A:\n" << A << endl;
    // FullPivLU<Matrix3f> lu_decomp(A);
    //cout << "The rank of A is " << lu_decomp.rank() << endl;
}

double VIFefficiency(const array_link &al, int verbose) {
    std::vector<double> ret;
    array_rank_D_B(al, &ret, verbose );
    return ret[2];
}

double Aefficiency(const array_link &al, int verbose) {
    std::vector<double> ret;
    array_rank_D_B(al, &ret, verbose );
    if (ret[2]==0)
        return 0;
    else
        return 1./ret[2];
}

double Eefficiency(const array_link &al, int verbose) {
    std::vector<double> ret;
    int r = array_rank_D_B(al, &ret, verbose );
    return ret[3];
}

std::vector<int> Jcharacteristics(const array_link &al, int jj, int verbose)
{
    jstruct_t js(al, jj);
    return js.vals;
}



double Defficiency(const array_link &al, int verbose) {

    int k = al.n_columns;
    int n = al.n_rows;
    int m = 1 + k + k*(k-1)/2;
    int N = n;
    double Deff = -1;

    Eigen::MatrixXd mymatrix(n, m);
    array2eigenxf(al, mymatrix);

        Eigen::FullPivLU<MatrixXd> lu_decomp(mymatrix);
    int rank = lu_decomp.rank();


    Eigen::MatrixXd mm = mymatrix.transpose() * mymatrix;
    SelfAdjointEigenSolver<Eigen::MatrixXd> es;
    es.compute(mm);
    const Eigen::VectorXd SS =  es.eigenvalues();
    Eigen::VectorXd S = SS; //sqrt(S);

      if (S[m-1]<1e-15 || rank < m) {
        if (verbose>=2) {
            printf("   array is singular, setting D-efficiency to zero\n");
	    
	}
      Deff=0;
      return Deff;	
      }
      
    for(int j=0; j<m; j++) {
        if (S[j]<1e-14) {
            if (verbose>=3)
                printf("  singular!\n");
            S[j]=0;
        }
        else {

            S[j]=sqrt(S[j]);
        }
    }

    if (verbose>=2) {
        JacobiSVD<Eigen::MatrixXd> svd(mymatrix);
        const Eigen::VectorXd S2 = svd.singularValues();

        if( ! (S2[m-1]<1e-15) ) {
            for(int ii=0; ii<m-3; ii++) {
                printf("ii %d: %f %f %f\n", ii, S[m-ii-1], SS[m-ii-1], S2[ii]);
            }
            for(int ii=m-3; ii<m-1; ii++) {
                printf("ii %d: %f %f %f\n", ii, S[m-ii-1], SS[m-ii-1], S2[ii]);
            }

            Eigen::MatrixXd Smat(S);
            Eigen::ArrayXd Sa=Smat.array();
            Deff = exp(2*Sa.log().sum()/m)/N;
            printf("  Aold: %.6f\n", Deff);
        }
    }
//	cout << "----" << S << std::endl;
//	cout << SS << std::endl;

    //   int rank = svd.nonzeroSingularValues();

    //printf("Avalue: S size %d %d\n", (int) S.cols(), (int) S.rows());

    if (S[0]<1e-15) {
        if (verbose>=2)
            printf("Avalue: singular matrix\n");
        Deff=0;
        return Deff;
    }

    Eigen::MatrixXd Smat(S);
    Eigen::ArrayXd Sa=Smat.array();
    Deff = exp(2*Sa.log().sum()/m)/N;


    if (verbose>=2) {
        printf("Avalue: A %.6f (S[0] %e)\n", Deff, S[0]);
    }

    Deff = std::min(Deff,1.);	// for numerical stability
    return Deff;
}

double CL2discrepancy(const array_link &al)
{
    const int m = al.n_columns;

    std::vector<double> gwp = al.GWLP();

    double v = 1;
    for (int k=1; k<=m; k++) {
        v += gwp[k]/std::pow(double(9), k);
    }
    double w = std::pow(double(13)/12, m) - 2*pow(35./32, m) + std::pow( 9./8, m) * v;

    return w;
}

Pareto<mvalue_t<long>,long> parsePareto ( const arraylist_t &arraylist, int verbose )
{
    Pareto<mvalue_t<long>,long> pset;
    pset.verbose=verbose;

    for ( size_t i=0; i<arraylist.size(); i++ ) {
        if ( verbose>=2 || ( ( i%2000==0 ) && verbose>=1 ) ) {
            printf ( "parsePareto: array %zu/%zu\n", i, arraylist.size() );
        }
        if ( ( ( i%10000==0 ) && verbose>=1 ) ) {
            pset.show ( 1 );
        }
        const array_link &al = arraylist.at ( i );
        parseArrayPareto ( al, ( long ) i, pset, verbose );

    }
    return pset;
}

