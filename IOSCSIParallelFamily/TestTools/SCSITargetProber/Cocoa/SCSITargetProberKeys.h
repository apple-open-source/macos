/*
 * Copyright (c) 2004-2007 Apple Inc. All rights reserved.
 *
 * IMPORTANT:  This Apple software is supplied to you by Apple Inc. ("Apple") in 
 * consideration of your agreement to the following terms, and your use, installation, 
 * modification or redistribution of this Apple software constitutes acceptance of these
 * terms.  If you do not agree with these terms, please do not use, install, modify or 
 * redistribute this Apple software.
 *
 * In consideration of your agreement to abide by the following terms, and subject to these 
 * terms, Apple grants you a personal, non exclusive license, under Apple’s copyrights in this 
 * original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute 
 * the Apple Software, with or without modifications, in source and/or binary forms; provided 
 * that if you redistribute the Apple Software in its entirety and without modifications, you 
 * must retain this notice and the following text and disclaimers in all such redistributions 
 * of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple 
 * Computer, Inc. may be used to endorse or promote products derived from the Apple Software 
 * without specific prior written permission from Apple. Except as expressly stated in this 
 * notice, no other rights or licenses, express or implied, are granted by Apple herein, 
 * including but not limited to any patent rights that may be infringed by your derivative 
 * works or by other works in which the Apple Software may be incorporated.
 * 
 * The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-
 * INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE 
 * SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 
 *
 * IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
 * REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND 
 * WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR 
 * OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// NSUserDefaults strings
#define kShowTargetIDString					@"showTargetID"
#define kShowDescriptionString				@"showDescription"
#define kShowRevisionString					@"showRevision"
#define kShowFeaturesString					@"showFeatures"
#define kShowPDTString						@"showPDT"

// NSUserDefaults key path strings
#define kShowTargetIDKeyPath				@"values.showTargetID"
#define kShowDescriptionKeyPath				@"values.showDescription"
#define kShowRevisionKeyPath				@"values.showRevision"
#define kShowFeaturesKeyPath				@"values.showFeatures"
#define kShowPDTKeyPath						@"values.showPDT"

// Table column identifier strings
#define kTableColumnIDString				@"id"
#define kTableColumnDescriptionString		@"description"
#define kTableColumnRevisionString			@"revision"
#define kTableColumnFeaturesString			@"features"
#define kTableColumnPDTString				@"pdt"


// SCSIInitiator key path strings
#define kInitiatorsKeyPath					@"initiators"
#define kDevicesKeyPath						@"devices"

// SCSIDevice key path strings
#define kDeviceIdentifierKeyPath			@"deviceIdentifier"
#define kDeviceTitleKeyPath					@"title"
#define kDeviceRevisionKeyPath				@"revision"
#define kDeviceFeaturesKeyPath				@"features"
#define kDevicePDTKeyPath					@"peripheralDeviceType"

// Localized resource strings
#define kNoControllersFoundTitle			@"NoControllersFoundTitle"
#define kNoControllersFoundText				@"NoControllersFoundText"

#define kHideInfoMenuItemString				@"HideInfoMenuItemTitle"
#define kGetInfoMenuItemString				@"GetInfoMenuItemTitle"
#define kHideInfoToolbarItemString			@"HideInfoToolbarItemTitle"
#define kHideInfoToolbarItemToolTipString	@"HideInfoToolbarItemToolTip"
#define kGetInfoToolbarItemString			@"GetInfoToolbarItemTitle"
#define kGetInfoToolbarItemToolTipString	@"GetInfoToolbarItemToolTip"
#define kShowPrefsToolbarItemString			@"ShowPrefsToolbarItemTitle"
#define kShowPrefsToolbarItemToolTipString	@"ShowPrefsToolbarItemToolTip"

#define kIDString							@"ID"
#define kDescriptionString					@"Description"
#define kRevisionString						@"Revision"
#define kFeaturesString						@"Features"
#define kPDTString							@"PDT"

#define kSCSIParallelFeatureSyncString		@"Sync"
#define kSCSIParallelFeatureWideString		@"Wide"
#define kSCSIParallelFeatureQASString		@"QAS"
#define kSCSIParallelFeatureDTString		@"DT"
#define kSCSIParallelFeatureIUString		@"IU"

// Localized image resource strings
#define kInfoImageString					@"info"
#define kPrefsImageString					@"prefs"
#define kHardDiskImageString				@"hd"
#define kNothingImageString					@"nothing"
#define kPCICardImageString					@"pcicard"
#define kPCCardImageString					@"pccard"