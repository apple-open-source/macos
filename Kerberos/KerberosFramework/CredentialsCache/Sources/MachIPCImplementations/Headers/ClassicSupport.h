#pragma once

#include "CCache.MachIPC.h"

class CCIClassicSupport {
	public:
		static bool KeepDiffs ();
		
		static void SaveOneDiff (
			Handle		inDiff);
		
		static void RemoveLastDiff ();
		
		static void RemoveDiffsUpTo (
			CCIUInt32	inSeqNo);
		
		static Handle GetAllDiffsSince (
			CCIUInt32	inSeqNo);
                        
                static Handle FabricateDiffs ();
                
                static void RemoveAllDiffs ();
			
	private:
		static	std::list <Handle>		sDiffs;
		static	CCIUInt32			sFirstSeqNo;
		static	CCIUInt32			sNextSeqNo;
                static	bool				sKeepDiffs;
};
