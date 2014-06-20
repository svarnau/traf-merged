
/**********************************************************************
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2003-2014 Hewlett-Packard Development Company, L.P.
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
 * File:         ExFastTransport.cpp
 * Description:  TDB/TCB for Fast transport
 *
 * Created:      08/28/2012
 * Language:     C++
 *
 *
 *****************************************************************************
 */
#include "ExFastTransportIO.h"
#include "ex_stdh.h"
#include "ExFastTransport.h"
#include "ex_globals.h"
#include "ex_exe_stmt_globals.h"
#include "exp_attrs.h"
#include "exp_clause_derived.h"
#include "ex_error.h"
#include "ExStats.h"
#include "ExCextdecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include "ComSysUtils.h"
#ifndef __EID
#include "SequenceFileReader.h" 
#endif



//----------------------------------------------------------------------
// ExFastExtractTcb methods
//----------------------------------------------------------------------
ex_tcb *ExFastExtractTdb::build(ex_globals *glob)
{
  // first build the child TCBs
  ex_tcb *childTcb ;
  ExFastExtractTcb *feTcb;

  childTcb = childTdb_->build(glob);

  if (!getIsHiveInsert())
  {
    feTcb = new (glob->getSpace()) ExFastExtractTcb(*this, *childTcb,glob);
  }
  else
  {
    feTcb = new (glob->getSpace()) ExHdfsFastExtractTcb(*this, *childTcb,glob);
  }

  feTcb->registerSubtasks();
  feTcb->registerResizeSubtasks();

  return feTcb;

}

ExFastExtractTcb::ExFastExtractTcb(
    const ExFastExtractTdb &fteTdb,
    const ex_tcb & childTcb,
    ex_globals *glob)
  : ex_tcb(fteTdb, 1, glob),
    workAtp_(NULL),
    outputPool_(NULL),
    inputPool_(NULL),
    childTcb_(&childTcb)
  , fileWriter_(NULL)
  , inSqlBuffer_(NULL)
  , childOutputTD_(NULL)
  , sourceFieldsConvIndex_(NULL)
  , currBuffer_(NULL)
  , trySleep_(TRUE)
  , ioSyncState_(PENDING_NONE)
  , bufferAllocFailuresCount_(0)
  , retCodet_(-1)
  , writeQueue_(NULL)
{
  
  ex_globals *stmtGlobals = getGlobals();

  Space *globSpace = getSpace();
  CollHeap *globHeap = getHeap();

  heap_ = globHeap;

  //convert to non constant to access the members.
  ExFastExtractTdb *mytdb = (ExFastExtractTdb*)&fteTdb;
  numBuffers_ = mytdb->getNumIOBuffers();
  ioTimeout_  = (Float64) mytdb->getIoTimeout();

   // NOT USED in M9
  /* 
  // Allocate output buffer pool
  if (myTdb().getOutputExpression() != NULL)
  {
    int bufferSize = (int) myTdb().getOutputSqlBufferSize();
    
//    outputPool_ = new (globSpace)
//      sql_buffer_pool((Lng32) myTdb().getNumOutputBuffers(),
//                      (Lng32) bufferSize,
//                      globSpace,
//                      SqlBufferBase::NORMAL_);
  }
  // Allocate input buffer pool
  if (myTdb().getInputExpression() != NULL)
  {
    inputPool_ = new (globSpace)
      sql_buffer_pool((Lng32) myTdb().getNumInputBuffers(),
                      (Lng32) myTdb().getInputSqlBufferSize(),
                      globSpace,
                      SqlBufferBase::NORMAL_);
                      } */

  // Allocate queues to communicate with parent
  allocateParentQueues(qParent_);

  // get the queue that child use to communicate with me
  qChild_  = childTcb.getParentQueue();
    


  // Allocate the work ATP
  if (myTdb().getWorkCriDesc())
    workAtp_ = allocateAtp(myTdb().getWorkCriDesc(), globSpace);

  // Fixup expressions
  // NOT USED in M9
  /*
  if (myTdb().getInputExpression())
    myTdb().getInputExpression()->fixup(0, getExpressionMode(), this,
                                       globSpace, globHeap);

  if (myTdb().getOutputExpression())
    myTdb().getOutputExpression()->fixup(0, getExpressionMode(), this,
                                        globSpace, globHeap);
  */

  if (myTdb().getChildDataExpr())
    myTdb().getChildDataExpr()->fixup(0,getExpressionMode(),this,
                                       globSpace, globHeap);


  //maybe we can move the below few line to the init section od work methods??
  UInt32 numAttrs = myTdb().getChildTuple()->numAttrs();

   sourceFieldsConvIndex_ = (int *)((NAHeap *)heap_)->allocateAlignedHeapMemory((UInt32)(sizeof(int) * numAttrs), 512, FALSE);

  maxExtractRowLength_ = ROUND8(myTdb().getChildTuple2()->tupleDataLength() + myTdb().getChildTuple2()->numAttrs());

  const ULng32 sqlBufferSize = maxExtractRowLength_ +
                               ROUND8(sizeof(SqlBufferNormal)) +
                               sizeof(tupp_descriptor);

  inSqlBuffer_ = (SqlBuffer *) new (heap_) char[sqlBufferSize];
  inSqlBuffer_->driveInit(sqlBufferSize, TRUE, SqlBuffer::NORMAL_);
  childOutputTD_ = inSqlBuffer_->add_tuple_desc(maxExtractRowLength_);

  endOfData_ = FALSE;

  //Thread related stuff.
  param_ = new(heap_)struct Params;

  threadState_ = new(heap_)ThreadState;
  *threadState_ = THREAD_OK;

  threadFlag_ = new(heap_)ThreadFlag;
  *threadFlag_ = FLAG_NORMAL_RUN;

  errMsg_ = new(heap_)ErrorMsg;
  errMsg_->ec = EXE_OK;

  attr_ = new (heap_) pthread_attr_t;
  queueMutex_ = new(heap_) pthread_mutex_t;
  queueReadyCv_ = new(heap_) pthread_cond_t;

} // ExFastExtractTcb::ExFastExtractTcb

ExFastExtractTcb::~ExFastExtractTcb()
{
  // Release resources acquired
  //
  freeResources();

  delete qParent_.up;
  delete qParent_.down;
 
  if (workAtp_)
  {
    workAtp_->release();
    deallocateAtp(workAtp_, getSpace());
  }

  if (fileWriter_)
  {
    NADELETE( fileWriter_, FileReadWrite, getHeap());
    fileWriter_ = NULL;
  }

  if (inSqlBuffer_ && getHeap())
  {
    getHeap()->deallocateMemory(inSqlBuffer_);
    inSqlBuffer_ = NULL;
    childOutputTD_ = NULL;
  }
  if (sourceFieldsConvIndex_)
    getHeap()->deallocateMemory(sourceFieldsConvIndex_);

} // ExFastExtractTcb::~ExFastExtractTcb()

//
// This function frees any resources acquired.
// It should only be called by the TCB destructor.
//
void ExFastExtractTcb::freeResources()
{
	for(Int16 i=0; i < numBuffers_; i++)
	{
		if(bufferPool_[i])
		{
			NADELETE(bufferPool_[i], IOBuffer, getHeap());
			bufferPool_[i] = NULL;
		}
	}
	if(writeQueue_)
	{
		writeQueue_->cleanup();
		NADELETE(writeQueue_, SimpleQueue, getHeap());
		writeQueue_ = NULL;
	}
	NADELETEBASIC(param_, getHeap());
	NADELETEBASIC(errMsg_, getHeap());

	NADELETEBASIC(attr_, getHeap());
	NADELETEBASIC(threadFlag_, getHeap());
	NADELETEBASIC(threadState_, getHeap());
	NADELETEBASIC(queueMutex_, getHeap());
	NADELETEBASIC(queueReadyCv_, getHeap());
}

void ExFastExtractTcb::registerSubtasks()
{
  ex_tcb :: registerSubtasks();
}

ex_tcb_private_state *ExFastExtractTcb::allocatePstates(
  Lng32 &numElems,      // [IN/OUT] desired/actual elements
  Lng32 &pstateLength)  // [OUT] length of one element
{
  PstateAllocator<ExFastExtractPrivateState> pa;
  return pa.allocatePstates(this, numElems, pstateLength);
}

// Insert a single entry into the up queue and optionally
// remove the head of the down queue
//
// Right now this function does not handle data rows, only error
// and end-of-data. It could possibly be extended to handle a data
// row. I have not looked at that closely enough yet.
//
void ExFastExtractTcb::insertUpQueueEntry(ex_queue::up_status status,
                                  ComDiagsArea *diags,
                                  NABoolean popDownQueue)
{

  ex_queue_entry *upEntry = qParent_.up->getTailEntry();
  ex_queue_entry *downEntry = qParent_.down->getHeadEntry();
  ExFastExtractPrivateState &privateState =
    *((ExFastExtractPrivateState *) downEntry->pstate);

  // Initialize the up queue entry. 
  //
  // copyAtp() will copy all tuple pointers and the diags area from
  // the down queue entry to the up queue entry.
  //
  // When we return Q_NO_DATA if the match count is > 0:
  // * assume down queue diags were returned with the Q_OK_MMORE entries
  // * release down queue diags before copyAtp()
  //
  if (status == ex_queue::Q_NO_DATA && privateState.matchCount_ > 0)
  {
    downEntry->setDiagsArea(NULL);
    upEntry->copyAtp(downEntry);
  }
  else
  {
    upEntry->copyAtp(downEntry);
    downEntry->setDiagsArea(NULL);
  }

  upEntry->upState.status = status;
  upEntry->upState.parentIndex = downEntry->downState.parentIndex;
  upEntry->upState.downIndex = qParent_.down->getHeadIndex();
  upEntry->upState.setMatchNo(privateState.matchCount_);
  
  // Move any diags to the up queue entry
  if (diags != NULL)
  {
    ComDiagsArea *atpDiags = upEntry->getDiagsArea();
    if (atpDiags == NULL)
    {
      // setDiagsArea() does not increment the reference count
      upEntry->setDiagsArea(diags);
      diags->incrRefCount();
    }
    else
    {
      atpDiags->mergeAfter(*diags);
    }
  }
  
  // Insert into up queue
  qParent_.up->insert();
 
  // Optionally remove the head of the down queue
  if (popDownQueue)
  {
    privateState.init();
    qParent_.down->removeHead();
  }
}


const char *ExFastExtractTcb::getTcbStateString(FastExtractStates s)
{
  switch (s)
  {
    case EXTRACT_NOT_STARTED:           return "EXTRACT_NOT_STARTED";
    case EXTRACT_INITIALIZE:            return "EXTRACT_INITIALIZE";
    case EXTRACT_PASS_REQUEST_TO_CHILD: return "EXTRACT_PASS_REQUEST_TO_CHILD";
    case EXTRACT_RETURN_ROWS_FROM_CHILD:return "EXTRACT_RETURN_ROWS_FROM_CHILD";
    case EXTRACT_DATA_READY_TO_SEND:    return "EXTRACT_DATA_READY_TO_SEND";
    case EXTRACT_SYNC_WITH_IO_THREAD:   return "EXTRACT_SYNC_WITH_IO_THREAD";  
    case EXTRACT_ERROR:                 return "EXTRACT_ERROR";
    case EXTRACT_DONE:                  return "EXTRACT_DONE";
    case EXTRACT_CANCELED:              return "EXTRACT_CANCELED";
    default:                            return "UNKNOWN";
  }
}

/* Work method, where all the action happens
The stick figure shows transitions between various states.
Transitions to EXTRACT_ERROR are not shown for clarity.

EXTRACT_NOT_STARTED
        |
        |
        V
EXTRACT_INITIALIZE
        |
        |
        V
EXTRACT_PASS_REQUEST_TO_CHILD                 
        |                                             
        |                   |----|
        V                   V    |            
EXTRACT_RETURN_ROWS_FROM_CHILD ->|    EXTRACT_ERROR 
        |      ^           |                | 
        |      |       |------|             |
        V      |       V   |  |             |   
EXTRACT_SYNC_WITH_IO_THREAD|->|             |
        |      ^           |                | 
        |      |           |                |
        V      |           |                V         
EXTRACT_DATA_READY_TO_SEND<-           EXTRACT_CANCELLED  
                       \               /
                        \             /
                         V           V
                         EXTRACT_DONE



*/
ExWorkProcRetcode ExFastExtractTcb::work()
{

  return WORK_OK;

}//ExFastExtractTcb::work()



void ExFastExtractTcb::updateWorkATPDiagsArea(ComDiagsArea *da)
{
    if (da)
    {
      if (workAtp_->getDiagsArea())
      {
        workAtp_->getDiagsArea()->mergeAfter(*da);
      }
      else
      {
        ComDiagsArea * da1 = da;
        workAtp_->setDiagsArea(da1);
        da1->incrRefCount();
      }
    }
}
void ExFastExtractTcb::updateWorkATPDiagsArea(ex_queue_entry * centry)
{
    if (centry->getDiagsArea())
    {
      if (workAtp_->getDiagsArea())
      {
        workAtp_->getDiagsArea()->mergeAfter(*centry->getDiagsArea());
      }
      else
      {
        ComDiagsArea * da = centry->getDiagsArea();
        workAtp_->setDiagsArea(da);
        da->incrRefCount();
        centry->setDiagsArea(0);
      }
    }
}

void ExFastExtractTcb::updateWorkATPDiagsArea(ExeErrorCode rc, const char *msg)
{
    ComDiagsArea *da = workAtp_->getDiagsArea();
    if(!da)
    {
      da = ComDiagsArea::allocate(getHeap());
      workAtp_->setDiagsArea(da);
    }
   if(msg != NULL)
   {
	   *da << DgSqlCode(-rc)
    	   << DgString0(msg);
   }
   else
   {
	  *da << DgSqlCode(-rc);
   }

}

void ExFastExtractTcb::updateWorkATPDiagsArea(const char *file, 
                                              int line, const char *msg)
{
    ComDiagsArea *da = workAtp_->getDiagsArea();
    if(!da)
    {
      da = ComDiagsArea::allocate(getHeap());
      workAtp_->setDiagsArea(da);
    }
   
    *da << DgSqlCode(-1001)
        << DgString0(file)
        << DgInt0(line)
        << DgString1(msg);
}

NABoolean ExFastExtractTcb::needStatsEntry()
{
  if ((getGlobals()->getStatsArea()->getCollectStatsType() == ComTdb::ALL_STATS) ||
      (getGlobals()->getStatsArea()->getCollectStatsType() == ComTdb::OPERATOR_STATS))
    return TRUE;
  else
    return FALSE;
}
ExOperStats * ExFastExtractTcb::doAllocateStatsEntry(
                                              CollHeap *heap,
                                              ComTdb *tdb)
{
  ExOperStats *stat = NULL;

  ComTdb::CollectStatsType statsType = getGlobals()->getStatsArea()->getCollectStatsType();

  if (statsType == ComTdb::OPERATOR_STATS)
  {
    return ex_tcb::doAllocateStatsEntry(heap, tdb);;
  }
  else
  {

    return new(heap) ExFastExtractStats( heap,
                                         this,
                                         tdb);
  }
}
///////////////////////////////////
///////////////////////////////////

ExHdfsFastExtractTcb::ExHdfsFastExtractTcb(
    const ExFastExtractTdb &fteTdb,
    const ex_tcb & childTcb,
    ex_globals *glob)
  : ExFastExtractTcb(
      fteTdb,
      childTcb,
      glob),
    sequenceFileWriter_(NULL)
{

} // ExHdfsFastExtractTcb::ExFastExtractTcb

ExHdfsFastExtractTcb::~ExHdfsFastExtractTcb()
{

  if (lobGlob_ != NULL)
  {
    lobGlob_ = NULL;
  }

} // ExHdfsFastExtractTcb::~ExHdfsFastExtractTcb()


Int32 ExHdfsFastExtractTcb::fixup()
{
  lobGlob_ = NULL;

  ex_tcb::fixup();

  ExpLOBinterfaceInit
    (lobGlob_, getGlobals()->getDefaultHeap());

  return 0;
}

ExWorkProcRetcode ExHdfsFastExtractTcb::work()
{
#ifdef __EID 
  // This class should not be instantiated in EID.
  return WORK_BAD_ERROR;
#else
  Lng32 retcode = 0;
  SFW_RetCode sfwRetCode = SFW_OK;
  ULng32 recSepLen = strlen(myTdb().getRecordSeparator());
  ULng32 delimLen = strlen(myTdb().getDelimiter());
  ULng32 nullLen = strlen(myTdb().getNullString());
  if (myTdb().getIsHiveInsert())
  {
    recSepLen = 1;
    delimLen = 1;
    nullLen = 0;
  }

  ExOperStats *stats = NULL;
  ExFastExtractStats *feStats = getFastExtractStats();

  while (TRUE)
  {
    // if no parent request, return
    if (qParent_.down->isEmpty())
      return WORK_OK;

    ex_queue_entry *pentry_down = qParent_.down->getHeadEntry();
    const ex_queue::down_request request = pentry_down->downState.request;
    const Lng32 value = pentry_down->downState.requestValue;
    ExFastExtractPrivateState &pstate = *((ExFastExtractPrivateState *) pentry_down->pstate);
    switch (pstate.step_)
    {
      case EXTRACT_NOT_STARTED:
      {
         pstate.step_= EXTRACT_INITIALIZE;
      }
      //  no break here
      case EXTRACT_INITIALIZE:
      {

        //ex_assert (lobGlobals != NULL, "lobGlobals is null");
        //lobGlob_ = (void*)lobGlobals;

        pstate.processingStarted_ = FALSE;
        ErrorOccured_ = FALSE;

        //Allocate writeBuffers. This pool is used by EID thread to
        //extract and fill the buffer. This buffer when full is inserted into
        //writeQueue for writeThread to write. Once written, the free buffer is
        //put back to pool.

        numBuffers_ = 1;
        for (Int16 i = 0; i < numBuffers_; i++)
        {
          bool done = false;
          Int64 input_datalen = myTdb().getHdfsIoBufferSize();
          char * buf_addr = 0;
          while ((!done) && input_datalen >= 32 * 1024)
          {
            buf_addr = 0;
            buf_addr = (char *)((NAHeap *)heap_)->allocateAlignedHeapMemory((UInt32)input_datalen, 512, FALSE);
            if (buf_addr)
            {
              done = true;
              bufferPool_[i] = new (heap_) IOBuffer((char*) buf_addr, (Int32)input_datalen);
            }
            else
            {
              bufferAllocFailuresCount_++;
              input_datalen = input_datalen / 2;
            }
          }
          if (!done)
          {
            numBuffers_ = i;
            break ; // if too few buffers have been allocated we will raise
          }         // an error later
        }

        if (feStats)
        {
          feStats->setBufferAllocFailuresCount(bufferAllocFailuresCount_);
          feStats->setBuffersCount(numBuffers_);
        }

//        if (numBuffers_ < 2)
//        {
//          updateWorkATPDiagsArea(EXE_EXTRACT_CANNOT_ALLOCATE_BUFFER);
//          pstate.step_ = EXTRACT_ERROR;
//          break;
//        }

        ComDiagsArea *da = NULL;
	//        char outFileName[500];
        if (myTdb().getTargetFile())
        {

          Lng32 fileNum = getGlobals()->castToExExeStmtGlobals()->getMyInstanceNumber();
          if (myTdb().getIsHiveInsert())
          {
            memset (hdfsHost_, '\0', sizeof(hdfsHost_)); 
            strncpy(hdfsHost_, myTdb().getHdfsHostName(), sizeof(hdfsHost_));
            hdfsPort_ = myTdb().getHdfsPortNum();
            memset (hdfsFileName_, '\0', sizeof(hdfsFileName_));
            memset (hiveTableLocation_, '\0', sizeof(hiveTableLocation_));

            time_t t;
            time(&t);
            char pt[30];
            struct tm * curgmtime = gmtime(&t);
            strftime(pt, 30, "%Y%m%d%H%M%S", curgmtime);
            srand(getpid());
            snprintf(hiveTableLocation_,999, "%s", myTdb().getTargetName());
            snprintf(hdfsFileName_,999, "%s%d-%s-%d", myTdb().getHiveTableName(), fileNum, pt,rand() % 1000);
          }
          else
          {
             ex_assert(0, "hdfs files only");
          }

	        if (isSequenceFile() && !sequenceFileWriter_)
	          {
	            sequenceFileWriter_ = new(getSpace()) 
                                 SequenceFileWriter((NAHeap *)getSpace());
	            sfwRetCode = sequenceFileWriter_->init();
	            if (sfwRetCode != SFW_OK)
	              {
                  createSequenceFileError(sfwRetCode);
    	    	      pstate.step_ = EXTRACT_ERROR;
    	    	      break;
	              }
	          }
	        
	        if (isSequenceFile())
	          {
	            strcat(hiveTableLocation_, "//");
	            strcat(hiveTableLocation_, hdfsFileName_);
	            //sfwRetCode = sequenceFileWriter_->open("hdfs://localhost:9000/user/hive/warehouse/promotion_seq/000001_0", SFW_COMP_NONE); //hiveTableLocation_);
	            sfwRetCode = sequenceFileWriter_->open(hiveTableLocation_, SFW_COMP_NONE);
	            if (sfwRetCode != SFW_OK)
	              {
                  createSequenceFileError(sfwRetCode);
    	    	      pstate.step_ = EXTRACT_ERROR;
    	    	      break;
	              }
	          }
	        else
	          {
              retcode = 0;
              retcode = ExpLOBinterfaceCreate(lobGlob_,
                                                hdfsFileName_,
                                                hiveTableLocation_,
                                                (Lng32)Lob_External_HDFS_File,
                                                hdfsHost_,
                                                hdfsPort_,
                                                0, //bufferSize -- 0 --> use default
                                                myTdb().getHdfsReplication(), //replication
                                                0 //bloclSize --0 -->use default
                                                );
              if (retcode < 0)
                {
                  Lng32 cliError = 0;
          
                  Lng32 intParam1 = -retcode;
                  ComDiagsArea * diagsArea = NULL;
                  ExRaiseSqlError(getHeap(), &diagsArea,
                                  (ExeErrorCode)(8442), NULL, &intParam1,
                                  &cliError, NULL, (char*)"ExpLOBinterfaceCreate",
                                  getLobErrStr(intParam1));
                  pentry_down->setDiagsArea(diagsArea);
                  pstate.step_ = EXTRACT_ERROR;
                  break;
                }
            }

          if (feStats)
          {
            feStats->setPartitionNumber(fileNum);
          }
        }
        else
        {
          updateWorkATPDiagsArea(__FILE__,__LINE__,"sockets are not supported");
          pstate.step_ = EXTRACT_ERROR;
          break;
        }

        for (UInt32 i = 0; i < myTdb().getChildTuple()->numAttrs(); i++)
        {
          Attributes * attr = myTdb().getChildTableAttr(i);
          Attributes * attr2 = myTdb().getChildTableAttr2(i);

          ex_conv_clause tempClause;
          int convIndex = 0;
          sourceFieldsConvIndex_[i] =
              tempClause.find_case_index(
                                   attr->getDatatype(),
                                   0,
                                   attr2->getDatatype(),
                                   0,
                                   0);

        }

        pstate.step_= EXTRACT_PASS_REQUEST_TO_CHILD;
      }
      break;

      case EXTRACT_PASS_REQUEST_TO_CHILD:
      {
        // pass the parent request to the child downqueue
        if (!qChild_.down->isFull())
        {
          ex_queue_entry * centry = qChild_.down->getTailEntry();

          if (request == ex_queue::GET_N)
            centry->downState.request = ex_queue::GET_ALL;
          else
            centry->downState.request = request;

          centry->downState.requestValue = pentry_down->downState.requestValue;
          centry->downState.parentIndex = qParent_.down->getHeadIndex();
          // set the child's input atp
          centry->passAtp(pentry_down->getAtp());
          qChild_.down->insert();
          pstate.processingStarted_ = TRUE;
        }
        else
          // couldn't pass request to child, return
          return WORK_OK;

        pstate.step_ = EXTRACT_RETURN_ROWS_FROM_CHILD;
      }
        break;
      case EXTRACT_RETURN_ROWS_FROM_CHILD:
      {
        if ((qChild_.up->isEmpty()))
        {
          return WORK_OK;
        }

        if (currBuffer_ == NULL)
        {
          currBuffer_ = bufferPool_[0];
          memset(currBuffer_->data_, '\0',currBuffer_->bufSize_);
          currBuffer_->bytesLeft_ = currBuffer_->bufSize_;
        }

        ex_queue_entry * centry = qChild_.up->getHeadEntry();
        ComDiagsArea *cda = NULL;
        ex_queue::up_status child_status = centry->upState.status;

        switch (child_status)
        {
          case ex_queue::Q_OK_MMORE:
          {
            // for the very first row retruned from child
            // include the header row if necessary
            if ((pstate.matchCount_ == 0) && myTdb().getIncludeHeader())
            {
              if ((!myTdb().getIsAppend()) ||
              (myTdb().getIsAppend() && fileWriter_->atStartOfFile()))
              {
                Int32 headerLength = strlen(myTdb().getHeader());
                char * target = currBuffer_->data_;
                if (headerLength + 1 < currBuffer_->bufSize_)
                {
                  strncpy(target, myTdb().getHeader(),headerLength);
                  target[headerLength] = '\n' ;
                  currBuffer_->bytesLeft_ -= headerLength+1 ;
                }
                else
                {
                  updateWorkATPDiagsArea(__FILE__,__LINE__,"header does not fit in buffer");
                  pstate.step_ = EXTRACT_ERROR;
                  break;
                }
              }
            }

            tupp_descriptor *dataDesc = childOutputTD_;
            ex_expr::exp_return_type expStatus = ex_expr::EXPR_OK;
            if (myTdb().getChildDataExpr())
            {
              UInt32 childTuppIndex = myTdb().childDataTuppIndex_;

              workAtp_->getTupp(childTuppIndex) = dataDesc;

              // Evaluate the child data expression. If diags are generated they
              // will be left in the down entry ATP.
              expStatus = myTdb().getChildDataExpr()->eval(centry->getAtp(), workAtp_);
              workAtp_->getTupp(childTuppIndex).release();

              if (expStatus == ex_expr::EXPR_ERROR)
              {
                updateWorkATPDiagsArea(centry);
                pstate.step_ = EXTRACT_ERROR;
                break;
              }

            } // if (myTdb().getChildDataExpr())
            char * targetData = currBuffer_->data_ + currBuffer_->bufSize_ - currBuffer_->bytesLeft_;
            if (targetData == NULL)
            {
              updateWorkATPDiagsArea(__FILE__,__LINE__,"targetData is NULL");
              pstate.step_ = EXTRACT_ERROR;
              break;
            }
            char * childRow = dataDesc->getTupleAddress();
            ULng32 childRowLen = dataDesc->getAllocatedSize();

            UInt32 vcActualLen = 0;
            NABoolean convError = FALSE;
            for (UInt32 i = 0; i < myTdb().getChildTuple()->numAttrs(); i++)
            {
              Attributes * attr = myTdb().getChildTableAttr( i);
              Attributes * attr2 = myTdb().getChildTableAttr2(i);
              char *childColData = NULL; //childRow + attr->getOffset();
              UInt32 childColLen = 0;
              UInt32 maxTargetColLen = attr2->getLength();

              //format is aligned format--
              //----------
              // field is varchar
              if (attr->getVCIndicatorLength() > 0)
              {
                childColData = childRow + *((UInt16*)(childRow + attr->getVoaOffset()));
                childColLen = attr->getLength(childColData);
                childColData += attr->getVCIndicatorLength();
              }
              else
              {//source is fixed length
                childColData = childRow + attr->getOffset();
                childColLen = attr->getLength();
                if ((attr->getCharSet() == CharInfo::ISO88591 ||
                     attr->getCharSet() == CharInfo::UTF8) &&
                    childColLen > 0)
                {
                  // trim trailing blanks
                  while (childColLen > 0 &&
                         childColData[childColLen -1] == ' ' )
                  {
                    childColLen--;
                  }
                }
                else
                  if (attr->getCharSet() == CharInfo::UCS2 &&
                      childColLen > 1)
                  {
                    ex_assert (childColLen % 2 == 0 , "invalid ucs2");
                    NAWchar* wChildColData = (NAWchar*)childColData;
                    Int32 wChildColLen = childColLen/2;
                    while (wChildColLen > 0 &&
                           wChildColData[wChildColLen -1 ] == L' ')
                    {
                      wChildColLen--;
                    }
                    childColLen = wChildColLen * 2;
                  }
              }

              if (attr->getNullFlag() &&
                  ExpAlignedFormat::isNullValue( childRow + attr->getNullIndOffset(), attr->getNullBitIndex() ))
              {
                if (!myTdb().getIsHiveInsert())// for hive null is empty string
                {
                  memcpy(targetData,myTdb().getNullString(), nullLen );
                  targetData += nullLen;
                }
                currBuffer_->bytesLeft_ -= nullLen;
              }
              else
              {
                switch ((conv_case_index)sourceFieldsConvIndex_[i])
                {
                  case CONV_ASCII_V_V:
                  case CONV_ASCII_F_V:
                  case CONV_UNICODE_V_V:
                  case CONV_UNICODE_F_V:
                  {
                    if (childColLen > 0)
                    {
                      memcpy(targetData,childColData, childColLen );
                      targetData += childColLen;
                      currBuffer_->bytesLeft_ -= childColLen;
                    }
                  }
                  break;

                  default:
                   ex_expr::exp_return_type err =
                       convDoIt(childColData,
                                childColLen,
                                attr->getDatatype(),
                                attr->getPrecision(),
                                attr->getScale(),
                                targetData,
                                attr2->getLength(),
                                attr2->getDatatype(),
                                attr2->getPrecision(),
                                attr2->getScale(),
                                (char*)&vcActualLen,
                                sizeof(vcActualLen),
                                0,
                                0,// diags may need to be added
                                (conv_case_index)sourceFieldsConvIndex_[i]
                                );

                   if (err == ex_expr::EXPR_ERROR)
                   {
                      convError = TRUE;
                      // not exit loop -- we will log the errenous row later
                      // do not cancel processing for this type of error???
                   }
                  targetData += vcActualLen;
                  currBuffer_->bytesLeft_ -= vcActualLen;
                  break;
                }//switch
              }

              if (i == myTdb().getChildTuple()->numAttrs() - 1)
              {
                strncpy(targetData, myTdb().getRecordSeparator(), recSepLen);
                targetData += recSepLen;
                currBuffer_->bytesLeft_ -= recSepLen;
              }
              else
              {
                strncpy(targetData, myTdb().getDelimiter(), delimLen);
                targetData += delimLen;
                currBuffer_->bytesLeft_ -= delimLen;
              }

            }
            pstate.matchCount_++;


            if (!convError)
            {
              if (feStats)
              {
                feStats->incProcessedRowsCount();
              }
              pstate.successRowCount_ ++;
            }
            else
            {
              if (feStats)
              {
                feStats->incErrorRowsCount();
              }
              pstate.errorRowCount_ ++;
            }

            if (currBuffer_->bytesLeft_ < (Int32) maxExtractRowLength_)
            {
              pstate.step_ = EXTRACT_DATA_READY_TO_SEND;
            }
          }
            break;

          case ex_queue::Q_NO_DATA:
          {
            pstate.step_ = EXTRACT_DATA_READY_TO_SEND;
            endOfData_ = TRUE;
            pstate.processingStarted_ = FALSE ; // so that cancel does not
            //wait for this Q_NO_DATA
          }
           break;
          case ex_queue::Q_SQLERROR:
          {
            pstate.step_ = EXTRACT_ERROR;
          }
            break;
          case ex_queue::Q_INVALID:
          {
            updateWorkATPDiagsArea(__FILE__,__LINE__,
                "ExFastExtractTcb::work() Invalid state returned by child");
            pstate.step_ = EXTRACT_ERROR;
          }
            break;

        } // switch
        qChild_.up->removeHead();
      }
        break;

      case EXTRACT_DATA_READY_TO_SEND:
      {
        ssize_t bytesToWrite = currBuffer_->bufSize_ - currBuffer_->bytesLeft_;
        Int64 requestTag = 0;
        Int64 descSyskey = 0;
        if (isSequenceFile())
          {
            sfwRetCode = sequenceFileWriter_->writeBuffer(currBuffer_->data_, bytesToWrite, myTdb().getRecordSeparator());
            if (sfwRetCode != SFW_OK)
              {
                createSequenceFileError(sfwRetCode);
  	    	      pstate.step_ = EXTRACT_ERROR;
  	    	      break;
              }
          }
        else
          {
            retcode = 0;
            retcode = ExpLOBInterfaceInsert(lobGlob_,
                                        hdfsFileName_,
                                        hiveTableLocation_,
                                        (Lng32)Lob_External_HDFS_File,
                                        hdfsHost_,
                                        hdfsPort_,
                                        0,
                                        NULL,  //lobHandle == NULL -->simpleInsert
                                        NULL,
                                        NULL,
                                        0,
                                        NULL,
                                        requestTag,
                                        0,
                                        descSyskey,
                                        NULL,
                                        Lob_None,//LobsSubOper so
                                        0,  //checkStatus
                                        1,  //waitedOp
                                        currBuffer_->data_,
                                        bytesToWrite,
                                        0,     //bufferSize
                                        myTdb().getHdfsReplication(),     //replication
                                        0      //blockSize
                                        );
            if (retcode < 0)
              {
                Lng32 cliError = 0;
    
                Lng32 intParam1 = -retcode;
                ComDiagsArea * diagsArea = NULL;
                ExRaiseSqlError(getHeap(), &diagsArea,
                                (ExeErrorCode)(8442), NULL, &intParam1,
                                &cliError, NULL, (char*)"ExpLOBInterfaceInsert",
                                getLobErrStr(intParam1));
                pentry_down->setDiagsArea(diagsArea);
                pstate.step_ = EXTRACT_ERROR;
                break;
              }
          }
          
        if (feStats)
        {
          feStats->incReadyToSendBuffersCount();
          feStats->incReadyToSendBytes(currBuffer_->bufSize_ - currBuffer_->bytesLeft_);
        }


        currBuffer_ = NULL;

        if (endOfData_)
        {
          pstate.step_ = EXTRACT_DONE;

        }
        else
        {
          pstate.step_ = EXTRACT_RETURN_ROWS_FROM_CHILD;
        }
      }
       break;

      case EXTRACT_ERROR:
      {
         // Cancel the child request - there must be a child request in
         // progress to get to the ERROR state.
         if (pstate.processingStarted_)
         {
           qChild_.down->cancelRequestWithParentIndex(qParent_.down->getHeadIndex());
         }

         // If there is no room in the parent queue for the reply,
         // try again later.
         //
         if (qParent_.up->isFull())
           return WORK_OK;

         ex_queue_entry *pentry_up = qParent_.up->getTailEntry();
         pentry_up->copyAtp(pentry_down);
         // Construct and return the error row.
         //
        if (workAtp_->getDiagsArea())
        {
          ComDiagsArea *diagsArea = pentry_up->getDiagsArea();
          if (diagsArea == NULL)
          {
            diagsArea = ComDiagsArea::allocate(getGlobals()->getDefaultHeap());
            pentry_up->setDiagsArea(diagsArea);
          }
          pentry_up->getDiagsArea()->mergeAfter(*workAtp_->getDiagsArea());
          workAtp_->setDiagsArea(NULL);
        }
         pentry_up->upState.status = ex_queue::Q_SQLERROR;
         pentry_up->upState.parentIndex
           = pentry_down->downState.parentIndex;
         pentry_up->upState.downIndex = qParent_.down->getHeadIndex();
         pentry_up->upState.setMatchNo(pstate.matchCount_);
         qParent_.up->insert();
         //
         ErrorOccured_ = TRUE;
         pstate.step_ = EXTRACT_DONE;
      }
       break;

      case EXTRACT_DONE:
      {
        if (isSequenceFile())
          {
            sfwRetCode = sequenceFileWriter_->close();
            if (sfwRetCode != SFW_OK)
              {
                createSequenceFileError(sfwRetCode);
  	    	      pstate.step_ = EXTRACT_ERROR;
  	    	      break;
              }
          }
        else
          {
            retcode = ExpLOBinterfaceCloseFile
                                    (lobGlob_,
                                        hdfsFileName_,
                                     NULL, //(char*)"",
                                     (Lng32)Lob_External_HDFS_File,
                                     hdfsHost_,
                                     hdfsPort_);
            if (! ErrorOccured_ && retcode < 0)
              {
                Lng32 cliError = 0;
    
                Lng32 intParam1 = -retcode;
                ComDiagsArea * diagsArea = NULL;
                ExRaiseSqlError(getHeap(), &diagsArea,
                                (ExeErrorCode)(8442), NULL, &intParam1,
                                &cliError, NULL,
                                (char*)"ExpLOBinterfaceCloseFile",
                                getLobErrStr(intParam1));
                pentry_down->setDiagsArea(diagsArea);
    
                pstate.step_ = EXTRACT_ERROR;
                break;
              }
          }
    
        //insertUpQueueEntry will insert Q_NO_DATA into the up queue and
        //remove the head of the down queue
        insertUpQueueEntry(ex_queue::Q_NO_DATA, NULL, TRUE);
        ErrorOccured_ = TRUE;

        endOfData_ = FALSE;

        //we need to set the next state so that the query can get re-executed
        //and we start from the beginning again. Not sure if pstate will be
        //valid anymore because insertUpQueueEntry() might have cleared it
        //already.
        pstate.step_ = EXTRACT_NOT_STARTED;

        //exit out now and not break.
        return WORK_OK;

      }
      break;

      default:
      {
        ex_assert(FALSE, "Invalid state in  ExHdfsFastExtractTcb ");
      }

      break;

    } // switch(pstate.step_)
  } // while

  return WORK_OK;
#endif
}//ExHdfsFastExtractTcb::work()

void ExHdfsFastExtractTcb::insertUpQueueEntry(ex_queue::up_status status, ComDiagsArea *diags, NABoolean popDownQueue)
{

  ex_queue_entry *upEntry = qParent_.up->getTailEntry();
  ex_queue_entry *downEntry = qParent_.down->getHeadEntry();
  ExFastExtractPrivateState &privateState = *((ExFastExtractPrivateState *) downEntry->pstate);

  // Initialize the up queue entry.
  //
  // copyAtp() will copy all tuple pointers and the diags area from
  // the down queue entry to the up queue entry.
  //
  // When we return Q_NO_DATA if the match count is > 0:
  // * assume down queue diags were returned with the Q_OK_MMORE entries
  // * release down queue diags before copyAtp()
  //
  if (status == ex_queue::Q_NO_DATA && privateState.matchCount_ > 0)
  {
    downEntry->setDiagsArea(NULL);
    upEntry->copyAtp(downEntry);
  }
  else
  {
    upEntry->copyAtp(downEntry);
    downEntry->setDiagsArea(NULL);
  }

  upEntry->upState.status = status;
  upEntry->upState.parentIndex = downEntry->downState.parentIndex;
  upEntry->upState.downIndex = qParent_.down->getHeadIndex();
  upEntry->upState.setMatchNo(privateState.matchCount_);

  // rows affected code (below) medeled after ex_partn_access.cpp
  ExMasterStmtGlobals *g = getGlobals()->castToExExeStmtGlobals()->castToExMasterStmtGlobals();
  if (!g)
  {
    ComDiagsArea *da = upEntry->getDiagsArea();
    if (da == NULL)
    {
      da = ComDiagsArea::allocate(getGlobals()->getDefaultHeap());
      upEntry->setDiagsArea(da);
    }
    da->addRowCount(privateState.matchCount_);
  }
  else
  {
    g->setRowsAffected(privateState.matchCount_);
  }


  //
  // Insert into up queue
  qParent_.up->insert();

  // Optionally remove the head of the down queue
  if (popDownQueue)
  {
    privateState.init();
    qParent_.down->removeHead();
  }
}

NABoolean ExHdfsFastExtractTcb::isSequenceFile()
{
  return myTdb().getIsSequenceFile();
}

void ExHdfsFastExtractTcb::createSequenceFileError(Int32 sfwRetCode)
{
#ifndef __EID 
  ComDiagsArea * diagsArea = NULL;
  char* errorMsg = sequenceFileWriter_->getErrorText((SFW_RetCode)sfwRetCode);
  ExRaiseSqlError(getHeap(), &diagsArea, (ExeErrorCode)(8447), NULL, 
  		  NULL, NULL, NULL, errorMsg, NULL);
  ex_queue_entry *pentry_down = qParent_.down->getHeadEntry();
  pentry_down->setDiagsArea(diagsArea);
#endif
}

ExFastExtractPrivateState::ExFastExtractPrivateState()
{
  init();
}

ExFastExtractPrivateState::~ExFastExtractPrivateState()
{
}
