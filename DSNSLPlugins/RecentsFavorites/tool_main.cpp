/*
 *  main.cpp
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Wed Feb 27 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */
#include <CoreServices/CoreServices.h>

#define kMaxArgs		3		// [-r <svc type>] | [-f <svc type>]

    #define min(A,B) (((A) < (B)) ? (A) : (B))
    #define max(A,B) (((A) > (B)) ? (A) : (B))

enum {
    flavorTypeURL			= FOUR_CHAR_CODE('url '),	/* URL data */
    flavorTypeURLName			= FOUR_CHAR_CODE('urln'),	/* URL name */
};

enum {
    flavorTypeStandardID		= 256
};


void PrintHelpInfo( void )
{
	fprintf( stderr,
		"Usage: recents_favorites [-r <svc type>] | [-f <svc type>]\n"
		"  -r <svc type> will return recent servers of type <svc type> \n"
		"  -f <svc type> will return favorite servers of type <svc type> \n" );
}

OSStatus GetRecFavFolderNames( void );
OSStatus GetFavoritesListFromFolder( char* serviceType, OSType folderType );
void MakeURLListNameFromURL( char* url, Str255& urlListName );

int main(int argc, char *argv[])
{
	char*		serviceType = NULL;
	UInt32		serviceTypeLen = 0;
	
	if ( argc > kMaxArgs || argc < 2 )
    {
        PrintHelpInfo();
        return -1;
    }
	
	for ( int i=1; i<argc; i++ )		// skip past [0]
	{
		if ( strcmp(argv[i], "-r") == 0 ) 
		{
            i++;
            if ( argv[i] && strlen(argv[i]) > 0 )
            {
                serviceTypeLen = strlen( argv[i] );

                serviceType = (char*)malloc(serviceTypeLen+1);
                strcpy( serviceType, argv[i] );
            }
                
            GetFavoritesListFromFolder( serviceType, kRecentServersFolderType );
        }
        else if ( strcmp(argv[i], "-f") == 0 ) 
		{
            i++;
            if ( argv[i] && strlen(argv[i]) > 0 )
            {
                serviceTypeLen = strlen( argv[i] );

                serviceType = (char*)malloc(serviceTypeLen+1);
                strcpy( serviceType, argv[i] );
            }
            
            GetFavoritesListFromFolder( serviceType, kFavoritesFolderType );
        }
        else if ( strcmp(argv[i], "-n") == 0 )
        {
            GetRecFavFolderNames();
        }
        
        if ( serviceType )
            free( serviceType );
    }
}

OSStatus GetRecFavFolderNames( void )
{
    short foundVRefNum;
    long foundDirID;
    OSStatus iErr, pbErr;
    char	recentsFolderName[256] = {0};
    char	favoritesFolderName[256] = {0};
            
    
    iErr = ::FindFolder(kUserDomain, kRecentServersFolderType, kCreateFolder, &foundVRefNum, &foundDirID );

    if ( iErr == noErr )
    {
        Str255			folderName;
        CInfoPBRec		pb;
        pb.dirInfo.ioNamePtr = (StringPtr)&folderName;

        pb.dirInfo.ioVRefNum = foundVRefNum;
        pb.dirInfo.ioFDirIndex = -1;	// get info about this directory
        pb.dirInfo.ioDrDirID = foundDirID;
        
        pbErr = ::PBGetCatInfoSync( &pb );
        
        if ( !pbErr )
        {
            memcpy( recentsFolderName, &folderName[1], folderName[0] );
        }
    }
    else
        fprintf( stderr, "*** got an error trying to find the Recents Folder! ***\n" );

    iErr = ::FindFolder(kUserDomain, kFavoritesFolderType, kCreateFolder, &foundVRefNum, &foundDirID );

    if ( iErr == noErr )
    {
        Str255			folderName;
        CInfoPBRec		pb;

        pb.dirInfo.ioNamePtr = (StringPtr)&folderName;

        pb.dirInfo.ioVRefNum = foundVRefNum;
        pb.dirInfo.ioFDirIndex = -1;	// get info about this directory
        pb.dirInfo.ioDrDirID = foundDirID;
        
        pbErr = ::PBGetCatInfoSync( &pb );
        
        if ( !pbErr )
        {
            memcpy( favoritesFolderName, &folderName[1], folderName[0] );
        }
    }
    else
        fprintf( stderr, "*** got an error trying to find the Favorites Folder! ***\n" );
    
    if ( !iErr )
    {
        fprintf( stderr, recentsFolderName );
        fprintf( stderr, "\n" );
        fprintf( stderr, favoritesFolderName );
        fprintf( stderr, "\n" );
    }
    
    return iErr;
}

OSStatus GetFavoritesListFromFolder( char* serviceType, OSType folderType )
{
    // basically empty our list if it has items,
    // then scan through the favorites folder for alias' with service types matching those in our shortcuts lsit
    // then create NavMenuItems and insert them into the favorites list
    short foundVRefNum, fRefNum;
    long foundDirID;
    OSStatus iErr, pbErr;
    
    iErr = ::FindFolder(kUserDomain, folderType, kCreateFolder, &foundVRefNum, &foundDirID );
    
    if ( iErr == noErr )
    {
        Str255			folderName;
        char			urlServiceType[64];
        char*			tempPtr;
        CInfoPBRec		pb;
        Handle			urlResHandle = NULL;
        Handle			urlNameResHandle = NULL;
        unsigned long	urlResHandleLen = 0;
        short			index = 1;
        Boolean			isDir, isAlias;

        pb.dirInfo.ioNamePtr = (StringPtr)&folderName;

        pb.dirInfo.ioVRefNum = foundVRefNum;
        pb.dirInfo.ioFDirIndex = -1;	// get info about this directory
        pb.dirInfo.ioDrDirID = foundDirID;
        
        pbErr = ::PBGetCatInfoSync( &pb );
        
        while ( (pbErr == noErr) )	// KA - 8/5/96
        {
            pb.dirInfo.ioVRefNum = foundVRefNum;
            pb.dirInfo.ioFDirIndex = index;	// get info about this item
            pb.dirInfo.ioDrDirID = foundDirID;
            
            pbErr = PBGetCatInfoSync( &pb );
            
            isDir = pb.hFileInfo.ioFlAttrib & ioDirMask;
            isAlias = ( (pb.hFileInfo.ioFlFndrInfo.fdFlags & 0x8000) != 0 );
            
            if ( (pbErr == noErr) && !isDir && !isAlias )
            {
                // check to see if this file has our required resource types
                fRefNum = ::HOpenResFile( pb.dirInfo.ioVRefNum, pb.hFileInfo.ioFlParID, pb.dirInfo.ioNamePtr, fsRdPerm );
                // //DBGLOG("file name is %s\n", pb.dirInfo.ioNamePtr);
                if ( fRefNum != -1 )
                {
                    urlResHandle = ::Get1Resource( flavorTypeURL, flavorTypeStandardID );
                    urlNameResHandle = ::Get1Resource( flavorTypeURLName, flavorTypeStandardID );
                    
                    if ( urlResHandle )
                    {
                        urlResHandleLen = ::GetHandleSize(urlResHandle);
                        
                        // just move this data over
                        ::BlockMove( *urlResHandle, urlServiceType, min(urlResHandleLen, sizeof(urlServiceType)) );
                        urlServiceType[sizeof(urlServiceType)] = '\0';				// null terminate for strstr op
                        
                        // as long as it has a url we can use it, don't worry about whether it has a name or not yet
                        tempPtr = strstr( urlServiceType, ":/" );			// get to the end of the servicetype
                        if ( tempPtr )
                        {
                            *tempPtr = '\0';			// now we terminate after the end of the serviceType
                            
                            char* itemName = NULL;
                            char* itemURL = (char*)::malloc( urlResHandleLen+1 );
                            
                            ::BlockMove( *urlResHandle, itemURL, urlResHandleLen );
                            itemURL[urlResHandleLen] = '\0';
                            
                            if ( urlNameResHandle )
                            {
                                long urlNameResHandleLen = ::GetHandleSize(urlNameResHandle);
                                
                                itemName = (char*)::malloc( urlNameResHandleLen+1 );
                                
                                ::BlockMove( *urlNameResHandle, itemName, urlNameResHandleLen );
                                itemName[urlNameResHandleLen] = '\0';
                            }

                            if ( !serviceType || strcmp( serviceType, urlServiceType ) == 0 )
                            {
                                if ( !itemName )
                                {
                                    // make an empty name string
                                    itemName = (char*)::malloc( 1 );
                                    *itemName = '\0';
                                }
                                
                                fprintf( stderr, "%s\t%s\n", itemName, itemURL );
                            }
                            
                            if ( itemURL )
                            {
                                ::free( itemURL );
                                itemURL = NULL;
                            }
                            
                            if ( itemName )
                            {
                                ::free( itemName );
                                itemName = NULL;
                            }
                        }
                    }
                    
                    ::CloseResFile(fRefNum);
                    iErr = ::ResError();
                }
            }

            index++;
        }
    }

    return iErr;
}

