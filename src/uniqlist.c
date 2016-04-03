#include "data.table.h"

// DONE: return 'uniqlist' as a vector (same as duplist) and write a separate function to get group sizes
// Also improvements for numeric type with a hack of checking unsigned int (to overcome NA/NaN/Inf/-Inf comparisons) (> 2x speed-up)
SEXP uniqlist(SEXP l, SEXP order)
{
    // This works like UNIX uniq as referred to by ?base::unique; i.e., it 
    // drops immediately repeated rows but doesn't drop duplicates of any 
    // previous row. Unless, order is provided, then it also drops any previous 
    // row. l must be a list of same length vectors ans is allocated first 
    // (maximum length the number of rows) and the length returned in anslen.
    // DONE: ans is now grown
    Rboolean b, byorder;
    unsigned long long *ulv; // for numeric check speed-up
    SEXP v, ans, class;
    R_len_t i, j, nrow, ncol, len, thisi, previ, isize=1000;

    int *iidx = Calloc(isize, int); // for 'idx'
    int *n_iidx; // to catch allocation errors using Realloc!
    if (NA_INTEGER != NA_LOGICAL || sizeof(NA_INTEGER)!=sizeof(NA_LOGICAL)) 
        error("Have assumed NA_INTEGER == NA_LOGICAL (currently R_NaInt). If R changes this in future (seems unlikely), an extra case is required; a simple change.");
    ncol = length(l);
    nrow = length(VECTOR_ELT(l,0));
    len = 1;
    iidx[0] = 1; // first row is always the first of the first group
    byorder = INTEGER(order)[0] != -1;
    // Using MISSING() does not seem stable under windows. Always having arguments passed in seems a good idea anyway.
    thisi = byorder ? INTEGER(order)[0]-1 : 0;
    for (i=1; i<nrow; i++) {
        previ = thisi;
        thisi = byorder ? INTEGER(order)[i]-1 : i;
        j = ncol;  // the last column varies the most frequently so check that first and work backwards
        b = TRUE;
        while (--j>=0 && b) {
            v=VECTOR_ELT(l,j);
            switch (TYPEOF(v)) {
            case INTSXP : case LGLSXP :
                b=INTEGER(v)[thisi]==INTEGER(v)[previ]; break;
            case STRSXP :
                // fix for #469, when key is set, duplicated calls uniqlist, where encoding 
                // needs to be taken care of.
                b=ENC2UTF8(STRING_ELT(v,thisi))==ENC2UTF8(STRING_ELT(v,previ)); break;  // marked non-utf8 encodings are converted to utf8 so as to match properly when inputs are of different encodings.
            case REALSXP :
                ulv = (unsigned long long *)REAL(v);  
                b = ulv[thisi] == ulv[previ]; // (gives >=2x speedup)
                if (!b) {
                    class = getAttrib(v, R_ClassSymbol);
                    twiddle = (isString(class) && STRING_ELT(class, 0)==char_integer64) ? &i64twiddle : &dtwiddle;
                    b = twiddle(ulv, thisi, 1) == twiddle(ulv, previ, 1);
                }
                break;
                // TO DO: store previ twiddle call, but it'll need to be vector since this is in a loop through columns. Hopefully the first == will short circuit most often
            default :
                error("Type '%s' not supported", type2char(TYPEOF(v))); 
            }
        }
        if (!b) iidx[len++] = i+1;
        if (len >= isize) {
            isize = 1.1*isize*nrow/i;
            n_iidx = Realloc(iidx, isize, int);
            if (n_iidx != NULL) iidx = n_iidx; else error("Error in reallocating memory in 'uniqlist'\n");
        }
    }
    PROTECT(ans = allocVector(INTSXP, len));
    memcpy(INTEGER(ans), iidx, sizeof(int)*len); // sizeof is of type size_t - no integer overflow issues   
    Free(iidx);
    UNPROTECT(1);
    return(ans);
}

SEXP uniqlengths(SEXP x, SEXP n) {
    SEXP ans;
    R_len_t i, len;
    if (TYPEOF(x) != INTSXP || length(x) < 0) error("Input argument 'x' to 'uniqlengths' must be an integer vector of length >= 0");
    if (TYPEOF(n) != INTSXP || length(n) != 1) error("Input argument 'n' to 'uniqlengths' must be an integer vector of length 1");
    PROTECT(ans = allocVector(INTSXP, length(x)));
    len = length(x);
    for (i=1; i<len; i++) {
        INTEGER(ans)[i-1] = INTEGER(x)[i] - INTEGER(x)[i-1];
    }
    INTEGER(ans)[len-1] = INTEGER(n)[0] - INTEGER(x)[len-1] + 1;
    UNPROTECT(1);
    return(ans);
}

// we could compute `uniqlist` and `uniqlengths` and then construct the result
// but that seems unnecessary waste of memory and roundabout..
// so, we'll do it directly here.. 'order()' is implemented in C-api, but not used 
// in R side yet, but if need be, it can be added easily.
SEXP rleid(SEXP l, SEXP order) {
    Rboolean b, byorder;
    unsigned long long *ulv; // for numeric check speed-up
    SEXP v, ans, class;
    R_len_t nrow = length(VECTOR_ELT(l,0)), ncol = length(l);
    R_len_t i, j, len = 1, thisi, previ;

    if (!nrow || !ncol) return (allocVector(INTSXP, 0));
    for (i=1; i<ncol; i++) {
        if (length(VECTOR_ELT(l,i)) != nrow)
            error("All elements to input list must be of same length. Element [%d] has length %d != length of first element = %d.", i+1, length(VECTOR_ELT(l,i)), nrow);
    }
    if (NA_INTEGER != NA_LOGICAL || sizeof(NA_INTEGER)!=sizeof(NA_LOGICAL)) 
        error("Have assumed NA_INTEGER == NA_LOGICAL (currently R_NaInt). If R changes this in future (seems unlikely), an extra case is required; a simple change.");
    ans = PROTECT(allocVector(INTSXP, nrow));
    INTEGER(ans)[0] = 1; // first row is always the first of first group
    byorder = INTEGER(order)[0] != -1;
    thisi = byorder ? INTEGER(order)[0]-1 : 0;
    for (i=1; i<nrow; i++) {
        previ = thisi;
        thisi = byorder ? INTEGER(order)[i]-1 : i;
        j = ncol;  // the last column varies the most frequently so check that first and work backwards
        b = TRUE;
        while (--j>=0 && b) {
            v=VECTOR_ELT(l,j);
            switch (TYPEOF(v)) {
            case INTSXP : case LGLSXP :
                b=INTEGER(v)[thisi]==INTEGER(v)[previ]; break;
            case STRSXP :
                b=STRING_ELT(v,thisi)==STRING_ELT(v,previ); break;  // forder checks no non-ascii unknown, and either UTF-8 or Latin1 but not both. So == pointers is ok given that check.
            case REALSXP :
                ulv = (unsigned long long *)REAL(v);  
                b = ulv[thisi] == ulv[previ]; // (gives >=2x speedup)
                if (!b) {
                    class = getAttrib(v, R_ClassSymbol);
                    twiddle = (isString(class) && STRING_ELT(class, 0)==char_integer64) ? &i64twiddle : &dtwiddle;
                    b = twiddle(ulv, thisi, 1) == twiddle(ulv, previ, 1);
                }
                break;
                // TO DO: store previ twiddle call, but it'll need to be vector since this is in a loop through columns. Hopefully the first == will short circuit most often
            default :
                error("Type '%s' not supported", type2char(TYPEOF(v))); 
            }
        }
        INTEGER(ans)[i] = b ? len : ++len;
    }
    UNPROTECT(1);
    return(ans);
}

SEXP nestedid(SEXP l, SEXP cols, SEXP order) {
    Rboolean b, byorder;
    SEXP v, ans, class;
    R_len_t nrow = length(VECTOR_ELT(l,0)), ncol = length(cols);
    R_len_t i, j, k, thisi, previ, grpsize=1000, ngrp=0;
    R_len_t *tmp, *grp = Calloc(grpsize, R_len_t), *i64 = Calloc(ncol, R_len_t);
    if (!isInteger(cols) || ncol == 0) error("cols must be an integer vector of positive length");
    for (j=0; j<ncol; j++) {
        class = getAttrib(VECTOR_ELT(l, INTEGER(cols)[j]-1), R_ClassSymbol);
        i64[j] = isString(class) && STRING_ELT(class, 0) == char_integer64;
    }
    ans  = PROTECT(allocVector(INTSXP, nrow));
    int *ians = INTEGER(ans);
    byorder = length(order);
    thisi = byorder ? INTEGER(order)[0]-1 : 0;
    grp[0] = thisi; ngrp = 1; ians[thisi] = 1;
    for (i=1; i<nrow; i++) {
        thisi = byorder ? INTEGER(order)[i]-1 : i;
        for (k=0; k<ngrp; k++) {
            j = ncol;
            b = TRUE;
            previ = grp[k];
            while(--j>0 && b) {
                v=VECTOR_ELT(l,INTEGER(cols)[j]-1);
                switch(TYPEOF(v)) {
                    case INTSXP: case LGLSXP:
                    b = INTEGER(v)[thisi]>=INTEGER(v)[previ];
                    break;
                    case STRSXP :
                    b=ENC2UTF8(STRING_ELT(v,thisi))==ENC2UTF8(STRING_ELT(v,previ)); break;
                    case REALSXP:
                    twiddle = i64[j] ? &i64twiddle : &dtwiddle;
                    b = twiddle(DATAPTR(v), thisi, 1) >= twiddle(DATAPTR(v), previ, 1);
                    break;
                    default: error("Type '%s' not supported", type2char(TYPEOF(v)));
                }
            }
            if (b) break;
        }
        if (b) {
            grp[k] = thisi;
            ians[thisi] = k+1;
        } else {
            grp[ngrp] = thisi;
            ians[thisi] = ++ngrp;
        }
        if (ngrp >= grpsize) {
            grpsize = 1.1*grpsize*nrow/i;
            tmp = Realloc(grp, grpsize, int);
            if (tmp != NULL) grp = tmp; else error("Error in reallocating memory in 'uniqlist'\n");
        }
    }
    Free(grp);
    UNPROTECT(1);
    return(ans);
}
