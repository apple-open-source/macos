#include "MachIPCInterface.h"
#include "ClassicProtocol.h"

std::list <Handle>		CCIClassicSupport::sDiffs;
CCIUInt32				CCIClassicSupport::sFirstSeqNo = 0;
CCIUInt32				CCIClassicSupport::sNextSeqNo = 0;

bool CCIClassicSupport::SaveDiffs ()
{
//	return true;
	return false;
}
		
void CCIClassicSupport::SaveOneDiff (
	Handle		inDiff)
{
	sDiffs.push_back (inDiff);
	sNextSeqNo++;
}

void CCIClassicSupport::RemoveLastDiff ()
{
	try {
		sDiffs.pop_back ();
		sNextSeqNo--;
	} catch (...) {
	}
}

void CCIClassicSupport::RemoveDiffsUpTo (
	CCIUInt32		inSeqNo)
{
	while (sFirstSeqNo < inSeqNo) {
		DisposeHandle (*(sDiffs.begin ()));
		sDiffs.pop_front ();
		sFirstSeqNo++;
	}
}

Handle CCIClassicSupport::GetAllDiffsSince (
	CCIUInt32	inSeqNo)
{
	CCIUInt32	size = sizeof (CCIUInt32);
	
	CCIUInt32	seqNo = sFirstSeqNo;
	for (list <Handle>::const_iterator i = sDiffs.begin ();
		i != sDiffs.end ();
		i++, seqNo++) {
		if (seqNo >= inSeqNo) {
			size += GetHandleSize (*i);
			size += 2 * sizeof (CCIUInt32);
		}
	}
	
	Handle	allDiffs = NewHandle (size);
	
	if (allDiffs == NULL) {
		CCIDebugThrow_ (CCIException (ccErrNoMem));
	}
	
	CCIUInt32 offset = 0;
	
	seqNo = sFirstSeqNo;
	for (list <Handle>::const_iterator i = sDiffs.begin ();
		i != sDiffs.end ();
		i++, seqNo++) {
		
		if (seqNo >= inSeqNo) {
			CCIUInt32		data = ccClassic_DiffCookie;
			
			BlockMoveData (&data, (*allDiffs) + offset, sizeof (data));
			offset += sizeof (data);
			
			data = seqNo;
			BlockMoveData (&data, (*allDiffs) + offset, sizeof (data));
			offset += sizeof (data);
			
			BlockMoveData (*(*i), (*allDiffs) + offset, GetHandleSize (*i));
			offset += GetHandleSize (*i);
		}
	}
	
	CCIUInt32	data = ccClassic_ResponseCookie;
	BlockMoveData (&data, (*allDiffs) + offset, sizeof (data));
	
	return allDiffs;
}
