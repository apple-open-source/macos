/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "_WKContentWorldConfigurationInternal.h"

@implementation _WKContentWorldConfiguration

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::ContentWorldConfiguration>(self);

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKContentWorldConfiguration.class, self))
        return;

    self._protectedWorldConfiguration->API::ContentWorldConfiguration::~ContentWorldConfiguration();

    [super dealloc];
}

- (Ref<API::ContentWorldConfiguration>)_protectedWorldConfiguration
{
    return *_worldConfiguration;
}

#pragma mark NSCopying protocol implementation

- (id)copyWithZone:(NSZone *)zone
{
    return wrapper(self._protectedWorldConfiguration->copy()).autorelease();
}

#pragma mark NSSecureCoding protocol implementation

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:retainPtr(self.name).get() forKey:@"name"];
    [coder encodeBool:self.allowAccessToClosedShadowRoots forKey:@"allowAccessToClosedShadowRoots"];
    [coder encodeBool:self.allowAutofill forKey:@"allowAutofill"];
    [coder encodeBool:self.allowElementUserInfo forKey:@"allowElementUserInfo"];
    [coder encodeBool:self.disableLegacyBuiltinOverrides forKey:@"disableLegacyBuiltinOverrides"];
    [coder encodeBool:self.allowJSHandleCreation forKey:@"allowJSHandleCreation"];
    [coder encodeBool:self.allowNodeSerialization forKey:@"allowNodeSerialization"];
    [coder encodeBool:self.isInspectable forKey:@"inspectable"];
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    self.name = [coder decodeObjectOfClass:[NSString class] forKey:@"name"];
    self.allowAccessToClosedShadowRoots = [coder decodeBoolForKey:@"allowAccessToClosedShadowRoots"];
    self.allowAutofill = [coder decodeBoolForKey:@"allowAutofill"];
    self.allowElementUserInfo = [coder decodeBoolForKey:@"allowElementUserInfo"];
    self.disableLegacyBuiltinOverrides = [coder decodeBoolForKey:@"disableLegacyBuiltinOverrides"];
    self.allowJSHandleCreation = [coder decodeBoolForKey:@"allowJSHandleCreation"];
    self.allowNodeSerialization = [coder decodeBoolForKey:@"allowNodeSerialization"];
    self.inspectable = [coder decodeBoolForKey:@"inspectable"];

    return self;
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_worldConfiguration;
}

- (NSString *)name
{
    return self._protectedWorldConfiguration->name().createNSString().autorelease();
}

- (void)setName:(NSString *)name
{
    self._protectedWorldConfiguration->setName(name);
}

- (BOOL)allowAccessToClosedShadowRoots
{
    return self._protectedWorldConfiguration->allowAccessToClosedShadowRoots();
}

- (void)setAllowAccessToClosedShadowRoots:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowAccessToClosedShadowRoots(allow);
}

- (BOOL)allowAutofill
{
    return self._protectedWorldConfiguration->allowAutofill();
}

- (void)setAllowAutofill:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowAutofill(allow);
}

- (BOOL)allowElementUserInfo
{
    return self._protectedWorldConfiguration->allowElementUserInfo();
}

- (void)setAllowElementUserInfo:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowElementUserInfo(allow);
}

- (BOOL)disableLegacyBuiltinOverrides
{
    return self._protectedWorldConfiguration->disableLegacyBuiltinOverrides();
}

- (void)setDisableLegacyBuiltinOverrides:(BOOL)disable
{
    self._protectedWorldConfiguration->setDisableLegacyBuiltinOverrides(disable);
}

- (BOOL)allowJSHandleCreation
{
    return self._protectedWorldConfiguration->allowJSHandleCreation();
}

- (void)setAllowJSHandleCreation:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowJSHandleCreation(allow);
}

- (BOOL)allowNodeSerialization
{
    return self._protectedWorldConfiguration->allowNodeSerialization();
}

- (void)setAllowNodeSerialization:(BOOL)allow
{
    self._protectedWorldConfiguration->setAllowNodeSerialization(allow);
}

- (BOOL)isInspectable
{
    return self._protectedWorldConfiguration->isInspectable();
}

- (void)setInspectable:(BOOL)inspectable
{
    self._protectedWorldConfiguration->setInspectable(inspectable);
}

@end
