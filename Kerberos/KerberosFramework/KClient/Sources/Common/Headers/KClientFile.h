#ifndef KClientFile_h_
#define KClientFile_h_

#pragma once

class KClientFilePriv {
	public:
	
							KClientFilePriv ();
							KClientFilePriv (
								const FSSpec&			inFileSpec);
							KClientFilePriv (
								const char*				inFilePath);
		
		KClientFilePriv&	operator = (
								const KClientFilePriv&	inOriginal);
		KClientFilePriv&	operator = (
								const FSSpec&			inFileSpec);
		KClientFilePriv&	operator = (
								const char*				inFilePath);
	
		Boolean				IsValid () const;
								
                            operator const FSSpec& () const;
                            operator const char * () const;

	private:
        FSSpec		mFileSpec;
        char        mFilePath[PATH_MAX];	// Used by operator const char *& () 
		Boolean		mValid;
							
		void				CheckValid () const;								
};

#endif /* KClientFile_h_ */