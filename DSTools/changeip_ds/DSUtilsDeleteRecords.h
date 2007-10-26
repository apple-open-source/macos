/*
 *  DSUtilsDeleteRecords.h
 *  NeST
 *
 *  Created by admin on Mon Sep 29 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __DSUtilsDeleteRecords__
#define __DSUtilsDeleteRecords__

#include "DSUtils.h"

class DSUtilsDeleteRecords : public DSUtils
{
	public:
	
											DSUtilsDeleteRecords( bool removeDefaultIPRec );
		virtual								~DSUtilsDeleteRecords();
		
		virtual tDirStatus					DoActionOnCurrentNode( void );
		
		
	protected:
		
		bool mRemoveDefaultIPRec;
};
#endif


