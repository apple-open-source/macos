#ifndef __CDDA_TRACK_NAME_H__
#define __CDDA_TRACK_NAME_H__

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declaration
//
//	CDDATrackName is the base class for all databases used. It provides
//	localized variants of the artist, title, and track names, as well as a
//	possible separator string used for autodiskmount
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CDDATrackName
{
	
	private:
		// Disable copy constructors
		CDDATrackName ( CDDATrackName &src );
		void operator = ( CDDATrackName &src );
		
		CFBundleRef		fCFBundleRef;
		
	public:
		
		// Constructor
		CDDATrackName ( void );
		
		// Destructor
		virtual ~CDDATrackName ( void );		
		
		virtual SInt32			Init ( const char * bsdDevNode, const void * TOCData );
		virtual CFStringRef 	GetArtistName ( void );
		virtual CFStringRef 	GetAlbumName ( void );
		virtual CFStringRef 	GetSeparatorString ( void );
		virtual CFStringRef 	GetTrackName ( UInt8 trackNumber );
		
};

#endif	/* __CDDA_TRACK_NAME_H__ */