#ifndef _SECURE_DOWNLOAD_INTERNAL_
#define _SECURE_DOWNLOAD_INTERNAL_

#ifdef __cplusplus
extern "C" {
#endif

#define SD_XML_NAME					CFSTR("name")
#define SD_XML_SIZE					CFSTR("size")
#define SD_XML_CREATED				CFSTR("created")
#define SD_XML_URL					CFSTR("url")
#define SD_XML_VERIFICATIONS		CFSTR("verifications")
#define SD_XML_DIGEST				CFSTR("digest")
#define SD_XML_SECTOR_SIZE			CFSTR("sector_size")
#define SD_XML_DIGESTS				CFSTR("digests")

CF_RETURNS_RETAINED CFPropertyListRef _SecureDownloadParseTicketXML(CFDataRef xmlData);
CFDataRef _SecureDownloadCreateTicketXML(CFPropertyListRef plist);

#ifdef __cplusplus
};
#endif

#endif
