// This file was generated by Rcpp::compileAttributes
// Generator token: 10BE3573-1514-4C36-9D1C-5A225CD40393

#include <Rcpp.h>

using namespace Rcpp;

// ddc_write
SEXP ddc_write(SEXP object, SEXP url, const Rcpp::List& options);
RcppExport SEXP hdfsconnector_ddc_write(SEXP objectSEXP, SEXP urlSEXP, SEXP optionsSEXP) {
BEGIN_RCPP
    Rcpp::RObject __result;
    Rcpp::RNGScope __rngScope;
    Rcpp::traits::input_parameter< SEXP >::type object(objectSEXP);
    Rcpp::traits::input_parameter< SEXP >::type url(urlSEXP);
    Rcpp::traits::input_parameter< const Rcpp::List& >::type options(optionsSEXP);
    __result = Rcpp::wrap(ddc_write(object, url, options));
    return __result;
END_RCPP
}
// ddc_read
Rcpp::DataFrame ddc_read(const std::string& url, const Rcpp::List& options);
RcppExport SEXP hdfsconnector_ddc_read(SEXP urlSEXP, SEXP optionsSEXP) {
BEGIN_RCPP
    Rcpp::RObject __result;
    Rcpp::RNGScope __rngScope;
    Rcpp::traits::input_parameter< const std::string& >::type url(urlSEXP);
    Rcpp::traits::input_parameter< const Rcpp::List& >::type options(optionsSEXP);
    __result = Rcpp::wrap(ddc_read(url, options));
    return __result;
END_RCPP
}
// create_plan
Rcpp::List create_plan(const std::string& url, const Rcpp::List& options, const Rcpp::List& workerMapR);
RcppExport SEXP hdfsconnector_create_plan(SEXP urlSEXP, SEXP optionsSEXP, SEXP workerMapRSEXP) {
BEGIN_RCPP
    Rcpp::RObject __result;
    Rcpp::RNGScope __rngScope;
    Rcpp::traits::input_parameter< const std::string& >::type url(urlSEXP);
    Rcpp::traits::input_parameter< const Rcpp::List& >::type options(optionsSEXP);
    Rcpp::traits::input_parameter< const Rcpp::List& >::type workerMapR(workerMapRSEXP);
    __result = Rcpp::wrap(create_plan(url, options, workerMapR));
    return __result;
END_RCPP
}
