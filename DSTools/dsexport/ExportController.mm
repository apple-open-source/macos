/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 *  ExportController.m
 *  DSTools
 */

#import "ExportController.h"

#include <errno.h>
#include <sys/termios.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define kMinNumArgs					4
#define SizeQuantum                 64
#define kProxyAddressArg			"-a"
#define kProxyUserArg				"-u"
#define kProxyPassArg				"-p"
#define kRecordNamesArg				"-r"
#define kExcludeAttribute           "-e"
#define kExportNativeAttributes     "--N"
#define kRecordNamesSeparator		","

#define kHeaderDelimiterChars		"0x0A 0x5C 0x3A 0x2C"

@interface ExportController (PrivateMethods)

- (void)parseArgs:(const char**)argv numArgs:(int)argc;
- (void)showUsage;
- (void)writeStringToOutputFile:(NSString*)theString;
- (void)writeStringToTempFile:(NSString*)theString;
- (void)writeHeaderToOutputFile;
- (void)exportDataToTempFile;
- (void)copyTempFileToOutputFile;
- (NSString*)getSecretString:(NSString*)prompt;
    BOOL create_tempdir(char *temp_dirname_pattern);
@end


@implementation ExportController

// ----------------------------------------------------------------------------
// initializes the class with arguments
// ----------------------------------------------------------------------------

- (ExportController*)initWithArgs:(const char**)argv numArgs:(int)argc
{
	self = [super init];
    _returnAttributes =@kDSAttributesStandardAll;
    
    // create array of attributes that should not be exported
    _attributesToBeExcluded = [[NSMutableArray alloc] initWithObjects:@"dsAttrTypeStandard:AppleMetaNodeLocation",@"dsAttrTypeStandard:RecordType",@"dsAttrTypeNative:objectClass",nil];
	
    // parse arguments given to the tool
    [self parseArgs:argv numArgs:argc];
    //if something about the args was bad, then show the usage
	if( _showUsage )
	{
		[self showUsage];
		exit(0);
	}
    
    // create a secure temp directory
    char str[] = "/tmp/dsexportXXXXXX";
    
    if (!create_tempdir(str)){
        NSLog([NSString stringWithFormat:@"ERROR: DSExport: Failed to create temporary directory.(Error %d)",errno]);
        return nil;
    }
    
    NSMutableString* tempPath = [[NSMutableString alloc] initWithUTF8String:str];
    _tmpDir = [[NSString alloc] initWithUTF8String:str];
    
    // create the path to the temp file (with random number suffix
    int rand = random();
    [tempPath appendString:[NSString stringWithFormat:@"/exportfile%d",rand]];
    [tempPath appendString:@".tmp"];
    _tmpFilePath  = [tempPath copy];
    [tempPath release];
	
	return self;
}

- (void)export
{
    //if something about the args was bad, then show the usage
	if( _showUsage )
	{
		[self showUsage];
		return;
	}
	
    @try
    { 
        
		// First we create a session (locally or on a proxy machine)
        ODSession *mySession;
		NSError *error;
		
		if( _remoteNetAddress == nil ) {  
            //local direct connection = default session
            mySession = [ODSession defaultSession];
            
        } else{     
            //proxy connection
            //create an ODSession with proxy information
            NSDictionary *myOptions      = [[NSDictionary alloc] initWithObjectsAndKeys:    _remoteNetAddress,  ODSessionProxyAddress,
                                                                                                    _userName,          ODSessionProxyUsername,
                                                                                                    _userPass,          ODSessionProxyPassword,
                                                                                                        NULL ];
            mySession = [ODSession sessionWithOptions:myOptions error: &error];
        }   

        // check to see that the connection to the proxy worked and session creation worked
        if (mySession == nil) {
           
            //if not we treat the exception
            NSException *exception;
            if ([error code] == (int)eDSAuthFailed) {
                exception = [NSException exceptionWithName:@"DSExportException"
                                                    reason:[NSString stringWithFormat:@"Authentification failed when trying to establish proxy connection to '%@'.",_remoteNetAddress]  userInfo:[error userInfo]];
                
            } else if ([error code] == (int)eDSUnknownHost){
                exception = [NSException exceptionWithName:@"DSExportException"
                                                    reason:[NSString stringWithFormat:@"'%@' is an unknown host.",_remoteNetAddress]  userInfo:[error userInfo]];
                
            } else if ([error code] == (int)eDSIPUnreachable){
                exception = [NSException exceptionWithName:@"DSExportException"
                                                    reason:[NSString stringWithFormat:@"'%@' is unreachable. Make sure that port 625 is open on this machine.",_remoteNetAddress]  userInfo:[error userInfo]];
                
                
            } else {
                exception = [NSException exceptionWithName:@"DSExportException"
                                                    reason:[NSString stringWithFormat:@"Directory Services returned following error:%@",[error localizedDescription]]  userInfo:[error userInfo]];
                
            }
            
            @throw exception;
        }
        
        //if the session create went fine we make the node
        _node   = [ODNode nodeWithSession:  mySession
                                     name: _nodePath
                                    error: &error ];
        //We check to see that the node create did not provoke any errors
        if (_node == nil) {
            NSException *exception;
            if ([error code] == (int)eDSNodeNotFound) {
                exception = [NSException exceptionWithName:@"DSExportException"
                                                    reason:[NSString stringWithFormat:@"No node was found at the following path:'%@'",_nodePath]  userInfo:[error userInfo]];
                
            }else{
                exception = [NSException exceptionWithName:@"DSExportException"
                                                    reason:[NSString stringWithFormat:@"Directory Services returned following error:%@",[error localizedDescription]]  userInfo:[error userInfo]];
                
            }
            @throw exception;
        }
        
        // We check the record type to see if it has a prefix a 'dsRecTypeStandard' or 'dsRecTypeNative' prefix
        if ((![_recordType hasPrefix:@"dsRecTypeStandard:"])&&(![_recordType hasPrefix:@"dsRecTypeNative:"])){
            // if not we check to see if the node supports the recordtype prefixed by 'dsRecTypeStandard
            // in this case we add the prefix
            // if the not does not support such a prefix we default to the 'dsRecTypeNative' prefix
            NSMutableString* tempRecordType = [[NSMutableString alloc] initWithString:@"dsRecTypeStandard:"];
            [tempRecordType appendString:_recordType];
            NSArray* supportedRecordTypes = [[NSArray alloc] init];
            supportedRecordTypes = [_node supportedRecordTypes: nil]; 
            
            if ([supportedRecordTypes containsObject:tempRecordType]) {
                
                _recordType = [tempRecordType description];
                NSLog([NSString stringWithFormat:@"The specified record type did not contain a valid prefix, the specified type corresponds to a supported standard type. The tool will be using the following record type: %@",_recordType]);
                
            } else {
                
                [tempRecordType setString:@"dsRecTypeNative:"];
                [tempRecordType appendString:_recordType];
                _recordType = [tempRecordType description];
                NSLog([NSString stringWithFormat:@"The specified record type did not contain a valid prefix, the specified type does not corresponds to a supported standard type. The tool therfore defaulted to a native type: %@",_recordType]);
                
            }
            
        }
        
        //delete any temp file that was there before
        if( [[NSFileManager defaultManager] fileExistsAtPath:_tmpFilePath] )
            [[NSFileManager defaultManager] removeFileAtPath:_tmpFilePath handler:nil];
        
        //secure the temp file
        NSDictionary *myFileAttr = [[NSDictionary alloc] initWithObjectsAndKeys: [[NSNumber alloc] initWithInt:S_IRWXU], NSFilePosixPermissions,  NULL];
        
        //create the temp file
        if( ![[NSFileManager defaultManager] createFileAtPath:_tmpFilePath contents:[NSData data] attributes:myFileAttr] ){
        }
        
        //open the temp file for writing
        _tmpFile = [[NSFileHandle fileHandleForWritingAtPath:_tmpFilePath] retain];
        
        //make sure the temp file was created properly
        if( _tmpFile == nil ){
            NSException *exception = [NSException exceptionWithName:@"DSExportException"
                                                             reason:@"Failed to create the temp file"  userInfo:nil];
            @throw exception; 
        }
        
        
        //write data to the temp file
        [self exportDataToTempFile];
        
        // Close _tmpFile for writing
        [_tmpFile closeFile];
        
        //open the temp file for reading
        [_tmpFile release];
        _tmpFile = [[NSFileHandle fileHandleForReadingAtPath:_tmpFilePath] retain];
        
        //make sure we can read the temp file
        if( _tmpFile == nil ){
            NSException *exception = [NSException exceptionWithName:@"DSExportException"
                                                             reason:@"Failed to reopen the temporary file"  userInfo:nil];
            @throw exception;
        }
        
        if( _tmpFile != nil )
        {
            //delete any outputfile that was there before
            if( [[NSFileManager defaultManager] fileExistsAtPath:_filePath] )
                [[NSFileManager defaultManager] removeFileAtPath:_filePath handler:nil];
            
            //create the output file
            if( ![[NSFileManager defaultManager] createFileAtPath:_filePath contents:[NSData data] attributes:nil] ){
                NSException *exception = [NSException exceptionWithName:@"DSExportException"
                                                                 reason:@"Failed to create output file."  userInfo:nil];
                @throw exception;
            }
            
            //open the output file for writing
            _outputFile = [[NSFileHandle fileHandleForWritingAtPath:_filePath] retain];
            if( _outputFile == nil ){
                NSException *exception = [NSException exceptionWithName:@"DSExportException"
                                                                 reason:@"Failed to open the output file for writing."  userInfo:nil];
                @throw exception;
            }
            
            if (_outputFile != nil){
                //Write the header line
                [self writeHeaderToOutputFile];
                //copy data from temp file to output file
                [self copyTempFileToOutputFile];
                
                //close these files
                [_outputFile closeFile];
                [_tmpFile closeFile];
                
            }
        }
        
    }
    
    @catch (NSException *exception) 
    {
        NSLog(@"%@: %@", [exception name], [exception  reason]);
        
        // If an exception is caught we close the output file and then delete it
        if( _outputFile != nil )
        {
            [_outputFile closeFile];
            _outputFile = nil;
        }
        
        //delete the outputfile if it exists
        if( [[NSFileManager defaultManager] fileExistsAtPath:_filePath] )
            [[NSFileManager defaultManager] removeFileAtPath:_filePath handler:nil];
        
        
    }
    
    @finally {
        
        //delete the tempFile if it exists
        //if( [[NSFileManager defaultManager] fileExistsAtPath:_tmpFilePath] )
        // [[NSFileManager defaultManager] removeFileAtPath:_tmpFilePath handler:nil];
        if( [[NSFileManager defaultManager] directoryContentsAtPath:_tmpDir] != nil)
            [[NSFileManager defaultManager] removeFileAtPath:_tmpDir handler:nil];
    }
}

- (NSString*)filePath
{
	return _filePath;
}

- (NSString*)nodePath
{
	return _nodePath;
}

- (NSString*)remoteNetAddress
{
	return _remoteNetAddress;
}

- (NSString*)userName
{
	return _userName;
}

- (NSString*)userPass
{
	return _userPass;
}

- (NSArray*)recordsToExport
{
	return _recordsToExport;
}

@end

@implementation ExportController (PrivateMethods)

// ----------------------------------------------------------------------------
// Parses arguments
// ----------------------------------------------------------------------------

- (void)parseArgs:(const char**)argv numArgs:(int)argc
{
	int argIndex = 1;
    
	if( argc < kMinNumArgs )
	{
		_showUsage = YES;
		return;
	}
	_filePath = [[NSString alloc] initWithUTF8String:argv[argIndex++]];		//file path is first argument
	_nodePath = [[NSString alloc] initWithUTF8String:argv[argIndex++]];		//node path is 2nd argument
    _recordType = [[NSString alloc] initWithUTF8String:argv[argIndex++]];	//record type is 3rd argument
    
	
	BOOL isProxy = NO;
	for( int i = argIndex; !_showUsage && ( i < argc ); i++ )
	{
		if ( strcmp( argv[i], kProxyAddressArg) == 0)		// proxy address
		{
			isProxy = YES;
            
            if ( ++i >= argc)
				_showUsage = YES;
			else
				_remoteNetAddress = [[NSString alloc] initWithUTF8String:argv[i]];
		}
		else if ( strcmp( argv[i], kProxyUserArg) == 0)		// proxy user
		{
			isProxy = YES;
            
            if ( ++i >= argc)
				_showUsage = YES;
			else
				_userName = [[NSString alloc] initWithUTF8String:argv[i]];
		}
		else if ( strcmp( argv[i], kProxyPassArg) == 0)		// proxy password
		{
			isProxy = YES;
            
            if ( ++i >= argc)
				_showUsage = YES;
			else {
                _userPass = [[NSString alloc] initWithUTF8String:argv[i]];
                memset((void*)argv[i],0,strlen(argv[i])); // erase password to reduce exposure to sniffing
            }
            
		}
		else if ( strcmp( argv[i], kRecordNamesArg) == 0)	// record names
		{
            if ( ++i >= argc)
				_showUsage = YES;
            
			else
			{
				_exportAll = NO;
                NSString* recordNamesString = [NSString stringWithUTF8String:argv[i]];
				_recordsToExport = [[recordNamesString componentsSeparatedByString:@kRecordNamesSeparator] retain];
			}
		}
        else if (strcmp( argv[i],kExcludeAttribute) ==0)
        {
            if ( ++i >= argc)
				_showUsage = YES;
			else{
                NSString* attributeNamesString = [NSString stringWithUTF8String:argv[i]];
				[_attributesToBeExcluded addObject:attributeNamesString];
            }
            
        }
        else if (strcmp( argv[i],kExportNativeAttributes) ==0)
        {
            _returnAttributes = @kDSAttributesAll;
            
        }
	}
	
	if( isProxy )   //if any of the proxy options were supplied, make sure they all were
	{
		if( ( _remoteNetAddress == nil ) || ( _userName == nil )  ){
            _showUsage = YES;
        } else {
            if ( ( _userPass == nil )){
                // If the user specified remote address and username we prompt him or her for password
                _userPass = [[self getSecretString: [NSString stringWithFormat:@"%@'s password:", _userName]] retain];
            }
        }
			
	}
        
        if(  _recordType == nil  ){
            _showUsage = YES;
        }
        if (_recordsToExport == nil) {
            _exportAll = YES;
        }
}

// ----------------------------------------------------------------------------
// Usage statement
// ----------------------------------------------------------------------------
- (void)showUsage
{
    fprintf( stdout,"\ndsexport - A tool for exporting records from Open Directory\n\n"
             "Usage:  dsexport filePath DSNodePath recordType [options] [DS proxy]\n"
             "flags:\n"
             "   --N exports Native attributes, by default only standard attributes\n"
             "       are exported.\n"
             "options: (comma delimited lists)\n"
             "    -r <recordNames>\n"
             "    -e <attributesToBeExcluded>\n"
             "\n"
             "proxy:   (when using DS proxy password prompt provided if none specified)\n"
             "    -a <proxyAddress>\n"
             "    -u <proxyUsername>\n"
             "    -p <proxyPassword>\n"
             "\n");
}

// ----------------------------------------------------------------------------
// Copies temp file to output file
// ----------------------------------------------------------------------------
- (void)copyTempFileToOutputFile{
    
    [_outputFile writeData:[_tmpFile readDataToEndOfFile]];
    
}

// ----------------------------------------------------------------------------
// Writes string to output file
// ----------------------------------------------------------------------------
- (void)writeStringToOutputFile:(NSString*)theString
{
	[_outputFile writeData:[theString dataUsingEncoding:NSUTF8StringEncoding]];
}

// ----------------------------------------------------------------------------
// Writes string to temp file
// ----------------------------------------------------------------------------
- (void)writeStringToTempFile:(NSString*)theString
{
	[_tmpFile writeData:[theString dataUsingEncoding:NSUTF8StringEncoding]];
}

// ----------------------------------------------------------------------------
// Write header to output file 
// ----------------------------------------------------------------------------
- (void)writeHeaderToOutputFile{
    
    //write the character constants part of the header
	[self writeStringToOutputFile:@kHeaderDelimiterChars];
    
	//write the record type and number of attribute columns
	[self writeStringToOutputFile:[NSString stringWithFormat:@" %@ %d ", _recordType, [_attributesForHeader count]]];
	
	//now write the column headers
	[self writeStringToOutputFile:[_attributesForHeader componentsJoinedByString:@" "]];
	[self writeStringToOutputFile:@"\n"];
}

// ----------------------------------------------------------------------------
// Export node data to the temp file
// ----------------------------------------------------------------------------
- (void)exportDataToTempFile
{
	ODQuery         *search;
	NSError			*error;
    
    search  = [ODQuery queryWithNode: _node
                      forRecordTypes: _recordType
                           attribute: @kDSNAttrRecordName
                           matchType: kODMatchInsensitiveEqualTo
                         queryValues: _recordsToExport
                    returnAttributes: _returnAttributes
                      maximumResults: 0
                               error: nil ];
    
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    NSArray* recordsAttributesValues = nil;
    recordsAttributesValues = [search resultsAllowingPartial: NO error: nil];
    
    NSLog( @"Exporting %d records", [recordsAttributesValues count] );
    
    // In case Open Directory framework returns an error
    if (recordsAttributesValues == nil) {
        NSException *exception = [NSException exceptionWithName:@"DSExportException"
                                                         reason:[NSString stringWithFormat:@"Directory Services returned following error:%@",[error localizedDescription]]  userInfo:[error userInfo]];
        @throw exception;
    }
    
    // Here we have a mutable array in which we will build up the Header line by determining all the Attribute types and adding them to this array
    NSMutableArray* mutableHeaderAttributes = [[NSMutableArray alloc] init];
    // we store the attribute values in this array
    NSMutableArray* currentAttributes = [[NSMutableArray alloc] init];
    
    // For every record returned by the search we check to see if there are new attribute types and get the values for every attribute type
    ODRecord *record = nil;
    for( NSEnumerator* recordEnum = [recordsAttributesValues objectEnumerator]; record = [recordEnum nextObject]; record != nil )
    {
        NSDictionary* attrsValues = [record recordDetailsForAttributes: nil error: &error];
        
        // Are we reading the first record or not
        if ([mutableHeaderAttributes count] == 0) {
            // If so we allocate and create the array by getting all the keys from the attrValues dictionary
            [mutableHeaderAttributes addObjectsFromArray:[attrsValues allKeys]];
            // We filter out the attributes we don't want to export
            [mutableHeaderAttributes removeObjectsInArray:_attributesToBeExcluded];
        } else {
            // If not we search for new attribute types that aren't already in the attribute array
            // We get all the keys (attribute types) for this record
            [currentAttributes addObjectsFromArray:[attrsValues allKeys]];
            // We filter out the attributes we don't want to export
            [currentAttributes removeObjectsInArray:_attributesToBeExcluded];
            // We subtract all the attribute types that already exist in the mutableHeaderAttribute array
            [currentAttributes removeObjectsInArray:mutableHeaderAttributes];
            // We then add these new attribute types to the mutableHeaderAttribute array
            [mutableHeaderAttributes addObjectsFromArray:currentAttributes];
        }
        
        
        // Here we write out the values that we just found to the temp file
        NSMutableArray* mutableAttrValues = [NSMutableArray array];
        for( NSEnumerator* headerEnum = [mutableHeaderAttributes objectEnumerator]; NSString* headerAttr = [headerEnum nextObject]; )
        {
            NSString* attrValuesString = @"";
            NSArray* attrValues = [attrsValues objectForKey:headerAttr];
            
            if( attrValues != nil )
            {
                NSMutableArray* escapedAttrValues = [NSMutableArray array];
                for( NSEnumerator* valueEnum = [attrValues objectEnumerator]; NSString* value = [valueEnum nextObject]; )
                {
                    if( [value isKindOfClass:[NSString class]] )
                    {
                        //escape any commas
                        NSMutableString* mutableValue = [NSMutableString stringWithString:value];
                        [mutableValue replaceOccurrencesOfString:@"," withString:@"\\," options:0 range:NSMakeRange( 0, [mutableValue length] )];
                        [escapedAttrValues addObject:mutableValue];
                    }
                    else
                    {
                        if ([value isKindOfClass:[NSData class]]) {
                            NSLog( @"Skipping binary data attribute `%@` in record `%@`. ", headerAttr,  [record recordName] );
                        } else {
                            NSLog( @"Bad value in record %@ - %@ - %@", [record recordName], headerAttr, value );
                        }                    }
                }
                attrValuesString = [escapedAttrValues componentsJoinedByString:@","];
            }
            //escape any colons
            NSMutableString* mutableAttrValuesString = [NSMutableString stringWithString:attrValuesString];
            [mutableAttrValuesString replaceOccurrencesOfString:@":" withString:@"\\:" options:0 range:NSMakeRange( 0, [mutableAttrValuesString length] )];
            [mutableAttrValues addObject:mutableAttrValuesString];
        }
        
        NSString* recordString = [mutableAttrValues componentsJoinedByString:@":"];
        NSMutableString* mutableRecordString = [NSMutableString stringWithString:recordString];
        //escape any newlines
        [mutableRecordString replaceOccurrencesOfString:@"\n" withString:@"\\\n" options:0 range:NSMakeRange( 0, [mutableRecordString length] )];
        [mutableRecordString appendString:@"\n"];
        
        [self writeStringToTempFile:mutableRecordString];
    }
    _attributesForHeader = mutableHeaderAttributes;
    [pool release];
    
}
// ----------------------------------------------------------------------------
// Prompts user for password
// ----------------------------------------------------------------------------
- (NSString*)getSecretString:(NSString*)prompt
{
#ifdef _OS_VERSION_NEXTSTEP_
	struct sgttyb iobasic;
#else
	struct termios term;
#endif
	char ch, *buf;
	FILE *fp = NULL, *outfp;
	long omask;
	int echo, len, buflen;
	char* out;
	const char* promptCStr = NULL;
	NSString* returnString = nil;
	
	if( prompt == nil )
		prompt = @"";
    
	promptCStr = [prompt UTF8String];
    
	/*
	 * read and write to /dev/tty if possible and there is a prompt string;
	 * else read from stdin and write to stderr.
	 */
	if (prompt != NULL && promptCStr[0] != '\0')
	{
		fp = fopen("/dev/tty", "w+");
		outfp = fp;
	}
	if (fp == NULL)
	{
		outfp = stderr;
		fp = stdin;
	}
    
	/*
	 * note - blocking signals isn't necessarily the
	 * right thing, but we leave it for now.
	 */
	omask = sigblock(sigmask(SIGINT) | sigmask(SIGTSTP));
    
#ifdef _OS_VERSION_NEXTSTEP_
	ioctl(fileno(fp), TIOCGETP, &iobasic);
	echo = iobasic.sg_flags & ECHO;
#else
	tcgetattr(fileno(fp), &term);
	echo = term.c_lflag & ECHO;
#endif
    
	if (echo != 0)
	{
#ifdef _OS_VERSION_NEXTSTEP_
		iobasic.sg_flags &= ~ECHO;
		ioctl(fileno(fp), TIOCSETN, &iobasic);
#else
		term.c_lflag &= ~ECHO;
		tcsetattr(fileno(fp), (TCSAFLUSH | TCSASOFT), &term);
#endif
	}
    
	fputs(promptCStr, outfp);
	rewind(outfp);
    
	len = 0;
	buflen = SizeQuantum;
	buf = (char*)malloc(buflen);
    
	ch = getc(fp);
	while ((ch != EOF) && (ch != '\n'))
	{
		if (len >= buflen)
		{
			buflen += SizeQuantum;
			buf = (char*)realloc(buf, buflen);
		}
        
		buf[len++] = ch;
		ch = getc(fp);
	}
	
	write(fileno(outfp), "\n", 1);
    
	if (echo)
	{
#ifdef _OS_VERSION_NEXTSTEP_
		iobasic.sg_flags |= ECHO;
		ioctl(fileno(fp), TIOCSETN, &iobasic);
#else
		term.c_lflag |= ECHO;
		tcsetattr(fileno(fp), (TCSAFLUSH | TCSASOFT), &term);
#endif
	}
    
	sigsetmask(omask);
	if (fp != stdin) fclose(fp);
    
	out = (char*)calloc(len+1,sizeof(char));
	strncpy(out,buf,len);
	free(buf);
	returnString = [NSString stringWithUTF8String:out];
	free(out);
	
	return returnString;
}

// ----------------------------------------------------------------------------
// creates secure temp directory
// ----------------------------------------------------------------------------
BOOL create_tempdir(char *temp_dirname_pattern)
{
    mode_t old_mode;
    
    temp_dirname_pattern = mkdtemp(temp_dirname_pattern);
    
    return (temp_dirname_pattern != NULL);
}


@end
