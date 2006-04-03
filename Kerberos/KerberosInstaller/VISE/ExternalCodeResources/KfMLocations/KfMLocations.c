#ifndef __EXTERNCODEDEFINES__
#include "ExternCodeDefines.h"
#endif

//-----------------------------------------------------------------------------------------

pascal void main(InstallLocExternPBPtr ILInfo);
pascal void main(InstallLocExternPBPtr ILInfo)
{
	OSErr			tErr;
	FSSpec			tSpec;
	SInt16 theVRefNum;
	SInt32 theDirID, createdDirID;

	ILInfo->ILE_ResultVRefNum	= -1;
	ILInfo->ILE_ResultDirID		= -1;
	
	if (ILInfo->ILE_Version == kInstallLocExternPBVersion) {	// make sure there hasn't been a change to PB since this was compiled
		switch (ILInfo->ILE_FindFldrCode) {
			case 'OKfM':
				// for the Old Kerberos for Mac Files folder, first we must create it in the Temporary Items folder
				// then get FSSpec to inside it to pass back
				tErr = FindFolder(kSystemDomain, kTemporaryFolderType, kDontCreateFolder, &theVRefNum, &theDirID);
				tErr = DirCreate (theVRefNum, theDirID, "\pOld Kerberos for Mac Files", &createdDirID);
				if (tErr == dupFNErr)
					tErr = FSMakeFSSpec(theVRefNum, theDirID, "\p:Old Kerberos for Mac Files:foo.h", &tSpec);
				else 
					tErr = FSMakeFSSpec(theVRefNum, createdDirID, "\pfoo.h", &tSpec);
				break;
			case 'SLAu':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:System:Library:Authenticators:foo.h", &tSpec);
				break;
			case 'SLCF':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:System:Library:CFMSupport:foo.h", &tSpec);
				break;
			case 'SLCS':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:System:Library:CoreServices:foo.h", &tSpec);
				break;
			case 'SLFr':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:System:Library:Frameworks:foo.h", &tSpec);
				break;
			case 'LiPr':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:Library:Preferences:foo.h", &tSpec);
				break;
			case 'LiRe':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:Library:Receipts:foo.h", &tSpec);
				break;
			case 'Usr ':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:usr:foo.h", &tSpec);
				break;
			case 'UsrB':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:usr:bin:foo.h", &tSpec);
				break;
			case 'UsrI':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:usr:include:foo.h", &tSpec);
				break;
			case 'UsrL':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:usr:lib:foo.h", &tSpec);
				break;
			case 'RtTr':
				tErr = FSMakeFSSpec(ILInfo->ILE_DestVRefNum, fsRtDirID, "\p:private:var:root:.Trash:foo.h", &tSpec);
				break;
			default:
				tErr = dirNFErr;
				break;
		}
		
		if (tErr == fnfErr)
			tErr = noErr;

		if (tErr == noErr) {
			ILInfo->ILE_ResultVRefNum = tSpec.vRefNum;
			ILInfo->ILE_ResultDirID = tSpec.parID;
		}
			
		ILInfo->ILE_ResultError = tErr;
	}
}

//-----------------------------------------------------------------------------------------

