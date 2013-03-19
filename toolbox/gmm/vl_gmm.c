/** @file   vl_gmm.c
 ** @brief  vl_gmm MEX definition.
 ** @author David Novotny
 **/

/*
Copyright (C) 2007-12 Andrea Vedaldi and Brian Fulkerson.
All rights reserved.

This file is part of the VLFeat library and is made available under
the terms of the BSD license (see the COPYING file).
*/

#include <vl/gmm.h>
#include <mexutils.h>
#include <string.h>
#include <stdio.h>

enum
{
  opt_max_num_iterations,
  opt_distance,
  opt_initialization,
  opt_num_repetitions,
  opt_verbose,
  opt_multithreading,
  opt_means,
  opt_sigmas,
  opt_weights,
  opt_sigma_low_bound
} ;

enum
{
  INIT_RAND,
  INIT_CUSTOM
} ;

vlmxOption  options [] =
{
  {"MaxNumIterations",  1,   opt_max_num_iterations  },
  {"Verbose",           0,   opt_verbose             },
  {"NumRepetitions",    1,   opt_num_repetitions,    },
  {"Initialization",    1,   opt_initialization      },
  {"Initialisation",    1,   opt_initialization      }, /* UK spelling */
  {"Multithreading",    1,   opt_multithreading      },
  {"InitMeans",         1,   opt_means               },
  {"InitSigmas",        1,   opt_sigmas              },
  {"InitWeights",       1,   opt_weights             },
  {"SigmaBound",        1,   opt_sigma_low_bound     },
  {0,                   0,   0                       }
} ;

/* driver */
void
mexFunction (int nout, mxArray * out[], int nin, const mxArray * in[])
{
  enum {IN_DATA = 0, IN_NUMCLUSTERS, IN_END} ;
  enum {OUT_MEANS, OUT_SIGMAS, OUT_WEIGHTS, OUT_LL, OUT_POSTERIORS} ;

  int opt ;
  int next = IN_END ;
  mxArray const  *optarg ;

  vl_size i;

  vl_size numClusters = 10;
  vl_size dimension ;
  vl_size numData ;

  void * initSigmas = 0;
  void * initMeans = 0;
  void * initWeights = 0;
  vl_bool meansSet = VL_FALSE;
  vl_bool sigmasSet = VL_FALSE;
  vl_bool weightsSet = VL_FALSE;

  double sigmaLowBound = 0.000001;

  VlGMMMultithreading multithreading = VlGMMParallel;

  void const * data = NULL ;

  vl_size maxNumIterations = 100 ;
  vl_size numRepetitions = 1 ;
  double LL ;
  int verbosity = 0 ;
  VlGMMInitialization initialization = VlGMMRand ;

  vl_type dataType ;
  mxClassID classID ;

  VlGMM * gmm ;

  VL_USE_MATLAB_ENV ;

  /* -----------------------------------------------------------------
   *                                               Check the arguments
   * -------------------------------------------------------------- */

  if (nin < 2)
  {
    vlmxError (vlmxErrInvalidArgument,
               "At least two arguments required.");
  }
  else if (nout > 5)
  {
    vlmxError (vlmxErrInvalidArgument,
               "Too many output arguments.");
  }

  classID = mxGetClassID (IN(DATA)) ;
  switch (classID)
  {
  case mxSINGLE_CLASS:
    dataType = VL_TYPE_FLOAT ;
    break ;
  case mxDOUBLE_CLASS:
    dataType = VL_TYPE_DOUBLE ;
    break ;
  default:
    vlmxError (vlmxErrInvalidArgument,
               "DATA must be of class SINGLE or DOUBLE") ;
    abort() ;
  }

  dimension = mxGetM (IN(DATA)) ;
  numData = mxGetN (IN(DATA)) ;

  if (dimension == 0)
  {
    vlmxError (vlmxErrInvalidArgument, "SIZE(DATA,1) is zero") ;
  }

  if (!vlmxIsPlainScalar(IN(NUMCLUSTERS)) ||
      (numClusters = (vl_size) mxGetScalar(IN(NUMCLUSTERS))) < 1  ||
      numClusters > numData)
  {
    vlmxError (vlmxErrInvalidArgument,
               "NUMCLUSTERS must be a positive integer not greater "
               "than the number of data.") ;
  }

  while ((opt = vlmxNextOption (in, nin, options, &next, &optarg)) >= 0)
  {
    char buf [1024] ;

    switch (opt)
    {

    case opt_verbose :
      ++ verbosity ;
      break ;

    case opt_max_num_iterations :
      if (!vlmxIsPlainScalar(optarg) || mxGetScalar(optarg) < 0)
      {
        vlmxError (vlmxErrInvalidArgument,
                   "MAXNUMITERATIONS must be a non-negative integer scalar") ;
      }
      maxNumIterations = (vl_size) mxGetScalar(optarg) ;
      break ;

    case opt_sigma_low_bound :
      sigmaLowBound = (double) mxGetScalar(optarg) ;
      break ;

    case opt_weights : ;
	  {
      mxClassID classIDweights = mxGetClassID (optarg) ;
      switch (classIDweights)
      {
      case mxSINGLE_CLASS:
        if (dataType == VL_TYPE_DOUBLE)
        {
          vlmxError (vlmxErrInvalidArgument, "INITWEIGHTS must be of same data type as X") ;
        }
        break ;
      case mxDOUBLE_CLASS:
        if (dataType == VL_TYPE_FLOAT)
        {
          vlmxError (vlmxErrInvalidArgument, "INITWEIGHTS must be of same data type as X") ;
        }
        break ;
      default :
        abort() ;
        break ;
      }

      if (! vlmxIsMatrix (optarg, -1, -1) || ! vlmxIsReal (optarg))
      {
        vlmxError(vlmxErrInvalidArgument,
                  "INITWEIGHTS must be a real matrix ") ;
      }

      if (mxGetNumberOfElements(optarg) != numClusters)
      {
        vlmxError(vlmxErrInvalidArgument,
                  "INITWEIGHTS has to have numClusters elements") ;
      }

      weightsSet = VL_TRUE;
      initWeights = mxGetPr(optarg) ;

      break;
	  }

    case opt_means : ;
	  {
      mxClassID classIDmeans = mxGetClassID (optarg) ;
      switch (classIDmeans)
      {
      case mxSINGLE_CLASS:
        if (dataType == VL_TYPE_DOUBLE)
        {
          vlmxError (vlmxErrInvalidArgument, "INITMEANS must be of same data type as X") ;
        }
        break ;
      case mxDOUBLE_CLASS:
        if (dataType == VL_TYPE_FLOAT)
        {
          vlmxError (vlmxErrInvalidArgument, "INITMEANS must be of same data type as X") ;
        }
        break ;
      default :
        abort() ;
        break ;
      }

      if (! vlmxIsMatrix (optarg, -1, -1) || ! vlmxIsReal (optarg))
      {
        vlmxError(vlmxErrInvalidArgument,
                  "INITMEANS must be a real matrix ") ;
      }

      if (mxGetM(optarg) != dimension)
      {
        vlmxError(vlmxErrInvalidArgument,
                  "INITMEANS has to have the same dimension (nb of rows) as input X") ;
      }

      if (mxGetN(optarg) != numClusters)
      {
        vlmxError(vlmxErrInvalidArgument,
                  "INITMEANS has to have NUMCLUSTERS number of points (columns)") ;
      }

      meansSet = VL_TRUE;
      initMeans = mxGetPr(optarg) ;

      break;
	  }
    case opt_sigmas : ;
	  {
      mxClassID classIDsigma = mxGetClassID (optarg) ;
      switch (classIDsigma)
      {
      case mxSINGLE_CLASS:
        if (dataType == VL_TYPE_DOUBLE)
        {
          vlmxError (vlmxErrInvalidArgument, "INITSIGMAS must be of same data type as X") ;
        }
        break ;
      case mxDOUBLE_CLASS:
        if (dataType == VL_TYPE_FLOAT)
        {
          vlmxError (vlmxErrInvalidArgument, "INITSIGMAS must be of same data type as X") ;
        }
        break ;
      default :
        abort() ;
        break ;
      }

      if (! vlmxIsMatrix (optarg, -1, -1) || ! vlmxIsReal (optarg))
      {
        vlmxError(vlmxErrInvalidArgument,
                  "INITSIGMAS must be a real matrix ") ;
      }

      if (mxGetM(optarg) != dimension)
      {
        vlmxError(vlmxErrInvalidArgument,
                  "INITSIGMAS has to have the same dimension (nb of rows) as input DATA") ;
      }

      if (mxGetN(optarg) != numClusters)
      {
        vlmxError(vlmxErrInvalidArgument,
                  "INITSIGMAS has to have NUMCLUSTERS number of points (columns)") ;
      }

      sigmasSet = VL_TRUE;
      initSigmas = mxGetPr(optarg) ;

      break;
	  }
    case opt_initialization :
      if (!vlmxIsString (optarg, -1))
      {
        vlmxError (vlmxErrInvalidArgument,
                   "INITLAIZATION must be a string.") ;
      }
      if (mxGetString (optarg, buf, sizeof(buf)))
      {
        vlmxError (vlmxErrInvalidArgument,
                   "INITIALIZATION argument too long.") ;
      }
      if (vlmxCompareStringsI("rand", buf) == 0)
      {
        initialization = VlGMMRand ;
      }
      else if (vlmxCompareStringsI("custom", buf) == 0)
      {
        initialization = VlGMMCustom ;
      }
      else
      {
        vlmxError (vlmxErrInvalidArgument,
                   "Invalid value %s for INITIALISATION.", buf) ;
      }
      break ;

    case opt_multithreading :
      if (!vlmxIsString (optarg, -1))
      {
        vlmxError (vlmxErrInvalidArgument,
                   "MULTITHREADING must be a string.") ;
      }
      if (mxGetString (optarg, buf, sizeof(buf)))
      {
        vlmxError (vlmxErrInvalidArgument,
                   "MULTITHREADING argument too long.") ;
      }

      if (vlmxCompareStringsI("serial", buf) == 0)
      {
        multithreading = VlGMMSerial ;
      }
      else if (vlmxCompareStringsI("parallel", buf) == 0)
      {
        multithreading = VlGMMParallel ;
      }
      else
      {
        vlmxError (vlmxErrInvalidArgument,
                   "Invalid value %s for MULTITHREADING.", buf) ;
      }
      break ;

    case opt_num_repetitions :
      if (!vlmxIsPlainScalar (optarg))
      {
        vlmxError (vlmxErrInvalidArgument,
                   "NUMREPETITIONS must be a scalar.") ;
      }
      if (mxGetScalar (optarg) < 1)
      {
        vlmxError (vlmxErrInvalidArgument,
                   "NUMREPETITIONS must be larger than or equal to 1.") ;
      }
      numRepetitions = (vl_size) mxGetScalar (optarg) ;
      break ;
    default :
      abort() ;
      break ;
    }
  }

  /* -----------------------------------------------------------------
   *                                                        Do the job
   * -------------------------------------------------------------- */

  data = mxGetPr(IN(DATA)) ;

  switch(dataType){
  case VL_TYPE_DOUBLE:
        for(i = 0; i < numData*dimension; i++) {
            double datum = *((double*)data + i);
            if(!(datum < VL_INFINITY_D && datum > -VL_INFINITY_D)){
                vlmxError (vlmxErrInvalidArgument,
                   "DATA contains NaNs or Infs.") ;
            }
        }
      break;
  case VL_TYPE_FLOAT:
        for(i = 0; i < numData*dimension; i++) {
            float datum = *((float*)data + i);
            if(!(datum < VL_INFINITY_F && datum > -VL_INFINITY_F)){
                vlmxError (vlmxErrInvalidArgument,
                   "DATA contains NaNs or Infs.") ;
            }
        }
    break;
  default:
    abort();
    break;
  }

  gmm = vl_gmm_new (dataType) ;
  vl_gmm_set_verbosity (gmm, verbosity) ;
  vl_gmm_set_num_repetitions (gmm, numRepetitions) ;
  vl_gmm_set_max_num_iterations (gmm, maxNumIterations) ;
  vl_gmm_set_multithreading (gmm,multithreading);
  vl_gmm_set_initialization (gmm, initialization) ;
  vl_gmm_set_sigma_lower_bound (gmm, sigmaLowBound) ;

  if(sigmasSet || meansSet || weightsSet)
  {
    if(vl_gmm_get_initialization(gmm) != VlGMMCustom)
    {
      vlmxWarning (vlmxErrInconsistentData, "Initial sigmas, means or weights have been set -> switching to custom initialization.");
    }
    vl_gmm_set_initialization(gmm,VlGMMCustom);
  }

  if(vl_gmm_get_initialization(gmm) == VlGMMCustom)
  {
    if (!sigmasSet || !meansSet || !weightsSet)
    {
      vlmxError (vlmxErrInvalidArgument, "When custom initialization is set, InitMeans, InitSigmas and initWeights options have to be specified.") ;
    }
    vl_gmm_set_means (gmm,initMeans,numClusters,dimension);
    vl_gmm_set_sigmas (gmm,initSigmas,numClusters,dimension);
    vl_gmm_set_weights (gmm,initWeights,numClusters);
  }

  if (verbosity)
  {
    char const * initializationName = 0 ;
    char const * multithreadingName = 0 ;

    switch (vl_gmm_get_initialization(gmm))
    {
    case VlGMMRand :
      initializationName = "rand" ;
      break ;
    case VlGMMCustom :
      initializationName = "custom" ;
      break ;
    default:
      abort() ;
    }
    switch (vl_gmm_get_multithreading(gmm))
    {
    case VlGMMSerial :
      multithreadingName = "serial" ;
      break ;
    case VlGMMParallel :
      multithreadingName = "parallel" ;
      break ;
    default:
      abort() ;
    }

    mexPrintf("gmm: initialization = %s\n", initializationName) ;
    mexPrintf("gmm: multithreading = %s\n", multithreadingName) ;
    mexPrintf("gmm: maxNumIterations = %d\n", vl_gmm_get_max_num_iterations(gmm)) ;
    mexPrintf("gmm: numRepetitions = %d\n", vl_gmm_get_num_repetitions(gmm)) ;
    mexPrintf("gmm: dataType = %s\n", vl_get_type_name(vl_gmm_get_data_type(gmm))) ;
    mexPrintf("gmm: dataDimension = %d\n", dimension) ;
    mexPrintf("gmm: num. data points = %d\n", numData) ;
    mexPrintf("gmm: num. centers = %d\n", numClusters) ;
    mexPrintf("gmm: lower bound on sigma = %f\n", vl_gmm_get_sigma_lower_bound(gmm)) ;
    mexPrintf("\n") ;
  }

  /* -------------------------------------------------------------- */
  /*                                                     Clustering */
  /* -------------------------------------------------------------- */

  LL = vl_gmm_cluster(gmm, data, dimension, numData, numClusters) ;

  /* copy centers */
  OUT(MEANS) = mxCreateNumericMatrix (dimension, numClusters, classID, mxREAL) ;
  OUT(SIGMAS) = mxCreateNumericMatrix (dimension, numClusters, classID, mxREAL) ;
  OUT(WEIGHTS) = mxCreateNumericMatrix (numClusters, 1, classID, mxREAL) ;
  OUT(POSTERIORS) = mxCreateNumericMatrix (numData, numClusters, classID, mxREAL) ;

  memcpy (mxGetData(OUT(MEANS)),
          vl_gmm_get_means (gmm),
          vl_get_type_size (dataType) * dimension * vl_gmm_get_num_clusters(gmm)) ;

  memcpy (mxGetData(OUT(SIGMAS)),
          vl_gmm_get_sigmas (gmm),
          vl_get_type_size (dataType) * dimension * vl_gmm_get_num_clusters(gmm)) ;

  memcpy (mxGetData(OUT(WEIGHTS)),
          vl_gmm_get_weights (gmm),
          vl_get_type_size (dataType) * vl_gmm_get_num_clusters(gmm)) ;

  /* optionally return loglikelyhood */
  if (nout > 3)
  {
    OUT(LL) = vlmxCreatePlainScalar (LL) ;
  }

  /* optionally return posterior probabilities */
  if (nout > 4)
  {
    memcpy (mxGetData(OUT(POSTERIORS)),
            vl_gmm_get_posteriors (gmm),
            vl_get_type_size (dataType) * numData * vl_gmm_get_num_clusters(gmm)) ;
  }



  vl_gmm_delete (gmm) ;
}
