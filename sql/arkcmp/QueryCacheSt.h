/**********************************************************************
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 1994-2015 Hewlett-Packard Development Company, L.P.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
// @@@ END COPYRIGHT @@@
**********************************************************************/
/* -*-C++-*-
 *****************************************************************************
 *
 * File:         QueryCacheSt.h 
 * Description:  
 *               
 *               
 * Created:      3/19/2002
 * Language:     C++
 *
 *
 *
 *
 *****************************************************************************
 */

#ifndef QCSTATISTICS_H
#define QCSTATISTICS_H

#include "CmpISPStd.h"
#include "NABasicObject.h"
#include "QCache.h"

// ---------------------------------------------------------------------------------------------------------
// QueryCacheStatStoredProcedure is a class that contains functions used by the
// QueryCache virtual table, whose purpose is to serve as an interface to the
// SQL/MX query cache. This table is implemented as an internal stored 
// procedure.
// --------------------------------------------------------------------------------------------------------
class QueryCacheStatStoredProcedure {

public:
  QueryCacheStatStoredProcedure();
  virtual ~QueryCacheStatStoredProcedure();

  // Initialize() is called at the time when the stored procedure is initially
  // being registered with ARKCMP.  The default implementation does  
  // nothing and the derived class can implement whatever initialization 
  // behavior it wants.
  static void Initialize(SP_REGISTER_FUNCPTR regFunc);

  // sp_Compile. For Embedded SQL environment, a stored procedure is  
  // compiled only  the first time it is invoked.	
  static SP_STATUS sp_Compile(SP_COMPILE_ACTION action,
			      SP_COMPILE_HANDLE *pCompileObj,
			      SP_HANDLE pObj,
			      SP_ERROR_STRUCT* error)
  {
    return SP_SUCCESS;
  }

  // sp_InputFormat is called with action=OPEN before any compile-time 
  // functions are called.  It is then again called after all compile-time
  // functions have been called, this time with action=CLOSE.
  static SP_STATUS sp_InputFormat(SP_FIELDDESC_STRUCT *inputFieldFormat,
				  Lng32 numFields,
				  SP_COMPILE_HANDLE spCompileObj,
				  SP_HANDLE spObj,
	    			  SP_ERROR_STRUCT *error);

  // sp_NumOutputFields function is called at compile-time of the stored 
  // procedure to inquire about the number of output fields in a row.
  static SP_STATUS sp_NumOutputFields(Lng32 *numFields,
				      SP_COMPILE_HANDLE spCompileObj,
				      SP_HANDLE spObj,
				      SP_ERROR_STRUCT *error);

  // sp_OutputFormat is called at compile-time of the stored procedure to 
  // determine  the format (type info) of each field that will become part of the 
  // row being  output from the stored procedure.
  static SP_STATUS sp_OutputFormat(SP_FIELDDESC_STRUCT *outputFieldFormat,
			           SP_KEYDESC_STRUCT keyFields[],
				   Lng32 *numKeyFields,
				   SP_HANDLE spCompileObj,
				   SP_HANDLE spObj,
				   SP_ERROR_STRUCT *error);

  // sp_Process is called at run-time of the stored procedure. 
  static SP_STATUS sp_Process(SP_PROCESS_ACTION action,
     			      SP_ROW_DATA inputData,
	     		      SP_EXTRACT_FUNCPTR eFunc,
			      SP_ROW_DATA outputData,
			      SP_FORMAT_FUNCPTR fFunc,
			      SP_KEY_VALUE keys,
			      SP_KEYVALUE_FUNCPTR kFunc,
			      SP_PROCESS_HANDLE *spProcHandle,
			      SP_HANDLE spObj,
			      SP_ERROR_STRUCT *error);
};


// ---------------------------------------------------------------------------------------------------------
// QueryCacheEntriesStoredProcedure is a class that contains functions used by 
// the QueryCacheEntries virtual table, whose purpose is to serve as an interface 
// to the  SQL/MX query cache. This table is implemented as an internal stored 
// procedure.
// --------------------------------------------------------------------------------------------------------
class QueryCacheEntriesStoredProcedure  {

public:
  QueryCacheEntriesStoredProcedure();
  virtual ~QueryCacheEntriesStoredProcedure();

  // Initialize() is called at the time when the stored procedure is initially
  // being registered with ARKCMP.  The default implementation does  
  // nothing and the derived class can implement whatever initialization 
  // behavior it wants.
  static void Initialize(SP_REGISTER_FUNCPTR regFunc);

  // sp_Compile. For Embedded SQL environment, a stored procedure is  
  // compiled only the first time it is invoked.	
  static SP_STATUS sp_Compile(SP_COMPILE_ACTION action,
			      SP_COMPILE_HANDLE *pCompileObj,
			      SP_HANDLE pObj,
			      SP_ERROR_STRUCT* error)
  {
    return SP_SUCCESS;
  }

  // sp_InputFormat is called with action OPEN before any compile-time 
  // functions are called.  It is then again called after all compile-time
  // functions have been called, this time with action CLOSE.
  static SP_STATUS sp_InputFormat(SP_FIELDDESC_STRUCT *inputFieldFormat,
	    			  Lng32 numFields,
				  SP_COMPILE_HANDLE spCompileObj,
				  SP_HANDLE spObj,
				  SP_ERROR_STRUCT *error);

  // sp_NumOutputFields function is called at compile-time of the stored 
  // procedure to inquire about the number of output fields in a row.
  static SP_STATUS sp_NumOutputFields(Lng32 *numFields,
				      SP_COMPILE_HANDLE spCompileObj,
				      SP_HANDLE spObj,
				      SP_ERROR_STRUCT *error);

  // sp_OutputFormat is called at compile-time of the stored procedure to 
  // determine the format (type info) of each field that will become part of the 
  // row being output from the stored procedure.
  static SP_STATUS sp_OutputFormat(SP_FIELDDESC_STRUCT *outputFieldFormat,
				   SP_KEYDESC_STRUCT keyFields[],
				   Lng32 *numKeyFields,
				   SP_HANDLE spCompileObj,
				   SP_HANDLE spObj,
				   SP_ERROR_STRUCT *error);

  // sp_Process is called at run-time of the stored procedure.  
  static SP_STATUS sp_Process(SP_PROCESS_ACTION action,
	 		      SP_ROW_DATA inputData,
			      SP_EXTRACT_FUNCPTR eFunc,
			      SP_ROW_DATA outputData,
			      SP_FORMAT_FUNCPTR fFunc,
			      SP_KEY_VALUE keys,
			      SP_KEYVALUE_FUNCPTR kFunc,
			      SP_PROCESS_HANDLE *spProcHandle,
			      SP_HANDLE spObj,
			      SP_ERROR_STRUCT *error);

};

//-----------------------------------------------------------------------
// QueryCacheDeleteStoredProcedure is a class that contains functions used
// to delete the contents of the  QueryCache virtual table. The delete 
// function is implemented as an internal stored procedure.
//-----------------------------------------------------------------------
class QueryCacheDeleteStoredProcedure 
{

public:

  QueryCacheDeleteStoredProcedure();
  virtual ~QueryCacheDeleteStoredProcedure();
  
  // Initialize() is called at the time when the stored procedure is
  // being registered with arkcmp.
  static void Initialize(SP_REGISTER_FUNCPTR regFunc);
  
  // sp_Compile. For Embedded SQL environment, a stored procedure is  
  // compiled only  the first time it is invoked.   
  static SP_STATUS sp_Compile(SP_COMPILE_ACTION action,
                              SP_COMPILE_HANDLE *pCompileObj,
                              SP_HANDLE pObj,
                              SP_ERROR_STRUCT* error)
  {
    return SP_SUCCESS;
  }
                                
  // sp_InputFormat is called with action=OPEN before any compile-time 
  // functions are called.  It is then again called after all compile-time
  // functions have been called, this time with action=CLOSE.
  static SP_STATUS sp_InputFormat(SP_FIELDDESC_STRUCT *inputFieldFormat,
                                  Lng32 numFields,
                                  SP_COMPILE_HANDLE spCompileObj,
                                  SP_HANDLE spObj,
                                  SP_ERROR_STRUCT *error)
  {
    return SP_SUCCESS;  
  }    

  // sp_NumOutputFields function is called at compile-time of the stored 
  // procedure to inquire about the number of output fields in a row.
  static SP_STATUS sp_NumOutputFields(Lng32 *numFields,
                                      SP_COMPILE_HANDLE spCompileObj,
                                      SP_HANDLE spObj,
                                      SP_ERROR_STRUCT *error)
  {
    *numFields = 0;
    return SP_SUCCESS;
  }

  // sp_OutputFormat is called at compile-time of the stored procedure to 
  // determine  the format (type info) of each field that will become part of the 
  // row being  output from the stored procedure.
  static SP_STATUS sp_OutputFormat(SP_FIELDDESC_STRUCT *outputFieldFormat,
                                   SP_KEYDESC_STRUCT keyFields[],
                                   Lng32 *numKeyFields,
                                   SP_HANDLE spCompileObj,
                                   SP_HANDLE spObj,
                                   SP_ERROR_STRUCT *error)
  {
    return SP_SUCCESS;
  }

  // sp_Process is called at run-time of the stored procedure. 
  static SP_STATUS sp_Process(SP_PROCESS_ACTION action,
                              SP_ROW_DATA inputData,
                              SP_EXTRACT_FUNCPTR eFunc,
                              SP_ROW_DATA outputData,
                              SP_FORMAT_FUNCPTR fFunc,
                              SP_KEY_VALUE keys,
                              SP_KEYVALUE_FUNCPTR kFunc,
                              SP_PROCESS_HANDLE *spProcHandle,
                              SP_HANDLE spObj,
                              SP_ERROR_STRUCT *error);
};    // class QueryCacheDeleteStoredProcedure


class HybridQueryCacheStatStoredProcedure {

public:
  HybridQueryCacheStatStoredProcedure();
  virtual ~HybridQueryCacheStatStoredProcedure();

  // Initialize() is called at the time when the stored procedure is initially
  // being registered with ARKCMP.  The default implementation does  
  // nothing and the derived class can implement whatever initialization 
  // behavior it wants.
  static void Initialize(SP_REGISTER_FUNCPTR regFunc);

  // sp_Compile. For Embedded SQL environment, a stored procedure is  
  // compiled only  the first time it is invoked.	
  static SP_STATUS sp_Compile(SP_COMPILE_ACTION action,
			      SP_COMPILE_HANDLE *pCompileObj,
			      SP_HANDLE pObj,
			      SP_ERROR_STRUCT* error)
  {
    return SP_SUCCESS;
  }

  // sp_InputFormat is called with action=OPEN before any compile-time 
  // functions are called.  It is then again called after all compile-time
  // functions have been called, this time with action=CLOSE.
  static SP_STATUS sp_InputFormat(SP_FIELDDESC_STRUCT *inputFieldFormat,
				  Lng32 numFields,
				  SP_COMPILE_HANDLE spCompileObj,
				  SP_HANDLE spObj,
	    			  SP_ERROR_STRUCT *error);
  
  // sp_NumOutputFields function is called at compile-time of the stored 
  // procedure to inquire about the number of output fields in a row.
  static SP_STATUS sp_NumOutputFields(Lng32 *numFields,
				      SP_COMPILE_HANDLE spCompileObj,
				      SP_HANDLE spObj,
				      SP_ERROR_STRUCT *error);

  // sp_OutputFormat is called at compile-time of the stored procedure to 
  // determine  the format (type info) of each field that will become part of the 
  // row being  output from the stored procedure.
  static SP_STATUS sp_OutputFormat(SP_FIELDDESC_STRUCT *outputFieldFormat,
			           SP_KEYDESC_STRUCT keyFields[],
				   Lng32 *numKeyFields,
				   SP_HANDLE spCompileObj,
				   SP_HANDLE spObj,
				   SP_ERROR_STRUCT *error);

  // sp_Process is called at run-time of the stored procedure. 
  static SP_STATUS sp_Process(SP_PROCESS_ACTION action,
     			      SP_ROW_DATA inputData,
	     		      SP_EXTRACT_FUNCPTR eFunc,
			      SP_ROW_DATA outputData,
			      SP_FORMAT_FUNCPTR fFunc,
			      SP_KEY_VALUE keys,
			      SP_KEYVALUE_FUNCPTR kFunc,
			      SP_PROCESS_HANDLE *spProcHandle,
			      SP_HANDLE spObj,
			      SP_ERROR_STRUCT *error);
};

class HybridQueryCacheEntriesStoredProcedure {

public:
  HybridQueryCacheEntriesStoredProcedure();
  virtual ~HybridQueryCacheEntriesStoredProcedure();

  // Initialize() is called at the time when the stored procedure is initially
  // being registered with ARKCMP.  The default implementation does  
  // nothing and the derived class can implement whatever initialization 
  // behavior it wants.
  static void Initialize(SP_REGISTER_FUNCPTR regFunc);

  // sp_Compile. For Embedded SQL environment, a stored procedure is  
  // compiled only  the first time it is invoked.	
  static SP_STATUS sp_Compile(SP_COMPILE_ACTION action,
			      SP_COMPILE_HANDLE *pCompileObj,
			      SP_HANDLE pObj,
			      SP_ERROR_STRUCT* error)
  {
    return SP_SUCCESS;
  }

  // sp_InputFormat is called with action=OPEN before any compile-time 
  // functions are called.  It is then again called after all compile-time
  // functions have been called, this time with action=CLOSE.
  static SP_STATUS sp_InputFormat(SP_FIELDDESC_STRUCT *inputFieldFormat,
				  Lng32 numFields,
				  SP_COMPILE_HANDLE spCompileObj,
				  SP_HANDLE spObj,
	    			  SP_ERROR_STRUCT *error);

  // sp_NumOutputFields function is called at compile-time of the stored 
  // procedure to inquire about the number of output fields in a row.
  static SP_STATUS sp_NumOutputFields(Lng32 *numFields,
				      SP_COMPILE_HANDLE spCompileObj,
				      SP_HANDLE spObj,
				      SP_ERROR_STRUCT *error);

  // sp_OutputFormat is called at compile-time of the stored procedure to 
  // determine  the format (type info) of each field that will become part of the 
  // row being  output from the stored procedure.
  static SP_STATUS sp_OutputFormat(SP_FIELDDESC_STRUCT *outputFieldFormat,
			           SP_KEYDESC_STRUCT keyFields[],
				   Lng32 *numKeyFields,
				   SP_HANDLE spCompileObj,
				   SP_HANDLE spObj,
				   SP_ERROR_STRUCT *error);

  // sp_Process is called at run-time of the stored procedure. 
  static SP_STATUS sp_Process(SP_PROCESS_ACTION action,
     			      SP_ROW_DATA inputData,
	     		      SP_EXTRACT_FUNCPTR eFunc,
			      SP_ROW_DATA outputData,
			      SP_FORMAT_FUNCPTR fFunc,
			      SP_KEY_VALUE keys,
			      SP_KEYVALUE_FUNCPTR kFunc,
			      SP_PROCESS_HANDLE *spProcHandle,
			      SP_HANDLE spObj,
			      SP_ERROR_STRUCT *error);
};


class ISPIterator 
{

public:
  ISPIterator(const NAArray<CmpContextInfo*> & ctxs, CollHeap * h)
  : currCacheIndex_(-1)
  , contextName_(h)
  , ctxInfos_(ctxs)
  , heap_(h) 
  {}
  
  NABoolean initializeISPCaches(SP_ROW_DATA  inputData, 
                                SP_EXTRACT_FUNCPTR  eFunc, 
                                SP_ERROR_STRUCT* error, 
                                const NAArray<CmpContextInfo*> & ctxs, //input 
                                NAString & contextName,  // out
                                Int32 & index           //out, set initial index in arrary of CmpContextInfos
                                ) ;
protected:
  Int32 currCacheIndex_;
  NAString contextName_;
  const NAArray<CmpContextInfo*> & ctxInfos_;
  CollHeap * heap_;
  
};

class QueryCacheStatsISPIterator : public ISPIterator
{
public:
  QueryCacheStatsISPIterator(SP_ROW_DATA  inputData, SP_EXTRACT_FUNCPTR  eFunc, 
                             SP_ERROR_STRUCT* error, const NAArray<CmpContextInfo*> & ctxs, CollHeap * h);
  //if currCacheIndex_ is set 0, currQCache_ is not used and should always be NULL
  NABoolean getNext(QueryCacheStats & stats);
private:
 QueryCache* currQCache_;
};

class QueryCacheEntriesISPIterator : public ISPIterator
{
public:
  QueryCacheEntriesISPIterator(SP_ROW_DATA  inputData, SP_EXTRACT_FUNCPTR  eFunc, 
                               SP_ERROR_STRUCT* error, const NAArray<CmpContextInfo*> & ctxs, CollHeap * h);
  
  NABoolean getNext(QueryCacheDetails & details);
  Int32 & counter() { return counter_; }
private:
  Int32 counter_;
  LRUList::iterator SQCIterator_;
  QueryCache* currQCache_;
};

class HybridQueryCacheStatsISPIterator : public ISPIterator
{
public:
    HybridQueryCacheStatsISPIterator(SP_ROW_DATA  inputData, SP_EXTRACT_FUNCPTR  eFunc, 
                                     SP_ERROR_STRUCT* error, const NAArray<CmpContextInfo*> & ctxs, CollHeap * h);

    NABoolean getNext(HybridQueryCacheStats & stats);
private:
 QueryCache* currQCache_;
};

class HybridQueryCacheEntriesISPIterator : public ISPIterator
{
public:
    HybridQueryCacheEntriesISPIterator(SP_ROW_DATA  inputData, SP_EXTRACT_FUNCPTR  eFunc, 
                                       SP_ERROR_STRUCT* error, const NAArray<CmpContextInfo*> & ctxs, CollHeap * h);

    ~HybridQueryCacheEntriesISPIterator()
    {
        if(HQCIterator_)
            delete HQCIterator_;
    }
    NABoolean getNext(HybridQueryCacheDetails & details);
private:
    Int32 currEntryIndex_;
    Int32 currEntriesPerKey_;
    HQCCacheKey* currHKeyPtr_;
    HQCCacheData* currValueList_;
    HQCHashTblItor* HQCIterator_;
    LRUList::iterator SQCIterator_;
  QueryCache* currQCache_;
};

#endif

