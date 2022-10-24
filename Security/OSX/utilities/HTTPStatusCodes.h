/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

//
//  HTTPStatusCodes.h
//  utilities
//

#ifndef _HTTPSTATUSCODES_H_
#define _HTTPSTATUSCODES_H_

#pragma mark RFC 2616 (Hypertext Transfer Protocol -- HTTP/1.1)
// http://tools.ietf.org/html/rfc2616#section-10

#pragma mark 1xx - Informational
#define HTTPResponseCodeContinue                      100
#define HTTPResponseCodeSwitchingProtocols            101

#pragma mark 2xx - Successful
#define HTTPResponseCodeOK                            200
#define HTTPResponseCodeCreated                       201
#define HTTPResponseCodeAccepted                      202
#define HTTPResponseCodeNonAuthoritativeInformation   203
#define HTTPResponseCodeNoContent                     204
#define HTTPResponseCodeResetContent                  205
#define HTTPResponseCodePartialContent                206

#pragma mark 3xx - Redirection
#define HTTPResponseCodeMultipleChoices               300
#define HTTPResponseCodeMovedPermanently              301
#define HTTPResponseCodeFound                         302
#define HTTPResponseCodeSeeOther                      303
#define HTTPResponseCodeNotModified                   304
#define HTTPResponseCodeUseProxy                      305
#define HTTPResponseCodeTemporaryRedirect             307

#pragma mark 4xx - Client Error
#define HTTPResponseCodeBadRequest                    400
#define HTTPResponseCodeUnauthorized                  401
#define HTTPResponseCodePaymentRequired               402
#define HTTPResponseCodeForbidden                     403
#define HTTPResponseCodeNotFound                      404
#define HTTPResponseCodeMethodNotAllowed              405
#define HTTPResponseCodeNotAcceptable                 406
#define HTTPResponseCodeProxyAuthenticationRequired   407
#define HTTPResponseCodeRequestTimeout                408
#define HTTPResponseCodeConflict                      409
#define HTTPResponseCodeGone                          410
#define HTTPResponseCodeLengthRequired                411
#define HTTPResponseCodePreconditionFailed            412
#define HTTPResponseCodeRequestEntityTooLarge         413
#define HTTPResponseCodeRequestURITooLong             414
#define HTTPResponseCodeUnsupportedMediaType          415
#define HTTPResponseCodeRequestedRangeNotSatisfiable  416
#define HTTPResponseCodeExpectationFailed             417
#define HTTPResponseCodeImATeaPot                     418

#pragma mark 5xx - Server Error
#define HTTPResponseCodeInternalServerError           500
#define HTTPResponseCodeNotImplemented                501
#define HTTPResponseCodeBadGateway                    502
#define HTTPResponseCodeServiceUnavailable            503
#define HTTPResponseCodeGatewayTimeout                504
#define HTTPResponseCodeHTTPVersionNotSupported       505

#pragma mark -
#pragma mark RFC 4918 (HTTP Extensions for Web Distributed Authoring and Versioning (WebDAV))
// http://tools.ietf.org/html/rfc4918#section-11

#pragma mark 2xx - Successful
#define HTTPResponseCodeMultiStatus                   207

#pragma mark 4xx - Client Error
#define HTTPResponseCodeUnprocessableEntity           422
#define HTTPResponseCodeLocked                        423
#define HTTPResponseCodeFailedDependency              424

#pragma mark 5xx - Server Error
#define HTTPResponseCodeInsufficientStorage           507

#endif
