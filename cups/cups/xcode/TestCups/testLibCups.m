//
//  TestLibCups.m
//  TestCups
//
//  Copyright © 2024 Apple Inc. All rights reserved.
//

#import <XCTest/XCTest.h>

@interface TestLibCups : XCTestCase

@end

void testFile(int withCompression);

@implementation TestLibCups

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testArray
{
  
  // Creating a sorted array of c-strings
  void *data  = (void *)"testarray";
  cups_array_t *array = cupsArrayNew((cups_array_func_t)strcmp, data);
  XCTAssert(array);
  XCTAssert(cupsArrayUserData(array) == data);

  // Adding to array
  XCTAssert(cupsArrayAdd(array, strdup("One Fish")));
  XCTAssert(cupsArrayAdd(array, strdup("Two Fish")));
  XCTAssert(cupsArrayAdd(array, strdup("Red Fish")));
  XCTAssert(cupsArrayAdd(array, strdup("Blue Fish")));
  
  int arrayCount = cupsArrayCount(array);
  XCTAssert(arrayCount == 4);

  // Confirm first, next, last, and prev work for the sorted array
  const char *firstItem = (const char *)cupsArrayFirst(array);
  XCTAssert(firstItem);
  XCTAssert(!strcmp(firstItem, "Blue Fish"));
  
  const char *nextItem = (const char *)cupsArrayNext(array);
  XCTAssert(nextItem);
  XCTAssert(!strcmp(nextItem, "One Fish"));
  
  const char *lastItem = (const char *)cupsArrayLast(array);
  XCTAssert(lastItem);
  XCTAssert(!strcmp(lastItem, "Two Fish"));
  
  const char *prevItem = (const char *)cupsArrayPrev(array);
  XCTAssert(prevItem);
  XCTAssert(!strcmp(prevItem, "Red Fish"));
  
  // Can we find items and update the current item?
  const char *foundItem = (const char *)cupsArrayFind(array, (void *)"One Fish");
  XCTAssert(foundItem);
  XCTAssert(!strcmp(foundItem, "One Fish"));

  const char *currentItem = (char *)cupsArrayCurrent(array);
  XCTAssert(currentItem);
  XCTAssert(!strcmp(currentItem, "One Fish"));

  // Duplicate the array
  {
    cups_array_t *dup_array = cupsArrayDup(array);
    XCTAssert(array);
    XCTAssert(cupsArrayCount(dup_array) == arrayCount);
    cupsArrayDelete(dup_array);
  }

  // Test removal and clear
  cupsArrayRemove(array, (void *)"One Fish");
  XCTAssert(cupsArrayCount(array) == arrayCount - 1);

  cupsArrayClear(array);
  XCTAssert(cupsArrayCount(array) == 0);

  cupsArrayDelete(array);
}

- (void)testPPD
{
  const char *ppdfile = "/System/Library/Frameworks/ApplicationServices.framework/Frameworks/PrintCore.framework/Resources/Generic.ppd";
  
  ppd_file_t *ppd = ppdOpenFile(ppdfile);
  XCTAssert(ppd);

  // Can we read an attribute?
  static const char *kModelName = "ModelName";
  static const char *kModel = "Generic PostScript Printer";
  ppd_attr_t  *attr = ppdFindAttr(ppd, kModelName, NULL);
  XCTAssert(attr);
  XCTAssert(!strcmp(attr->name, kModelName));
  XCTAssert(!strcmp(attr->value, kModel));

  // Can we get the enumerate the choices for an option?
  static const char *kDuplexKeyword = "Duplex";
  static const char *kDuplexNone = "None";
  static const char *kDuplexNoTumble = "DuplexNoTumble";
  static const char *kDuplexTumble = "DuplexTumble";
  
  ppd_option_t *duplexOption = ppdFindOption(ppd, kDuplexKeyword);
  XCTAssert(kDuplexKeyword);
  XCTAssert(!strcmp(duplexOption->keyword, kDuplexKeyword));
  XCTAssert(duplexOption->num_choices == 3);
  XCTAssert(!strcmp(duplexOption->choices[0].choice, kDuplexNone));
  XCTAssert(!strcmp(duplexOption->choices[1].choice, kDuplexNoTumble));
  XCTAssert(!strcmp(duplexOption->choices[2].choice, kDuplexTumble));

  // Marking options
  ppdMarkDefaults(ppd);
  
  ppd_choice_t *duplexChoice = ppdFindMarkedChoice(ppd, kDuplexKeyword);
  XCTAssert(duplexChoice);
  XCTAssert(duplexChoice->marked);
  XCTAssert(!strcmp(duplexChoice->choice, kDuplexNone));
  
  static const char *kPageSizeKeyword = "PageSize";
  static const char *kPageSizeLetter = "Letter";
  ppd_choice_t *pageSizeChoice = ppdFindMarkedChoice(ppd, kPageSizeKeyword);
  XCTAssert(pageSizeChoice);
  XCTAssert(pageSizeChoice->marked);
  XCTAssert(!strcmp(pageSizeChoice->choice, kPageSizeLetter));

  ppdClose(ppd);
}

void testFile(int withCompression)
{
  static const char *kTestFile = "testfile.dat.gz";
  static const ssize_t kRecordSize = 1024;
  cups_file_t *fp;
  ssize_t  totalByteWritten;
  ssize_t recordStartPos;
  char *openFlags;
  int recordReadDirection;
  int recordStartIndex;
  int recordEndIndex;
  
  // We need to read the compressed file in the forward direction
  if (withCompression) {
    openFlags = "w9";
    recordStartIndex = 0;
    recordEndIndex = 9999;
    recordReadDirection = 1;

  // We can test backward seeks with the uncompressed file
  } else {
    openFlags = "w";
    recordStartIndex = 9999;
    recordEndIndex = 0;
    recordReadDirection = -1;
  }
    
  // Can we open the file?
  fp = cupsFileOpen(kTestFile, openFlags);
  XCTAssert(fp);

  // Confirm the stream is setup for compression.
  XCTAssert(cupsFileCompression(fp) == withCompression);

  // Simple write tests for cupsFilePuts(), cupsFilePrintf(), cupsFilePutChar(),
  // cupsFileWrite(), and cupsFilePuts().
  static const char kHello[] = "# Hello, World\n";
  int bytesWritten = cupsFilePuts(fp, kHello);
  XCTAssert(bytesWritten == sizeof(kHello) - 1);
  totalByteWritten = bytesWritten;
  
  for (int i = 0; i < 1000; i ++) {
    static const char kTestLine[] = "TestLine %03d\n";
    bytesWritten = cupsFilePrintf(fp, kTestLine, i);
    XCTAssert(bytesWritten == sizeof(kTestLine) - 2);
    totalByteWritten += bytesWritten;
  }
  
  for (int i = 0; i < 256; i ++) {
    int success = !cupsFilePutChar(fp, i);
    XCTAssert(success);
    totalByteWritten += 1;
  }
  
  unsigned char  writebuf[kRecordSize];
  arc4random_buf(writebuf, sizeof(writebuf));
  recordStartPos = totalByteWritten;
  for (int i = 0; i < 10000; i ++) {
    bytesWritten = cupsFileWrite(fp, (char *)writebuf, sizeof(writebuf));
    XCTAssert(bytesWritten == sizeof(writebuf));
    totalByteWritten += bytesWritten;
  }

  static const char partialLine[] = "partial line";
  bytesWritten = cupsFilePuts(fp, partialLine);
  XCTAssert(bytesWritten == sizeof(partialLine) - 1);
  totalByteWritten += bytesWritten;

  // Is the file length correct?
  off_t length = cupsFileTell(fp);
  XCTAssert(length == totalByteWritten);

  XCTAssert(!cupsFileClose(fp));
  
  // Read tests
  cups_file_t *readFP = cupsFileOpen(kTestFile, "r");
  XCTAssert(readFP);
  
  char readbuf[kRecordSize];
  for (int i = recordStartIndex; i < recordEndIndex; i += recordReadDirection) {
    off_t pos = recordStartPos + i * kRecordSize;
    off_t actualPos = cupsFileSeek(readFP, pos);
    XCTAssert(pos == actualPos);
    ssize_t bytesRead = cupsFileRead(readFP, readbuf, sizeof(readbuf));
    XCTAssert(bytesRead == sizeof(readbuf));
    XCTAssert(!memcmp(readbuf, writebuf, sizeof(readbuf)));
  }
  
  XCTAssert(!cupsFileClose(readFP));

  // Remove the test file...
 unlink(kTestFile);
}

- (void)testFile
{
  testFile(0);  // test without compression
  testFile(1);  // test with compression
}

typedef struct uri_test_s    /**** URI test cases ****/
{
  http_uri_status_t  result;    /* Expected return value */
  const char    *uri,    /* URI */
      *scheme,  /* Scheme string */
      *username,  /* Username:password string */
      *hostname,  /* Hostname string */
      *resource;  /* Resource string */
  int      port,    /* Port number */
      assemble_port;  /* Port number for httpAssembleURI() */
  http_uri_coding_t  assemble_coding;/* Coding for httpAssembleURI() */
} uri_test_t;


static uri_test_t  uri_tests[] =  /* URI test data */
      {
        /* Start with valid URIs */
        { HTTP_URI_STATUS_OK, "file:/filename",
          "file", "", "", "/filename", 0, 0,
          HTTP_URI_CODING_MOST },
        { HTTP_URI_STATUS_OK, "file:/filename%20with%20spaces",
          "file", "", "", "/filename with spaces", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "file:///filename",
          "file", "", "", "/filename", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "file:///filename%20with%20spaces",
          "file", "", "", "/filename with spaces", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "file://localhost/filename",
          "file", "", "localhost", "/filename", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "file://localhost/filename%20with%20spaces",
          "file", "", "localhost", "/filename with spaces", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "http://server/",
          "http", "", "server", "/", 80, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "http://username@server/",
          "http", "username", "server", "/", 80, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "http://username:passwor%64@server/",
          "http", "username:password", "server", "/", 80, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "http://username:passwor%64@server:8080/",
          "http", "username:password", "server", "/", 8080, 8080,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "http://username:passwor%64@server:8080/directory/filename",
          "http", "username:password", "server", "/directory/filename", 8080, 8080,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "http://[2000::10:100]:631/ipp",
          "http", "", "2000::10:100", "/ipp", 631, 631,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "https://username:passwor%64@server/directory/filename",
          "https", "username:password", "server", "/directory/filename", 443, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "ipp://username:passwor%64@[::1]/ipp",
          "ipp", "username:password", "::1", "/ipp", 631, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "lpd://server/queue?reserve=yes",
          "lpd", "", "server", "/queue?reserve=yes", 515, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "mailto:user@domain.com",
          "mailto", "", "", "user@domain.com", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "socket://server/",
          "socket", "", "server", "/", 9100, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "socket://192.168.1.1:9101/",
          "socket", "", "192.168.1.1", "/", 9101, 9101,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "tel:8005551212",
          "tel", "", "", "8005551212", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "ipp://username:password@[v1.fe80::200:1234:5678:9abc+eth0]:999/ipp",
          "ipp", "username:password", "fe80::200:1234:5678:9abc%eth0", "/ipp", 999, 999,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "ipp://username:password@[fe80::200:1234:5678:9abc%25eth0]:999/ipp",
          "ipp", "username:password", "fe80::200:1234:5678:9abc%eth0", "/ipp", 999, 999,
          (http_uri_coding_t)(HTTP_URI_CODING_MOST | HTTP_URI_CODING_RFC6874) },
        { HTTP_URI_STATUS_OK, "http://server/admin?DEVICE_URI=usb://HP/Photosmart%25202600%2520series?serial=MY53OK70V10400",
          "http", "", "server", "/admin?DEVICE_URI=usb://HP/Photosmart%25202600%2520series?serial=MY53OK70V10400", 80, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "lpd://Acme%20Laser%20(01%3A23%3A45).local._tcp._printer/",
          "lpd", "", "Acme Laser (01:23:45).local._tcp._printer", "/", 515, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "ipp://HP%20Officejet%204500%20G510n-z%20%40%20Will's%20MacBook%20Pro%2015%22._ipp._tcp.local./",
          "ipp", "", "HP Officejet 4500 G510n-z @ Will's MacBook Pro 15\"._ipp._tcp.local.", "/", 631, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_OK, "ipp://%22%23%2F%3A%3C%3E%3F%40%5B%5C%5D%5E%60%7B%7C%7D/",
          "ipp", "", "\"#/:<>?@[\\]^`{|}", "/", 631, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_UNKNOWN_SCHEME, "smb://server/Some%20Printer",
          "smb", "", "server", "/Some Printer", 0, 0,
          HTTP_URI_CODING_ALL },

        /* Missing scheme */
        { HTTP_URI_STATUS_MISSING_SCHEME, "/path/to/file/index.html",
          "file", "", "", "/path/to/file/index.html", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_MISSING_SCHEME, "//server/ipp",
          "ipp", "", "server", "/ipp", 631, 0,
          HTTP_URI_CODING_MOST  },

        /* Unknown scheme */
        { HTTP_URI_STATUS_UNKNOWN_SCHEME, "vendor://server/resource",
          "vendor", "", "server", "/resource", 0, 0,
          HTTP_URI_CODING_MOST  },

        /* Missing resource */
        { HTTP_URI_STATUS_MISSING_RESOURCE, "socket://[::192.168.2.1]",
          "socket", "", "::192.168.2.1", "/", 9100, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_MISSING_RESOURCE, "socket://192.168.1.1:9101",
          "socket", "", "192.168.1.1", "/", 9101, 0,
          HTTP_URI_CODING_MOST  },

        /* Bad URI */
        { HTTP_URI_STATUS_BAD_URI, "",
          "", "", "", "", 0, 0,
          HTTP_URI_CODING_MOST  },

        /* Bad scheme */
        { HTTP_URI_STATUS_BAD_SCHEME, "://server/ipp",
          "", "", "", "", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_BAD_SCHEME, "bad_scheme://server/resource",
          "", "", "", "", 0, 0,
          HTTP_URI_CODING_MOST  },

        /* Bad username */
        { HTTP_URI_STATUS_BAD_USERNAME, "http://username:passwor%6@server/resource",
          "http", "", "", "", 80, 0,
          HTTP_URI_CODING_MOST  },

        /* Bad hostname */
        { HTTP_URI_STATUS_BAD_HOSTNAME, "http://[/::1]/index.html",
          "http", "", "", "", 80, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_BAD_HOSTNAME, "http://[",
          "http", "", "", "", 80, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_BAD_HOSTNAME, "http://serve%7/index.html",
          "http", "", "", "", 80, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_BAD_HOSTNAME, "http://server with spaces/index.html",
          "http", "", "", "", 80, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_BAD_HOSTNAME, "ipp://\"#/:<>?@[\\]^`{|}/",
          "ipp", "", "", "", 631, 0,
          HTTP_URI_CODING_MOST  },

        /* Bad port number */
        { HTTP_URI_STATUS_BAD_PORT, "http://127.0.0.1:9999a/index.html",
          "http", "", "127.0.0.1", "", 0, 0,
          HTTP_URI_CODING_MOST  },

        /* Bad resource */
        { HTTP_URI_STATUS_BAD_RESOURCE, "mailto:\r\nbla",
          "mailto", "", "", "", 0, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_BAD_RESOURCE, "http://server/index.html%",
          "http", "", "server", "", 80, 0,
          HTTP_URI_CODING_MOST  },
        { HTTP_URI_STATUS_BAD_RESOURCE, "http://server/index with spaces.html",
          "http", "", "server", "", 80, 0,
          HTTP_URI_CODING_MOST  }
      };

- (void)testHTTPHostname
{
  char hostname[HTTP_MAX_URI];
  http_addrlist_t *addrlist;
  http_addrlist_t *addr;
  int i;
  
  // httpGetHostname()
  
  const char *fqn = httpGetHostname(NULL, hostname, sizeof(hostname));
  XCTAssert(fqn);
  XCTAssert(strlen(hostname) > 0);
  
  // Test httpAddrGetList() and httpAddrString()
  
  addrlist = httpAddrGetList(hostname, AF_UNSPEC, NULL);
  XCTAssert(addrlist);
  
  for (i = 0, addr = addrlist; addr; i ++, addr = addr->next)
  {
    char  numeric[1024];    /* Numeric IP address */
    char *numAddr = httpAddrString(&(addr->addr), numeric, sizeof(numeric));
    XCTAssert(numAddr);
    XCTAssert(strlen(numeric) > 0);
    
    if (!strcmp(numeric, "UNKNOWN"))
      break;
  }

  httpAddrFreeList(addrlist);
}


- (void)testHTTPURIs
{
  static const int kNumURIs= (int)(sizeof(uri_tests) / sizeof(uri_tests[0]));
  http_uri_status_t uri_status;
  char scheme[HTTP_MAX_URI];
  char username[HTTP_MAX_URI];
  char hostname[HTTP_MAX_URI];
  char resource[HTTP_MAX_URI];
  int port;
  char buffer[8192];
  
  for (int i = 0; i < kNumURIs; i ++) {
    uri_status = httpSeparateURI(HTTP_URI_CODING_MOST,
                                 uri_tests[i].uri, scheme, sizeof(scheme),
                                 username, sizeof(username),
                                 hostname, sizeof(hostname), &port,
                                 resource, sizeof(resource));
    
    XCTAssert(uri_status == uri_tests[i].result);
    XCTAssert(!strcmp(scheme, uri_tests[i].scheme));
    XCTAssert(!strcmp(username, uri_tests[i].username));
    XCTAssert(!strcmp(hostname, uri_tests[i].hostname));
    XCTAssert(port == uri_tests[i].port);
    XCTAssert(!strcmp(resource, uri_tests[i].resource));
  }
  
  /*
   * Test httpAssembleURI()...
   */
  
  for (int i = 0; i < kNumURIs; i ++) {
    
    if (uri_tests[i].result == HTTP_URI_STATUS_OK &&
        !strstr(uri_tests[i].uri, "%64") &&
        strstr(uri_tests[i].uri, "//"))
    {
      uri_status = httpAssembleURI(uri_tests[i].assemble_coding,
                                   buffer, sizeof(buffer),
                                   uri_tests[i].scheme,
                                   uri_tests[i].username,
                                   uri_tests[i].hostname,
                                   uri_tests[i].assemble_port,
                                   uri_tests[i].resource);
      XCTAssert(uri_status == HTTP_URI_STATUS_OK);
      XCTAssert(!strcmp(buffer, uri_tests[i].uri));
    }
  }
  
  char *uuid = httpAssembleUUID("hostname.example.com", 631, "printer", 12345, buffer, sizeof(buffer));
  XCTAssert(uuid);
  XCTAssert(!strcmp(uuid, buffer));
  XCTAssert(!strncmp(buffer, "urn:uuid:", 9));
  
}

- (void)testHTTPDateString
{
  time_t start;
  time_t current;
  char buffer[8192];
  
  start = time(NULL);
  strlcpy(buffer, httpGetDateString(start), sizeof(buffer));
  current = httpGetDateTime(buffer);
  XCTAssert(start == current);
}

static const char * const base64_tests[][2] =
      {
        { "A", "QQ==" },
        /* 010000 01 */
        { "AB", "QUI=" },
        /* 010000 010100 0010 */
        { "ABC", "QUJD" },
        /* 010000 010100 001001 000011 */
        { "ABCD", "QUJDRA==" },
        /* 010000 010100 001001 000011 010001 00 */
        { "ABCDE", "QUJDREU=" },
        /* 010000 010100 001001 000011 010001 000100 0101 */
        { "ABCDEF", "QUJDREVG" },
        /* 010000 010100 001001 000011 010001 000100 010101 000110 */
      };

- (void)testBase64
{
  char encode[256];
  char decode[256];
  int decodelen;
  static const int kNumBase64Tests = (int)(sizeof(base64_tests) / sizeof(base64_tests[0]));
                                    
  for (int i = 0; i < kNumBase64Tests; i++) {
    httpEncode64_2(encode, sizeof(encode), base64_tests[i][0],
                   (int)strlen(base64_tests[i][0]));
    decodelen = (int)sizeof(decode);
    httpDecode64_2(decode, &decodelen, base64_tests[i][1]);
    XCTAssert(!strcmp(decode, base64_tests[i][0]));
  }
}

typedef struct ippdata_t
{
  size_t  rpos,      /* Read position */
    wused,      /* Bytes used */
    wsize;      /* Max size of buffer */
  ipp_uchar_t  *wbuffer;    /* Buffer */
} ippdata_t;

static ipp_uchar_t kCollection[] =  /* Collection buffer */
    {
      0x01, 0x01,    /* IPP version */
      0x00, 0x02,    /* Print-Job operation */
      0x00, 0x00, 0x00, 0x01,
          /* Request ID */

      IPP_TAG_OPERATION,

      IPP_TAG_CHARSET,
      0x00, 0x12,    /* Name length + name */
      'a','t','t','r','i','b','u','t','e','s','-',
      'c','h','a','r','s','e','t',
      0x00, 0x05,    /* Value length + value */
      'u','t','f','-','8',

      IPP_TAG_LANGUAGE,
      0x00, 0x1b,    /* Name length + name */
      'a','t','t','r','i','b','u','t','e','s','-',
      'n','a','t','u','r','a','l','-','l','a','n',
      'g','u','a','g','e',
      0x00, 0x02,    /* Value length + value */
      'e','n',

      IPP_TAG_URI,
      0x00, 0x0b,    /* Name length + name */
      'p','r','i','n','t','e','r','-','u','r','i',
      0x00, 0x1c,      /* Value length + value */
      'i','p','p',':','/','/','l','o','c','a','l',
      'h','o','s','t','/','p','r','i','n','t','e',
      'r','s','/','f','o','o',

      IPP_TAG_JOB,    /* job group tag */

      IPP_TAG_BEGIN_COLLECTION,
          /* begCollection tag */
      0x00, 0x09,    /* Name length + name */
      'm', 'e', 'd', 'i', 'a', '-', 'c', 'o', 'l',
      0x00, 0x00,    /* No value */
        IPP_TAG_MEMBERNAME,  /* memberAttrName tag */
        0x00, 0x00,    /* No name */
        0x00, 0x0a,    /* Value length + value */
        'm', 'e', 'd', 'i', 'a', '-', 's', 'i', 'z', 'e',
        IPP_TAG_BEGIN_COLLECTION,
          /* begCollection tag */
        0x00, 0x00,    /* Name length + name */
        0x00, 0x00,    /* No value */
          IPP_TAG_MEMBERNAME,
          /* memberAttrName tag */
          0x00, 0x00,  /* No name */
          0x00, 0x0b,  /* Value length + value */
          'x', '-', 'd', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n',
          IPP_TAG_INTEGER,  /* integer tag */
          0x00, 0x00,  /* No name */
          0x00, 0x04,  /* Value length + value */
          0x00, 0x00, 0x54, 0x56,
          IPP_TAG_MEMBERNAME,
          /* memberAttrName tag */
          0x00, 0x00,  /* No name */
          0x00, 0x0b,  /* Value length + value */
          'y', '-', 'd', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n',
          IPP_TAG_INTEGER,  /* integer tag */
          0x00, 0x00,  /* No name */
          0x00, 0x04,  /* Value length + value */
          0x00, 0x00, 0x6d, 0x24,
        IPP_TAG_END_COLLECTION,
          /* endCollection tag */
        0x00, 0x00,    /* No name */
        0x00, 0x00,    /* No value */
        IPP_TAG_MEMBERNAME,  /* memberAttrName tag */
        0x00, 0x00,    /* No name */
        0x00, 0x0b,    /* Value length + value */
        'm', 'e', 'd', 'i', 'a', '-', 'c', 'o', 'l', 'o', 'r',
        IPP_TAG_KEYWORD,  /* keyword tag */
        0x00, 0x00,    /* No name */
        0x00, 0x04,    /* Value length + value */
        'b', 'l', 'u', 'e',

        IPP_TAG_MEMBERNAME,  /* memberAttrName tag */
        0x00, 0x00,    /* No name */
        0x00, 0x0a,    /* Value length + value */
        'm', 'e', 'd', 'i', 'a', '-', 't', 'y', 'p', 'e',
        IPP_TAG_KEYWORD,  /* keyword tag */
        0x00, 0x00,    /* No name */
        0x00, 0x05,    /* Value length + value */
        'p', 'l', 'a', 'i', 'n',
      IPP_TAG_END_COLLECTION,
          /* endCollection tag */
      0x00, 0x00,    /* No name */
      0x00, 0x00,    /* No value */

      IPP_TAG_BEGIN_COLLECTION,
          /* begCollection tag */
      0x00, 0x00,    /* No name */
      0x00, 0x00,    /* No value */
        IPP_TAG_MEMBERNAME,  /* memberAttrName tag */
        0x00, 0x00,    /* No name */
        0x00, 0x0a,    /* Value length + value */
        'm', 'e', 'd', 'i', 'a', '-', 's', 'i', 'z', 'e',
        IPP_TAG_BEGIN_COLLECTION,
          /* begCollection tag */
        0x00, 0x00,    /* Name length + name */
        0x00, 0x00,    /* No value */
          IPP_TAG_MEMBERNAME,
          /* memberAttrName tag */
          0x00, 0x00,  /* No name */
          0x00, 0x0b,  /* Value length + value */
          'x', '-', 'd', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n',
          IPP_TAG_INTEGER,  /* integer tag */
          0x00, 0x00,  /* No name */
          0x00, 0x04,  /* Value length + value */
          0x00, 0x00, 0x52, 0x08,
          IPP_TAG_MEMBERNAME,
          /* memberAttrName tag */
          0x00, 0x00,  /* No name */
          0x00, 0x0b,  /* Value length + value */
          'y', '-', 'd', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n',
          IPP_TAG_INTEGER,  /* integer tag */
          0x00, 0x00,  /* No name */
          0x00, 0x04,  /* Value length + value */
          0x00, 0x00, 0x74, 0x04,
        IPP_TAG_END_COLLECTION,
          /* endCollection tag */
        0x00, 0x00,    /* No name */
        0x00, 0x00,    /* No value */
        IPP_TAG_MEMBERNAME,  /* memberAttrName tag */
        0x00, 0x00,    /* No name */
        0x00, 0x0b,    /* Value length + value */
        'm', 'e', 'd', 'i', 'a', '-', 'c', 'o', 'l', 'o', 'r',
        IPP_TAG_KEYWORD,  /* keyword tag */
        0x00, 0x00,    /* No name */
        0x00, 0x05,    /* Value length + value */
        'p', 'l', 'a', 'i', 'd',

        IPP_TAG_MEMBERNAME,  /* memberAttrName tag */
        0x00, 0x00,    /* No name */
        0x00, 0x0a,    /* Value length + value */
        'm', 'e', 'd', 'i', 'a', '-', 't', 'y', 'p', 'e',
        IPP_TAG_KEYWORD,  /* keyword tag */
        0x00, 0x00,    /* No name */
        0x00, 0x06,    /* Value length + value */
        'g', 'l', 'o', 's', 's', 'y',
      IPP_TAG_END_COLLECTION,
          /* endCollection tag */
      0x00, 0x00,    /* No name */
      0x00, 0x00,    /* No value */

      IPP_TAG_END    /* end tag */
    };

static ssize_t write_cb(ippdata_t *data, ipp_uchar_t *buffer, size_t bytes)
{
  size_t  count;      /* Number of bytes */

 /*
  * Loop until all bytes are written...
  */

  if ((count = data->wsize - data->wused) > bytes)
    count = bytes;

  memcpy(data->wbuffer + data->wused, buffer, count);
  data->wused += count;

 /*
  * Return the number of bytes written...
  */

  return ((ssize_t)count);
}

static ssize_t read_cb(ippdata_t *data, ipp_uchar_t *buffer, size_t bytes)
{
  size_t  count;      /* Number of bytes */

 /*
  * Copy bytes from the data buffer to the read buffer...
  */

  if ((count = data->wsize - data->rpos) > bytes)
    count = bytes;

  memcpy(buffer, data->wbuffer + data->rpos, count);
  data->rpos += count;

 /*
  * Return the number of bytes read...
  */

  return ((ssize_t)count);
}

static void checkMediaDimensions(ipp_t *mediaDesc, int expectedWidth, int expectedHeight)
{
  ipp_attribute_t *media_size = ippFindAttribute(mediaDesc, "media-size", IPP_TAG_BEGIN_COLLECTION);
  XCTAssert(media_size);
  
  ipp_t *mediaSizeCol = ippGetCollection(media_size, 0);
  XCTAssert(mediaSizeCol);

  ipp_attribute_t *xDim = ippFindAttribute(mediaSizeCol, "x-dimension", IPP_TAG_INTEGER);
  XCTAssert(xDim);

  int width = ippGetInteger(xDim, 0);
  XCTAssert(width == expectedWidth);

  ipp_attribute_t *yDim = ippFindAttribute(mediaSizeCol, "y-dimension", IPP_TAG_ZERO);
  XCTAssert(yDim);
  
  int height = ippGetInteger(yDim, 0);
  XCTAssert(height == expectedHeight);
  
}

- (void)testGoodIPPCollection
{
  int success;
  ipp_t *cols[2];
  ipp_t *size;
  ippdata_t data;
  ipp_state_t state;
  
  // Create a request, write it to memory, and check its format.
  
  ipp_t *request = ippNew();
  XCTAssert(request);
  
  XCTAssert(ippSetVersion(request, 0x01, 0x01));
  XCTAssert(ippSetOperation(request, IPP_OP_PRINT_JOB));
  XCTAssert(ippSetRequestId(request, 1));

  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
         "attributes-charset", NULL, "utf-8");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
         "attributes-natural-language", NULL, "en");
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI,
         "printer-uri", NULL, "ipp://localhost/printers/foo");

  cols[0] = ippNew();
  size    = ippNew();
  ippAddInteger(size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "x-dimension", 21590);
  ippAddInteger(size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "y-dimension", 27940);
  ippAddCollection(cols[0], IPP_TAG_JOB, "media-size", size);
  ippDelete(size);
  ippAddString(cols[0], IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-color", NULL,
               "blue");
  ippAddString(cols[0], IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-type", NULL,
               "plain");

  cols[1] = ippNew();
  size    = ippNew();
  ippAddInteger(size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "x-dimension", 21000);
  ippAddInteger(size, IPP_TAG_ZERO, IPP_TAG_INTEGER, "y-dimension", 29700);
  ippAddCollection(cols[1], IPP_TAG_JOB, "media-size", size);
  ippDelete(size);
  ippAddString(cols[1], IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-color", NULL,
               "plaid");
  ippAddString(cols[1], IPP_TAG_JOB, IPP_TAG_KEYWORD, "media-type", NULL,
   "glossy");

  ippAddCollections(request, IPP_TAG_JOB, "media-col", 2,
                    (const ipp_t **)cols);
  ippDelete(cols[0]);
  ippDelete(cols[1]);
  
  size_t length = ippLength(request);
  XCTAssert(length == sizeof(kCollection));
  
  ipp_uchar_t  buffer[8192];
  data.wused   = 0;
  data.wsize   = sizeof(buffer);
  data.wbuffer = buffer;

  while ((state = ippWriteIO(&data, (ipp_iocb_t)write_cb, 1, NULL, request)) != IPP_STATE_DATA) {
    if (state == IPP_STATE_ERROR) break;
  }
  XCTAssert(state == IPP_STATE_DATA);
  XCTAssert(data.wused == sizeof(kCollection));
  XCTAssert(!memcmp(data.wbuffer, kCollection, data.wused));
  
  ippDelete(request);
  
  ipp_t *request2 = ippNew();
  data.rpos = 0;

  while ((state = ippReadIO(&data, (ipp_iocb_t)read_cb, 1, NULL, request2)) != IPP_STATE_DATA) {
    if (state == IPP_STATE_ERROR) break;
  }
  
  length = ippLength(request2);
  XCTAssert(state == IPP_STATE_DATA);
  XCTAssert(length == sizeof(kCollection));
  XCTAssert(data.rpos == data.wused);
  
  ipp_attribute_t *media_col = ippFindAttribute(request2, "media-col", IPP_TAG_BEGIN_COLLECTION);
  XCTAssert(media_col);
  XCTAssert(ippGetCount(media_col) == 2);
  
  ipp_t *mediaDesc = ippGetCollection(media_col, 0);
  XCTAssert(mediaDesc);
  checkMediaDimensions(mediaDesc, 21590, 27940);
  
  ipp_t *mediaDesc2 = ippGetCollection(media_col, 1);
  XCTAssert(mediaDesc2);
  checkMediaDimensions(mediaDesc2, 21000, 29700);
  
  // Check hierarchical find.
  
  ipp_attribute_t *xDimAttr = ippFindAttribute(request2, "media-col/media-size/x-dimension", IPP_TAG_INTEGER);
  XCTAssert(xDimAttr);
  XCTAssert(ippGetInteger(xDimAttr, 0) == 21590);
  
  ipp_attribute_t *yDimAttr = ippFindAttribute(request2, "media-col/media-size/y-dimension", IPP_TAG_INTEGER);
  XCTAssert(yDimAttr);
  XCTAssert(ippGetInteger(yDimAttr, 0) == 27940);

  ippDelete(request2);
}

static ipp_uchar_t kBadCollection[] =  /* Collection buffer (bad encoding) */
    {
      0x01, 0x01,    /* IPP version */
      0x00, 0x02,    /* Print-Job operation */
      0x00, 0x00, 0x00, 0x01,
          /* Request ID */

      IPP_TAG_OPERATION,

      IPP_TAG_CHARSET,
      0x00, 0x12,    /* Name length + name */
      'a','t','t','r','i','b','u','t','e','s','-',
      'c','h','a','r','s','e','t',
      0x00, 0x05,    /* Value length + value */
      'u','t','f','-','8',

      IPP_TAG_LANGUAGE,
      0x00, 0x1b,    /* Name length + name */
      'a','t','t','r','i','b','u','t','e','s','-',
      'n','a','t','u','r','a','l','-','l','a','n',
      'g','u','a','g','e',
      0x00, 0x02,    /* Value length + value */
      'e','n',

      IPP_TAG_URI,
      0x00, 0x0b,    /* Name length + name */
      'p','r','i','n','t','e','r','-','u','r','i',
      0x00, 0x1c,      /* Value length + value */
      'i','p','p',':','/','/','l','o','c','a','l',
      'h','o','s','t','/','p','r','i','n','t','e',
      'r','s','/','f','o','o',

      IPP_TAG_JOB,    /* job group tag */

      IPP_TAG_BEGIN_COLLECTION,
          /* begCollection tag */
      0x00, 0x09,    /* Name length + name */
      'm', 'e', 'd', 'i', 'a', '-', 'c', 'o', 'l',
      0x00, 0x00,    /* No value */
        IPP_TAG_BEGIN_COLLECTION,
          /* begCollection tag */
        0x00, 0x0a,    /* Name length + name */
        'm', 'e', 'd', 'i', 'a', '-', 's', 'i', 'z', 'e',
        0x00, 0x00,    /* No value */
          IPP_TAG_INTEGER,  /* integer tag */
          0x00, 0x0b,  /* Name length + name */
          'x', '-', 'd', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n',
          0x00, 0x04,  /* Value length + value */
          0x00, 0x00, 0x54, 0x56,
          IPP_TAG_INTEGER,  /* integer tag */
          0x00, 0x0b,  /* Name length + name */
          'y', '-', 'd', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n',
          0x00, 0x04,  /* Value length + value */
          0x00, 0x00, 0x6d, 0x24,
        IPP_TAG_END_COLLECTION,
          /* endCollection tag */
        0x00, 0x00,    /* No name */
        0x00, 0x00,    /* No value */
      IPP_TAG_END_COLLECTION,
          /* endCollection tag */
      0x00, 0x00,    /* No name */
      0x00, 0x00,    /* No value */

      IPP_TAG_END    /* end tag */
    };

- (void)testBadIPPCollection
{
  ipp_state_t state;
  ippdata_t data = {
    .rpos = 0,
    .wused = sizeof(kBadCollection),
    .wsize = sizeof(kBadCollection),
    .wbuffer = kBadCollection
  };

  ipp_t *request = ippNew();
  XCTAssert(request);
  
  while ((state = ippReadIO(&data, (ipp_iocb_t)read_cb, 1, NULL, request)) != IPP_STATE_DATA) {
    if (state == IPP_STATE_ERROR) break;
  }
  
  // The collection is bad so we expect to fail the read.
  XCTAssert(state == IPP_STATE_ERROR);
  
  ippDelete(request);
}

static ipp_uchar_t kMixedCollection[] =    /* Mixed value buffer */
    {
      0x01, 0x01,    /* IPP version */
      0x00, 0x02,    /* Print-Job operation */
      0x00, 0x00, 0x00, 0x01,
          /* Request ID */

      IPP_TAG_OPERATION,

      IPP_TAG_INTEGER,  /* integer tag */
      0x00, 0x1f,    /* Name length + name */
      'n', 'o', 't', 'i', 'f', 'y', '-', 'l', 'e', 'a', 's', 'e',
      '-', 'd', 'u', 'r', 'a', 't', 'i', 'o', 'n', '-', 's', 'u',
      'p', 'p', 'o', 'r', 't', 'e', 'd',
      0x00, 0x04,    /* Value length + value */
      0x00, 0x00, 0x00, 0x01,

      IPP_TAG_RANGE,  /* rangeOfInteger tag */
      0x00, 0x00,    /* No name */
      0x00, 0x08,    /* Value length + value */
      0x00, 0x00, 0x00, 0x10,
      0x00, 0x00, 0x00, 0x20,

      IPP_TAG_END    /* end tag */
    };

- (void)testMixedPPCollection
{
  ipp_state_t state;
  ippdata_t data = {
    .rpos = 0,
    .wused = sizeof(kMixedCollection),
    .wsize = sizeof(kMixedCollection),
    .wbuffer = kMixedCollection
  };

  ipp_t *request = ippNew();
  XCTAssert(request);

  // Read the attribute and check the length
  
  while ((state = ippReadIO(&data, (ipp_iocb_t)read_cb, 1, NULL, request)) != IPP_STATE_DATA) {
    if (state == IPP_STATE_ERROR) break;
  }
  XCTAssert(state == IPP_STATE_DATA);
  XCTAssert(data.rpos == sizeof(kMixedCollection));
  
  size_t length = ippLength(request);
  XCTAssert(length == sizeof(kMixedCollection) + 4);
  
  ipp_attribute_t *leaseAttr = ippFindAttribute(request, "notify-lease-duration-supported", IPP_TAG_ZERO);
  XCTAssert(leaseAttr);

  XCTAssert(ippGetValueTag(leaseAttr) == IPP_TAG_RANGE);
  XCTAssert(ippGetCount(leaseAttr) == 2);
  
  // Check the first range
  
  int upper;
  int lower = ippGetRange(leaseAttr, 0, &upper);
  XCTAssert(lower == 1);
  XCTAssert(upper == 1);
  
  // Check the second range

  lower = ippGetRange(leaseAttr, 1, &upper);
  XCTAssert(lower == 16);
  XCTAssert(upper == 32);
  
  ippDelete(request);
}

typedef struct {
  const char *key;
  ipp_tag_t tag;
  int count;
  const char *value;
  const char *value2;
} OptionInfo;

- (void)testOptions
{
  static const OptionInfo optionInfo[] = {
    {"baz",       IPP_TAG_BEGIN_COLLECTION, 1, "{param1=1 param2=2}",                   NULL},
    {"foo",       IPP_TAG_NAME,             1, "1234",                                  NULL},
    {"bar",       IPP_TAG_NAME,             1, "One Fish,Two Fish,Red Fish,Blue Fish",  NULL},
    {"foobar",    IPP_TAG_NAME,             1, "FOO BAR",                               NULL},
    {"barfoo",    IPP_TAG_NAME,             1, "\'BAR FOO\'",                           NULL},
    {"auth-info", IPP_TAG_TEXT,             2, "user",                                  "pass,word\\"}
  };
  static const int kNumOptions = sizeof(optionInfo) / sizeof(optionInfo[0]);
  static char *optionsStr = "foo=1234 "
                        "bar=\"One Fish\",\"Two Fish\",\"Red Fish\","
                        "\"Blue Fish\" "
                        "baz={param1=1 param2=2} "
                        "foobar=FOO\\ BAR "
                        "barfoo=barfoo "
                        "barfoo=\"\'BAR FOO\'\" "
  "auth-info=user,pass\\\\,word\\\\\\\\";
  cups_option_t *options = NULL;
  
  // Parse the option string
  int num_options = cupsParseOptions(optionsStr, 0, &options);
  XCTAssert(num_options == 6);
  XCTAssert(options);
  
  // Make sure the parse options are correct
  for (int i = 0; i < kNumOptions; i++) {
    const OptionInfo *optionDesc = &optionInfo[i];
    const char *value = cupsGetOption(optionDesc->key, num_options, options);
    XCTAssert(value);
    if (optionDesc->count == 1) {
      XCTAssert(!strcmp(value, optionDesc->value));
    }
  }
  
  // Encode the options as an IPP request
  
  ipp_t *request = ippNew();
  XCTAssert(request);
  
  ippSetOperation(request, IPP_OP_PRINT_JOB);
  cupsEncodeOptions2(request, num_options, options, IPP_TAG_JOB);
  
  int count = 0;
  for (ipp_attribute_t *attr = ippFirstAttribute(request); attr; attr = ippNextAttribute(request)) {
    count++;
  }
  XCTAssert(count == 6);
  
  for (int i = 0; i < kNumOptions; i++) {
    const OptionInfo *optionDesc = &optionInfo[i];
    ipp_attribute_t *attr = ippFindAttribute(request, optionDesc->key, IPP_TAG_ZERO);
    XCTAssert(attr);
    XCTAssert(ippGetCount(attr) == optionDesc->count);
    XCTAssert(ippGetValueTag(attr) == optionDesc->tag);

    if (optionDesc->tag == IPP_TAG_NAME) {
      const char *value = ippGetString(attr, 0, NULL);
      XCTAssert(value);
      XCTAssert(!strcmp(value, optionDesc->value));
      
      if (optionDesc->count > 1) {
        const char *value2 = ippGetString(attr, 1, NULL);
        XCTAssert(value2);
        XCTAssert(!strcmp(value2, optionDesc->value2));
      }
    }
  }
  
  ippDelete(request);
}

- (void)testPWGMediaSizes
{
  
  {
    const pwg_media_t *a4Media = pwgMediaForPWG("iso_a4_210x297mm");
    XCTAssert(a4Media);
    XCTAssert(!strcmp(a4Media->pwg, "iso_a4_210x297mm"));
    XCTAssert(a4Media->width == 21000);
    XCTAssert(a4Media->length == 29700);
  }
  
  {
    const pwg_media_t *rollMedia = pwgMediaForPWG("roll_max_36.1025x3622.0472in");
    XCTAssert(rollMedia);
    XCTAssert(rollMedia->width == 91700);
    XCTAssert(rollMedia->length == 9199999);
  }
  
  {
    const pwg_media_t *discMedia = pwgMediaForPWG("disc_test_10x100mm");
    XCTAssert(discMedia);
    XCTAssert(discMedia->width == 10000);
    XCTAssert(discMedia->length == 10000);
  }
  
  {
    const pwg_media_t *ppd46Media = pwgMediaForPPD("4x6");
    XCTAssert(ppd46Media);
    XCTAssert(!strcmp(ppd46Media->pwg, "na_index-4x6_4x6in"));
    XCTAssert(ppd46Media->width == 10160);
    XCTAssert(ppd46Media->length == 15240);
  }
  
  {
    const pwg_media_t *ppd1015Media = pwgMediaForPPD("10x15cm");
    XCTAssert(ppd1015Media);
    XCTAssert(!strcmp(ppd1015Media->pwg, "om_100x150mm_100x150mm"));
    XCTAssert(ppd1015Media->width == 10000);
    XCTAssert(ppd1015Media->length == 15000);
  }
  
  {
    const pwg_media_t *ppdCustommedia = pwgMediaForPPD("Custom.10x15cm");
    XCTAssert(ppdCustommedia);
    XCTAssert(!strcmp(ppdCustommedia->pwg, "custom_10x15cm_100x150mm"));
  }
  
  {
    const pwg_media_t *sizedA3Media = pwgMediaForSize(29700, 42000);
    XCTAssert(sizedA3Media);
    XCTAssert(!strcmp(sizedA3Media->pwg, "iso_a3_297x420mm"));
  }
  
  {
    const pwg_media_t *sizedMonarchMedia = pwgMediaForSize(9842, 19050);
    XCTAssert(sizedMonarchMedia);
    XCTAssert(!strcmp(sizedMonarchMedia->pwg, "na_monarch_3.875x7.5in"));
  }
  
  {
    const pwg_media_t *sizedYou6Media = pwgMediaForSize(9800, 19000);
    XCTAssert(sizedYou6Media);
    XCTAssert(!strcmp(sizedYou6Media->pwg, "jpn_you6_98x190mm"));
  }
}

@end
