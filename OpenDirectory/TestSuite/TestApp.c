#include <stdint.h>
#include <OpenDirectory/OpenDirectory.h>
#include <unistd.h>
#include <pthread.h>

uint32_t    gTestCase       = 0;
Boolean     gLogErrors      = FALSE;
Boolean     gVerbose        = FALSE;
char        *gLogPath       = NULL;
char        *gAdminAccount  = NULL;
char        *gAdminPassword = NULL;
char        *gProxyHost     = NULL;
char        *gProxyPassword = NULL;
char        *gProxyUser     = NULL;
uint32_t    gTestTimes      = 1;
uint32_t    gCopies         = 1;

void usage( char *argv[] )
{
    fprintf( stderr, "Usage:  TestAppCF -a adminPass -p adminPass [-v] [-e] [-l path] [-N runNumber]\n", argv[0] );
    fprintf( stderr, "                 [-c copies] [-t times] [-u proxyUser -P proxyPass -h proxyHost]\n" );
    fprintf( stderr, "             -v  verbose output\n" );
    fprintf( stderr, "             -e  output error log\n" );
    fprintf( stderr, "             -l  log path (default \"./Logs/<runNumber>/\"\n" );
    fprintf( stderr, "             -N  test run number\n" );
    fprintf( stderr, "             -c  number of copies to fork for stress testing\n" );
    fprintf( stderr, "             -t  number of times to run the test\n" );
    fprintf( stderr, "             -u  Proxy username\n" );
    fprintf( stderr, "             -P  Proxy username password\n" );
    fprintf( stderr, "             -h  Proxy host\n" );
    fprintf( stderr, "             -a  local admin account\n" );
    fprintf( stderr, "             -p  local admin password\n" );
}

void parseOptions( int argc, char *argv[] )
{
    int                 ch;
    
    gTestCase = (uint32_t) CFAbsoluteTimeGetCurrent();
    
    while ((ch = getopt(argc, argv, "vel:N:c:t:u:P:h:a:p:")) != -1)
    {
        switch (ch)
        {
            case 'v':
                gVerbose = TRUE;
                break;
            case 'e':
                gLogErrors = TRUE;
                break;
            case 'l':
                gLogPath = optarg;
                break;
            case 'N':
                gTestCase = strtol( optarg, NULL, 10 );
                break;
            case 'c':
                if( NULL != optarg )
                {
                    gCopies = strtol( optarg, NULL, 10 );
                    if( gCopies > 1024 )
                        gCopies = 1024;
                }
                else
                    usage( argv );
                break;
            case 't':
                if( NULL != optarg )
                {
                    gTestTimes = strtol( optarg, NULL, 10 );
                    if( 0 == gTestTimes )
                        gTestTimes = 1;
                }
                else
                    usage( argv );
                break;
            case 'u':   // proxy user
                gProxyUser = optarg;
                break;
            case 'P':   // proxy password
                gProxyPassword = optarg;
                break;
            case 'h':   // host
                gProxyHost = optarg;
                break;
            case 'a':   // admin
                gAdminAccount = optarg;
                break;
            case 'p':   // local admin password
                gAdminPassword = optarg;
                break;
            case '?':
            default:
                usage( argv );
                exit(1);
        }
    }
    
    if( NULL == gAdminAccount || NULL == gAdminPassword )
    {
        usage( argv );
        exit( 1 );
    }
    else if( NULL != gProxyHost || NULL != gProxyUser || NULL != gProxyHost  )
    {
        if( NULL == gProxyHost || NULL == gProxyUser || NULL == gProxyHost  )
        {
            usage( argv );
            exit( 1 );
        }
    }
}

void searchCallback( ODContextRef inContext, CFMutableArrayRef inResults, CFErrorRef inResultCode )
{
    if( NULL != inResults )
    {
        printf( "Got %d results\n", (int) CFArrayGetCount(inResults) );
    }
    else
    {
        printf( "Got no more results\n" );
    }
}

void *doTest( void *inData )
{
    ODSessionRef        cfRef               = NULL;
    ODContextRef        cfContext           = 0;
    ODQueryRef          cfQuery             = NULL;
    CFArrayRef          cfResults           = NULL;
    CFStringRef         cfProxyHost         = NULL;
    CFStringRef         cfProxyUser         = NULL;
    CFStringRef         cfProxyPassword     = NULL;
    int                 ii;
    CFErrorRef          cfError;
    ODNodeRef           cfNodeRef           = NULL;
    ODNodeRef           cfLocalNodeRef      = (ODNodeRef) inData;
    CFStringRef         cfRecordName        = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("TestCFFramework%d"), (int) pthread_self() );
    CFStringRef         cfGroupName         = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("TestGroup%d"), (int) pthread_self() );
    CFStringRef         cfAddRecordName     = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("TestAddRecord%d"), (int) pthread_self() );
    CFStringRef         cfAddRecordAlias    = CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR("TestAddRecordAlias%d"), (int) pthread_self() );
    
    if( NULL != gProxyHost )
    {
        cfProxyHost     = CFStringCreateWithCString( kCFAllocatorDefault, gProxyHost, kCFStringEncodingUTF8 );
        cfProxyUser     = CFStringCreateWithCString( kCFAllocatorDefault, gProxyUser, kCFStringEncodingUTF8 );
        cfProxyPassword = CFStringCreateWithCString( kCFAllocatorDefault, gProxyPassword, kCFStringEncodingUTF8 );
    }
    
    CFStringRef cfAdminAccount  = CFStringCreateWithCString( kCFAllocatorDefault, gAdminAccount, kCFStringEncodingUTF8 );
    CFStringRef cfAdminPassword = CFStringCreateWithCString( kCFAllocatorDefault, gAdminPassword, kCFStringEncodingUTF8 );
    
    for( ii = 0; ii < gTestTimes; ii++ )
    {
        // test all of the APIs
        if( NULL != gProxyHost )
        {
            CFMutableDictionaryRef  cfOptions   = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, 
                                                                             &kCFTypeDictionaryValueCallBacks );
            
            CFDictionarySetValue( cfOptions, kODSessionProxyAddress, cfProxyHost );
            CFDictionarySetValue( cfOptions, kODSessionProxyUsername, cfProxyUser );
            CFDictionarySetValue( cfOptions, kODSessionProxyPassword, cfProxyPassword );
            
            cfRef = ODSessionCreate( kCFAllocatorDefault, cfOptions, NULL );
            if( NULL != cfRef )
            {
                printf("ODSessionCreate (with options) proxy - PASS\n");
                
                CFRelease( cfRef );
                cfRef = NULL;
            }
            else
            {
                printf( "ODSessionCreate (with options) proxy - FAIL\n" );
            }
            
            CFRelease( cfOptions );
        }
        else
        {
            printf("ODSessionCreate (with options) proxy - SKIP\n");
        }
        
        cfRef = ODSessionCreate( kCFAllocatorDefault, NULL, NULL );
        if( NULL != cfRef )
        {
            printf( "ODSessionCreate - PASS\n" );

            CFRelease( cfRef );
            cfRef = NULL;
        }
        
        cfNodeRef = ODNodeCreateWithNodeType( kCFAllocatorDefault, kODSessionDefault, kODTypeAuthenticationSearchNode, NULL );
        if( NULL != cfNodeRef )
        {
            printf( "ODNodeCreate - PASS\n" );
            
            CFArrayRef  cfNodes = ODNodeCopySubnodeNames( cfNodeRef, NULL );
            if( NULL != cfNodes )
            {
                printf( "ODNodeCopySubnodeNames (Auth search node) - PASS\n" );
                
                CFArrayRef  cfUnreachable = ODNodeCopyUnreachableSubnodeNames( cfNodeRef, NULL );
                if ( cfUnreachable != NULL )
                {
                    printf( "ODNodeCopyUnreachableSubnodeNames - FAIL - returned %d\n", CFArrayGetCount(cfUnreachable) );
                    
                    CFRelease( cfUnreachable );
                    cfUnreachable = NULL;
                }
                else
                {
                    printf( "ODNodeCopyUnreachableSubnodeNames - PASS\n" );
                }
                
                CFRelease( cfNodes );
                cfNodes = NULL;
            }
            else
            {
                printf( "ODNodeCopySubnodeNames (Auth search node) - FAIL\n" );
            }
            
            CFRelease( cfNodeRef );
            cfNodeRef = NULL;
        }
        else
        {
            printf( "ODNodeCreate - FAIL\n" );
        }

        cfNodeRef = ODNodeCreateWithName( kCFAllocatorDefault, kODSessionDefault, CFSTR("/LDAPv3/od.apple.com"), &cfError );
        if( NULL != cfNodeRef )
        {
            printf( "ODNodeCreateWithName with /LDAPv3/od.apple.com - PASS\n" );
            
            ODNodeRef cfNodeRef2 = ODNodeCreateCopy( kCFAllocatorDefault, cfNodeRef, NULL );
            if( NULL != cfNodeRef2 )
            {
                printf( "ODNodeCreateCopy - PASS\n" );
                CFRelease( cfNodeRef2 );
                cfNodeRef2 = NULL;
            }
            else
            {
                printf( "ODNodeCreateCopy - FAIL\n" );
            }
            
            CFRelease( cfNodeRef );
            cfNodeRef = NULL;
        }
        else
        {
            printf( "ODNodeCreateWithName with /LDAPv3/od.apple.com - FAIL (%d)\n", CFErrorGetCode(cfError) );
            CFRelease( cfError );
        }
        
        cfLocalNodeRef = ODNodeCreateWithNodeType( kCFAllocatorDefault, kODSessionDefault, kODTypeLocalNode, NULL );
        if( NULL != cfLocalNodeRef )
        {
            CFTypeRef   cfValues[] = { cfAdminAccount };
            CFArrayRef cfSearchValue = CFArrayCreate( kCFAllocatorDefault, cfValues, 1, &kCFTypeArrayCallBacks );
            
            printf( "ODNodeCreateWithNodeType with kODTypeLocalNode - PASS\n" );
            
            cfQuery = ODQueryCreateWithNode( kCFAllocatorDefault, cfLocalNodeRef, CFSTR(kDSStdRecordTypeUsers), CFSTR(kDSNAttrRecordName), 
                                             kODMatchEqualTo, cfSearchValue, NULL, 0, NULL );
            
            if( NULL != cfQuery )
            {
                printf( "ODQueryCreateWithNode - PASS\n" );

                cfResults = ODQueryCopyResults( cfQuery, FALSE, NULL );
                
                if( NULL != cfResults )
                {
                    printf( "ODQueryCopyResults returned non-NULL - PASS\n" );
                    
                    if( CFArrayGetCount( cfResults ) != 0 ) 
                    {
                        printf( "ODQueryCopyResults returned results (%d) - PASS\n", CFArrayGetCount(cfResults) );
                    }
                    else
                    {
                        printf( "ODQueryCopyResults returned results - FAIL\n" );
                    }
                    
                    CFRelease( cfResults );
                    cfResults = NULL;
                }
                else
                {
                    printf( "ODQueryCopyResults returned non-NULL - FAIL\n" );
                }
                
                CFRelease( cfQuery );
                cfQuery = NULL;
            }
            
            ODRecordRef cfRecord = ODNodeCopyRecord( cfLocalNodeRef, CFSTR(kDSStdRecordTypeUsers), cfAdminAccount, NULL, NULL );
            if( NULL != cfRecord )
            {
                printf( "ODNodeCopyRecord - PASS\n" );

                CFDictionaryRef cfPolicy = ODRecordCopyPasswordPolicy( kCFAllocatorDefault, cfRecord, NULL );
                if( NULL != cfPolicy )
                {
                    printf( "ODNodeCopyUserPolicy - PASS\n" );
                    
                    CFRelease( cfPolicy );
                    cfPolicy = NULL;
                }
                else
                {
                    printf( "ODNodeCopyUserPolicy - FAIL\n" );
                }
                
                if( ODRecordVerifyPassword(cfRecord, cfAdminPassword, NULL) == TRUE )
                {
                    printf( "ODNodeVerifyCredentials - PASS\n" );
                }
                else
                {
                    printf( "ODNodeVerifyCredentials - FAIL\n" );
                }
                
                CFStringRef     cfAttribute = CFSTR(kDSAttributesAll);
                CFArrayRef      cfAttribs   = CFArrayCreate( kCFAllocatorDefault, (const void **) &cfAttribute, 1, &kCFTypeArrayCallBacks );
                CFDictionaryRef cfValues    = ODRecordCopyDetails( cfRecord, cfAttribs, NULL );
                if( NULL != cfValues )
                {
                    printf( "ODRecordCopyDetails (kDSAttributesAll) - non-NULL - PASS\n" );
                    
                    if( CFDictionaryGetCount(cfValues) != 0 )
                    {
                        printf( "ODRecordCopyDetails (kDSAttributesAll) - has values - PASS\n" );
                    }
                    else
                    {
                        printf( "ODRecordCopyDetails (kDSAttributesAll) - has values - FAIL\n" );
                    }
                    
                    CFRelease( cfValues );
                    cfValues = NULL;
                }
                else
                {
                    printf( "ODRecordCopyAllAttributes - FAIL\n" );
                }
                
                CFArrayRef cfAttribValues = ODRecordCopyValues( cfRecord, CFSTR(kDS1AttrNFSHomeDirectory), NULL );
                if( NULL != cfAttribValues )
                {
                    printf( "ODRecordCopyValues - PASS\n" );
                    
                    CFRelease( cfAttribValues );
                    cfAttribValues = NULL;
                }
                else
                {
                    printf( "ODRecordCopyAttribute - FAIL\n" );
                }
                
                ODRecordRef cfGroup = ODNodeCopyRecord( cfLocalNodeRef, CFSTR(kDSStdRecordTypeGroups), CFSTR("admin"), NULL, NULL );
                if( NULL != cfGroup )
                {
                    printf( "ODNodeCopyRecord (admin) - PASS\n" );
                    
                    if( ODRecordContainsMember( cfGroup, cfRecord, NULL ) == TRUE )
                    {
                        printf( "ODRecordContainsMember - PASS\n" );
                    }
                    else
                    {
                        printf( "ODRecordContainsMember - FAIL\n" );
                    }
                    
                    CFRelease( cfGroup );
                    cfGroup = NULL;
                }
                else
                {
                    printf( "ODNodeCopyRecord (admin) - FAIL\n" );
                }
                
#warning needs ODNodeAuthenticateExtended
//                dsStatus = ODNodeAuthenticateExtended( dsNodeRef, CFSTR(kDSStdRecordTypeUsers), CFSTR(), inAuthItems, 
//                                                                    &outAuthItems, &dsContext );
//                if( eDSNoErr == dsStatus )
//                {
//                    printf( "ODNodeAuthenticateExtended - PASS\n" );
//                }
//                else
//                {
//                    printf( "ODNodeAuthenticateExtended - FAIL\n" );
//                }
                
                if( ODRecordSetNodeCredentials(cfRecord, cfAdminAccount, cfAdminPassword, NULL) )
                {
                    printf( "ODRecordSetNodeCredentials - PASS\n" );
                }
                else
                {
                    printf( "ODRecordSetNodeCredentials - FAIL\n" );
                }                
            }
            else
            {
                printf( "ODNodeCopyRecord - FAIL\n" );
            }
            
            CFStringRef cfNodeName = ODNodeGetName( cfLocalNodeRef );
            if( NULL != cfNodeName )
            {
                printf( "ODNodeGetName - PASS\n" );
            }
            else
            {
                printf( "ODNodeGetName - FAIL\n" );
            }

            CFDictionaryRef cfNodeInfo = ODNodeCopyDetails( cfLocalNodeRef, NULL, NULL );
            if( NULL != cfNodeInfo )
            {
                printf( "ODNodeCopyDetails - PASS\n" );
                
                CFRelease( cfNodeInfo );
                cfNodeInfo = NULL;
            }
            else
            {
                printf( "ODNodeCopyDetails - FAIL\n" );
            }

            CFArrayRef cfRecTypes = ODNodeCopySupportedRecordTypes( cfLocalNodeRef, NULL );
            if( NULL != cfRecTypes )
            {
                printf( "ODNodeCopySupportedRecordTypes - PASS\n" );
                
                CFRelease( cfRecTypes );
                cfRecTypes = NULL;
            }
            else
            {
                printf( "ODNodeCopySupportedRecordTypes - FAIL\n" );
            }
            
            CFArrayRef cfAttribTypes = ODNodeCopySupportedAttributes( cfLocalNodeRef, NULL, NULL );
            if( NULL != cfAttribTypes )
            {
                printf( "ODNodeCopySupportedAttributes - PASS\n" );
                
                CFRelease( cfAttribTypes );
                cfAttribTypes = NULL;
            }
            else
            {
                printf( "ODNodeCopySupportedAttributes - FAIL\n" );
            }

            if( ODNodeSetCredentials(cfLocalNodeRef, CFSTR(kDSStdRecordTypeUsers), cfAdminAccount, cfAdminPassword, NULL) )
            {
                ODRecordRef    cfRecord = NULL;
                
                printf( "ODNodeSetCredentials - PASS\n" );
                
                cfRecord = ODNodeCreateRecord( cfLocalNodeRef, CFSTR(kDSStdRecordTypeUsers), cfRecordName, NULL, NULL );
                if( NULL != cfRecord )
                {
                    printf( "ODNodeCreateRecord - PASS\n" );
                    
                    ODRecordRef cfRecordTemp = ODNodeCreateRecord( cfLocalNodeRef, CFSTR(kDSStdRecordTypeUsers), cfRecordName, NULL, &cfError );
                    if( NULL != cfRecordTemp )
                    {
                        printf( "ODNodeCreateRecord (create duplicate) - FAIL record returned\n" );
                    }
                    else
                    {
                        CFIndex errCode = CFErrorGetCode( cfError );
                        if( errCode == -14135 )
                        {
                            printf( "ODNodeCreateRecord (create duplicate) - PASS\n" );
                        }
                        else
                        {
                            printf( "ODNodeCreateRecord (create duplicate) - FAIL (%d)\n", errCode );
                            CFRelease( cfError );
                        }
                    }
                    
                    if( ODRecordDelete(cfRecord, &cfError) )
                    {
                        printf( "ODRecordDelete - PASS\n" );
                    }
                    else
                    {
                        printf( "ODRecordDelete - FAIL (%d)\n", CFErrorGetCode(cfError) );
                    }
                    
                    CFRelease( cfRecord );
                    cfRecord = NULL;
                }
                else
                {
                    printf( "ODNodeCreateRecord - FAIL\n" );
                }
                
                CFMutableDictionaryRef cfAttributes = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, 
                                                                                 &kCFTypeDictionaryValueCallBacks );
                CFStringRef     cfHome[]    = { CFSTR("/Users/blah"), NULL };
                CFStringRef     cfNames[]   = { cfAddRecordName, cfAddRecordAlias, NULL };
                
                CFArrayRef      cfHomeAttrib    = CFArrayCreate( kCFAllocatorDefault, (const void **) &cfHome, 1, &kCFTypeArrayCallBacks );
                CFArrayRef      cfNamesAttrib   = CFArrayCreate( kCFAllocatorDefault, (const void **) &cfNames, 2, &kCFTypeArrayCallBacks );

                CFDictionarySetValue( cfAttributes, CFSTR(kDS1AttrNFSHomeDirectory), cfHomeAttrib );
                CFDictionarySetValue( cfAttributes, CFSTR(kDSNAttrRecordName), cfNamesAttrib );
                
                CFRelease( cfNamesAttrib );
                CFRelease( cfHomeAttrib );
                
                cfRecord = ODNodeCreateRecord( cfLocalNodeRef, CFSTR(kDSStdRecordTypeUsers), cfAddRecordName, cfAttributes, &cfError );
                if( NULL != cfRecord )
                {
                    printf( "ODNodeCreateRecord (with attributes) - PASS\n" );
                    
                    if( NULL != cfRecord )
                    {
                        printf( "ODNodeAddRecordWithAttributes (record returned) - PASS\n" );
                        
                        CFArrayRef cfValue = ODRecordCopyValues( cfRecord, CFSTR(kDS1AttrNFSHomeDirectory), NULL );
                        if( NULL != cfValue && CFArrayGetCount(cfValue) == 1 )
                        {
                            printf( "ODNodeAddRecordWithAttributes (value exist) - PASS\n" );
                        }
                        else
                        {
                            printf( "ODNodeAddRecordWithAttributes (value exist) - FAIL\n" );
                        }
                        
                        if( NULL != cfValue )
                            CFRelease( cfValue );
                        
                        if( ODRecordChangePassword(cfRecord, NULL, CFSTR("test"), &cfError) )
                        {
                            printf( "ODNodeSetPassword - PASS\n" );
                        }
                        else
                        {
                            printf( "ODNodeSetPassword - FAIL (%d)\n", CFErrorGetCode(cfError) );
                            CFRelease( cfError );
                        }
                        
                        if( ODRecordChangePassword(cfRecord, CFSTR("test"), CFSTR("test2"), &cfError) )
                        {
                            printf( "ODNodeChangePassword - PASS\n" );
                        }
                        else
                        {
                            printf( "ODNodeChangePassword - FAIL (%d)\n", CFErrorGetCode(cfError) );
                            CFRelease( cfError );
                        }
                        
                        CFStringRef cfNewValue  = CFSTR("Test street");
                        CFArrayRef  cfNewValues = CFArrayCreate( kCFAllocatorDefault, (const void **) &cfNewValue, 1, &kCFTypeArrayCallBacks );
                        
                        if( ODRecordSetValues( cfRecord, CFSTR(kDSNAttrState), cfNewValues, &cfError ) )
                        {
                            printf( "ODRecordSetValueOrValues - PASS\n" );
                        }
                        else
                        {
                            printf( "ODRecordSetValueOrValues - FAIL (%d)\n", CFErrorGetCode(cfError) );
                            CFRelease( cfError );
                        }
                        
                        CFRelease( cfNewValues );
                        cfNewValues = NULL;
                        
                        if( ODRecordAddValue( cfRecord, CFSTR(kDSNAttrState), CFSTR("Test street 2"), &cfError ) )
                        {
                            printf( "ODRecordAddValue - PASS\n" );
                        }
                        else
                        {
                            printf( "ODRecordAddValue - FAIL (%d)\n", CFErrorGetCode(cfError) );
                            CFRelease( cfError );
                        }
                        
                        if( ODRecordRemoveValue(cfRecord, CFSTR(kDSNAttrState), CFSTR("Test street 2"), &cfError) )
                        {
                            printf( "ODRecordRemoveValue - PASS\n" );
                        }
                        else
                        {
                            printf( "ODRecordRemoveValue - FAIL (%d)\n", CFErrorGetCode(cfError) );
                        }
                        
                        CFArrayRef  cfArray = CFArrayCreate( kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks );
                        if( ODRecordSetValues(cfRecord, CFSTR(kDSNAttrState), cfArray, &cfError) )
                        {
                            printf( "ODRecordSetValues (remove attribute) - PASS\n" );
                            
                            CFArrayRef cfValues = ODRecordCopyValues( cfRecord, CFSTR(kDSNAttrState), NULL );
                            if( NULL == cfValues || CFArrayGetCount(cfValues) == 0 )
                            {
                                printf( "ODRecordSetValues (attrib gone) - PASS\n" );
                            }
                            else
                            {
                                printf( "ODRecordSetValues (attrib gone) - FAIL\n" );
                            }
                            
                            if( NULL != cfValues )
                                CFRelease( cfValues );
                        }
                        else
                        {
                            printf( "ODRecordSetValues (deleting attrib) - FAIL (%d)\n", CFErrorGetCode(cfError) );
                        }
                        
                        CFRelease( cfArray );
                        
                        // Try adding this record to group admin
                        ODRecordRef    cfGroupRef = NULL;
                        
                        cfGroupRef = ODNodeCreateRecord( cfLocalNodeRef, CFSTR(kDSStdRecordTypeGroups), cfGroupName, NULL, NULL );
                        if( NULL != cfGroupRef )
                        {
                            if( ODRecordAddMember( cfGroupRef, cfRecord, &cfError ) )
                            {
                                printf( "ODRecordAddMember - PASS\n" );
                                
                                if( ODRecordRemoveMember(cfGroupRef, cfRecord, &cfError) )
                                {
                                    printf( "ODRecordRemoveMember - PASS\n" );
                                }
                                else
                                {
                                    printf( "ODRecordRemoveMember - FAIL (%d)\n", CFErrorGetCode(cfError) );
                                    CFRelease( cfError );
                                }
                            }
                            else
                            {
                                printf( "ODRecordAddRecordToGroup - FAIL (%d)\n", CFErrorGetCode(cfError) );
                                CFRelease( cfError );
                            }
                            
                            ODRecordDelete( cfGroupRef, NULL );
                            CFRelease( cfGroupRef );
                            cfGroupRef = NULL;
                        }
                        
                        ODRecordDelete( cfRecord, NULL );
                    }
                    else
                    {
                        printf( "ODNodeAddRecordWithAttributes (record returned) - FAIL\n" );
                    }
                    
                    CFRelease( cfRecord );
                    cfRecord = NULL;
                }
                else
                {
                    printf( "ODNodeCreateRecord (with attributes) - FAIL (%d)\n", CFErrorGetCode(cfError) );
                    CFRelease( cfError );
                }
                
                CFRelease( cfAttributes );
                cfAttributes = NULL;
            }
            else
            {
                printf( "ODNodeSetCredentials - FAIL\n" );
            }
            
            CFRelease( cfLocalNodeRef );
            cfLocalNodeRef = NULL;
        }
        else
        {
            printf( "ODNodeCreateWithNodeType with kODTypeLocalNode - FAIL\n" );
        }
    }
    
    CFRelease( cfRecordName );
    CFRelease( cfGroupName );
    CFRelease( cfAddRecordName );
    CFRelease( cfAddRecordAlias );
    
    return NULL;
}

int main( int argc, char *argv[] )
{
    parseOptions( argc, argv );
    
    argc -= optind;
    argv += optind;    
    
    ODNodeRef   cfLocalNodeRef = ODNodeCreateWithNodeType( kCFAllocatorDefault, kODSessionDefault, kODTypeLocalNode, NULL );

    if ( gCopies > 0 ) {
        
        int         ii;
        void        *junk;
        pthread_t   *threads    = (pthread_t *) calloc( gCopies, sizeof(pthread_t) );
        
        
        for ( ii = 0; ii < gCopies; ii++ )
        {
            while( pthread_create( &threads[ii], NULL, doTest, cfLocalNodeRef ) != 0 )
            {
                usleep( 1000 );
            }
        }
        
        // now wait for each thread to finish
        for ( ii = 0; ii < gCopies; ii++ ) {
            pthread_join( threads[ii], &junk );
        }
        
    } else {
        doTest( cfLocalNodeRef );
    }
    
    return 0;
}
