/* -*-C++-*- */
#ifndef ELEMDDLUUDFPARAMDEF_H
#define ELEMDDLUUDFPARAMDEF_H
/* -*-C++-*-
******************************************************************************
*
* File:         ElemDDLUudfParamDef.h
* Description:  class for universal function parameter kind (parse node)
*               elements in DDL statements
*
*
* Created:      1/20/2010
* Language:     C++
*
*
// @@@ START COPYRIGHT @@@
//
// (C) Copyright 2010-2014 Hewlett-Packard Development Company, L.P.
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
*
*
******************************************************************************
*/

#include "ElemDDLNode.h"

class ElemDDLUudfParamDef : public ElemDDLNode
{

public:

  // constructor
  ElemDDLUudfParamDef(ComUudfParamKind uudfParamKind);

  // virtual destructor
  virtual ~ElemDDLUudfParamDef(void);

  // cast
  virtual ElemDDLUudfParamDef * castToElemDDLUudfParamDef(void);

  // accessor
  inline const ComUudfParamKind getUudfParamKind(void) const
  {
    return uudfParamKind_;
  }

  //
  // methods for tracing
  //

  virtual NATraceList getDetailInfo() const;
  virtual const NAString getText() const;

private:

  ComUudfParamKind uudfParamKind_;

}; // class ElemDDLUudfParamDef

#endif /* ELEMDDLUUDFPARAMDEF_H */
