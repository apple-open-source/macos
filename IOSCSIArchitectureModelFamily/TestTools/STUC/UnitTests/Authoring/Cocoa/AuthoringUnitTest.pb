// !$*UTF8*$!
{
	archiveVersion = 1;
	classes = {
	};
	objectVersion = 39;
	objects = {
		014F4CCE00C5613A11CE15BF = {
			expectedFileType = wrapper.framework;
			fallbackIsa = PBXFileReference;
			isa = PBXFrameworkReference;
			name = System.framework;
			path = /System/Library/Frameworks/System.framework;
			refType = 0;
			sourceTree = "<absolute>";
		};
		01D59BA400ADEAD811CE1A69 = {
			expectedFileType = sourcecode.c.h;
			isa = PBXFileReference;
			path = AuthoringDeviceTester.h;
			refType = 4;
			sourceTree = "<group>";
		};
		01D59BA500ADEAD811CE1A69 = {
			expectedFileType = sourcecode.c.objc;
			isa = PBXFileReference;
			path = AuthoringDeviceTester.m;
			refType = 4;
			sourceTree = "<group>";
		};
		01D59BA700ADEAD811CE1A69 = {
			fileRef = 01D59BA500ADEAD811CE1A69;
			isa = PBXBuildFile;
			settings = {
			};
		};
		01D59BAE00AE1C8911CE1A69 = {
			expectedFileType = wrapper.framework;
			fallbackIsa = PBXFileReference;
			isa = PBXFrameworkReference;
			name = IOKit.framework;
			path = /System/Library/Frameworks/IOKit.framework;
			refType = 0;
			sourceTree = "<absolute>";
		};
		01D59BAF00AE1C8911CE1A69 = {
			fileRef = 01D59BAE00AE1C8911CE1A69;
			isa = PBXBuildFile;
			settings = {
			};
		};
//010
//011
//012
//013
//014
//020
//021
//022
//023
//024
		02E3FB8C00C4150411CE15BF = {
			fileRef = 0826EDEE00BED45011CE15BF;
			isa = PBXBuildFile;
			settings = {
			};
		};
		02E3FB8D00C4150411CE15BF = {
			fileRef = 0826EDEF00BED45011CE15BF;
			isa = PBXBuildFile;
			settings = {
			};
		};
//020
//021
//022
//023
//024
//040
//041
//042
//043
//044
		043355CEFE303625C02AAC07 = {
			buildActionMask = 2147483647;
			files = (
			);
			isa = PBXRezBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
		04F1F35200C6FC1E11CE15BF = {
			children = (
				04F1F35300C6FC1E11CE15BF,
			);
			isa = PBXVariantGroup;
			name = MainMenu.nib;
			path = "";
			refType = 4;
			sourceTree = "<group>";
		};
		04F1F35300C6FC1E11CE15BF = {
			expectedFileType = wrapper.nib;
			isa = PBXFileReference;
			name = English;
			path = English.lproj/MainMenu.nib;
			refType = 4;
			sourceTree = "<group>";
		};
		04F1F35400C6FC1E11CE15BF = {
			fileRef = 04F1F35200C6FC1E11CE15BF;
			isa = PBXBuildFile;
			settings = {
			};
		};
//040
//041
//042
//043
//044
//050
//051
//052
//053
//054
		05AFF72000AE22C011CE1A69 = {
			fileRef = 01D59BA400ADEAD811CE1A69;
			isa = PBXBuildFile;
			settings = {
			};
		};
//050
//051
//052
//053
//054
//080
//081
//082
//083
//084
		080E96DBFE201CDB7F000001 = {
			fileRef = 2A37F4B4FDCFA73011CA2CEA;
			isa = PBXBuildFile;
			settings = {
			};
		};
		0826EDEE00BED45011CE15BF = {
			expectedFileType = sourcecode.c.h;
			isa = PBXFileReference;
			path = InterfaceController.h;
			refType = 4;
			sourceTree = "<group>";
		};
		0826EDEF00BED45011CE15BF = {
			expectedFileType = sourcecode.c.objc;
			isa = PBXFileReference;
			path = InterfaceController.m;
			refType = 4;
			sourceTree = "<group>";
		};
		0826EDF400BED9B911CE15BF = {
			expectedFileType = sourcecode.c.h;
			isa = PBXFileReference;
			path = DeviceDataSource.h;
			refType = 4;
			sourceTree = "<group>";
		};
		0826EDF500BED9B911CE15BF = {
			expectedFileType = sourcecode.c.objc;
			isa = PBXFileReference;
			path = DeviceDataSource.m;
			refType = 4;
			sourceTree = "<group>";
		};
		0826EDF600BED9B911CE15BF = {
			fileRef = 0826EDF400BED9B911CE15BF;
			isa = PBXBuildFile;
			settings = {
			};
		};
		0826EDF700BED9B911CE15BF = {
			fileRef = 0826EDF500BED9B911CE15BF;
			isa = PBXBuildFile;
			settings = {
			};
		};
		0826EDF800BEDBD311CE15BF = {
			expectedFileType = sourcecode.c.h;
			isa = PBXFileReference;
			path = AuthoringDevice.h;
			refType = 4;
			sourceTree = "<group>";
		};
		0826EDF900BEDBD311CE15BF = {
			expectedFileType = sourcecode.c.objc;
			isa = PBXFileReference;
			path = AuthoringDevice.m;
			refType = 4;
			sourceTree = "<group>";
		};
		0826EDFA00BEDBD311CE15BF = {
			fileRef = 0826EDF800BEDBD311CE15BF;
			isa = PBXBuildFile;
			settings = {
			};
		};
		0826EDFB00BEDBD311CE15BF = {
			fileRef = 0826EDF900BEDBD311CE15BF;
			isa = PBXBuildFile;
			settings = {
			};
		};
		089C165FFE840EACC02AAC07 = {
			children = (
				089C1660FE840EACC02AAC07,
			);
			isa = PBXVariantGroup;
			name = InfoPlist.strings;
			refType = 4;
			sourceTree = "<group>";
		};
		089C1660FE840EACC02AAC07 = {
			expectedFileType = text.plist.strings;
			fileEncoding = 10;
			isa = PBXFileReference;
			name = English;
			path = English.lproj/InfoPlist.strings;
			refType = 4;
			sourceTree = "<group>";
		};
		089C1661FE840EACC02AAC07 = {
			fileRef = 089C165FFE840EACC02AAC07;
			isa = PBXBuildFile;
			settings = {
			};
		};
//080
//081
//082
//083
//084
//0F0
//0F1
//0F2
//0F3
//0F4
		0F623CF400BF100E11CE15BF = {
			expectedFileType = wrapper.framework;
			fallbackIsa = PBXFileReference;
			isa = PBXFrameworkReference;
			name = CoreFoundation.framework;
			path = /System/Library/Frameworks/CoreFoundation.framework;
			refType = 0;
			sourceTree = "<absolute>";
		};
//0F0
//0F1
//0F2
//0F3
//0F4
//100
//101
//102
//103
//104
		1058C7A6FEA54F5311CA2CBB = {
			children = (
				0F623CF400BF100E11CE15BF,
				1058C7A7FEA54F5311CA2CBB,
				01D59BAE00AE1C8911CE1A69,
			);
			isa = PBXGroup;
			name = "Linked Frameworks";
			refType = 4;
			sourceTree = "<group>";
		};
		1058C7A7FEA54F5311CA2CBB = {
			expectedFileType = wrapper.framework;
			fallbackIsa = PBXFileReference;
			isa = PBXFrameworkReference;
			name = Cocoa.framework;
			path = /System/Library/Frameworks/Cocoa.framework;
			refType = 0;
			sourceTree = "<absolute>";
		};
		1058C7A8FEA54F5311CA2CBB = {
			children = (
				2A37F4C5FDCFA73011CA2CEA,
				2A37F4C4FDCFA73011CA2CEA,
				014F4CCE00C5613A11CE15BF,
			);
			isa = PBXGroup;
			name = "Other Frameworks";
			refType = 4;
			sourceTree = "<group>";
		};
		1058C7A9FEA54F5311CA2CBB = {
			fileRef = 1058C7A7FEA54F5311CA2CBB;
			isa = PBXBuildFile;
			settings = {
			};
		};
//100
//101
//102
//103
//104
//170
//171
//172
//173
//174
		1758732AFF379DA111CA2CBB = {
			expectedFileType = wrapper.application;
			fallbackIsa = PBXFileReference;
			isa = PBXApplicationReference;
			path = AuthoringUnitTest.app;
			refType = 3;
			sourceTree = BUILT_PRODUCTS_DIR;
		};
//170
//171
//172
//173
//174
//190
//191
//192
//193
//194
		19C28FB0FE9D524F11CA2CBB = {
			children = (
				1758732AFF379DA111CA2CBB,
			);
			isa = PBXGroup;
			name = Products;
			refType = 4;
			sourceTree = "<group>";
		};
//190
//191
//192
//193
//194
//2A0
//2A1
//2A2
//2A3
//2A4
		2A37F4A9FDCFA73011CA2CEA = {
			buildSettings = {
			};
			buildStyles = (
				4A9504D0FFE6A4CB11CA0CBA,
				4A9504D1FFE6A4CB11CA0CBA,
			);
			hasScannedForEncodings = 1;
			isa = PBXProject;
			mainGroup = 2A37F4AAFDCFA73011CA2CEA;
			projectDirPath = "";
			targets = (
				2A37F4C6FDCFA73011CA2CEA,
			);
		};
		2A37F4AAFDCFA73011CA2CEA = {
			children = (
				2A37F4B0FDCFA73011CA2CEA,
				0826EDEE00BED45011CE15BF,
				0826EDEF00BED45011CE15BF,
				0826EDF400BED9B911CE15BF,
				0826EDF500BED9B911CE15BF,
				2A37F4AEFDCFA73011CA2CEA,
				2A37F4ACFDCFA73011CA2CEA,
				01D59BA400ADEAD811CE1A69,
				01D59BA500ADEAD811CE1A69,
				0826EDF800BEDBD311CE15BF,
				0826EDF900BEDBD311CE15BF,
				ACDEE76604D89E1A0001C085,
				ACDEE76704D89E1A0001C085,
				ACCEF86904D8814100D5E026,
				ACCEF86A04D8814100D5E026,
				2A37F4B8FDCFA73011CA2CEA,
				2A37F4C3FDCFA73011CA2CEA,
				19C28FB0FE9D524F11CA2CBB,
			);
			isa = PBXGroup;
			name = UserClientTester;
			path = "";
			refType = 4;
			sourceTree = "<group>";
		};
		2A37F4ACFDCFA73011CA2CEA = {
			expectedFileType = sourcecode.c.objc;
			isa = PBXFileReference;
			path = MyDocument.m;
			refType = 4;
			sourceTree = "<group>";
		};
		2A37F4AEFDCFA73011CA2CEA = {
			expectedFileType = sourcecode.c.h;
			isa = PBXFileReference;
			path = MyDocument.h;
			refType = 4;
			sourceTree = "<group>";
		};
		2A37F4B0FDCFA73011CA2CEA = {
			expectedFileType = sourcecode.c.objc;
			isa = PBXFileReference;
			path = main.m;
			refType = 4;
			sourceTree = "<group>";
		};
		2A37F4B4FDCFA73011CA2CEA = {
			children = (
				2A37F4B5FDCFA73011CA2CEA,
			);
			isa = PBXVariantGroup;
			name = MyDocument.nib;
			path = "";
			refType = 4;
			sourceTree = "<group>";
		};
		2A37F4B5FDCFA73011CA2CEA = {
			expectedFileType = wrapper.nib;
			isa = PBXFileReference;
			name = English;
			path = English.lproj/MyDocument.nib;
			refType = 4;
			sourceTree = "<group>";
		};
		2A37F4B8FDCFA73011CA2CEA = {
			children = (
				AC989A6604E99F5700C23B3E,
				AC989A6304E99F4200C23B3E,
				089C165FFE840EACC02AAC07,
			);
			isa = PBXGroup;
			name = Resources;
			path = "";
			refType = 4;
			sourceTree = "<group>";
		};
		2A37F4C3FDCFA73011CA2CEA = {
			children = (
				1058C7A6FEA54F5311CA2CBB,
				1058C7A8FEA54F5311CA2CBB,
			);
			isa = PBXGroup;
			name = Frameworks;
			path = "";
			refType = 4;
			sourceTree = "<group>";
		};
		2A37F4C4FDCFA73011CA2CEA = {
			expectedFileType = wrapper.framework;
			fallbackIsa = PBXFileReference;
			isa = PBXFrameworkReference;
			name = AppKit.framework;
			path = /System/Library/Frameworks/AppKit.framework;
			refType = 0;
			sourceTree = "<absolute>";
		};
		2A37F4C5FDCFA73011CA2CEA = {
			expectedFileType = wrapper.framework;
			fallbackIsa = PBXFileReference;
			isa = PBXFrameworkReference;
			name = Foundation.framework;
			path = /System/Library/Frameworks/Foundation.framework;
			refType = 0;
			sourceTree = "<absolute>";
		};
		2A37F4C6FDCFA73011CA2CEA = {
			buildPhases = (
				2A37F4C7FDCFA73011CA2CEA,
				2A37F4C9FDCFA73011CA2CEA,
				2A37F4CEFDCFA73011CA2CEA,
				2A37F4D1FDCFA73011CA2CEA,
				043355CEFE303625C02AAC07,
			);
			buildSettings = {
				FRAMEWORK_SEARCH_PATHS = "\"$(SYSTEM_LIBRARY_DIR)/PrivateFrameworks\"";
				HEADER_SEARCH_PATHS = "";
				INSTALL_PATH = "$(HOME)/Applications";
				LIBRARY_SEARCH_PATHS = "";
				OTHER_CFLAGS = "";
				PRODUCT_NAME = AuthoringUnitTest;
				SECTORDER_FLAGS = "";
				WARNING_CFLAGS = "-Wmost -Wno-four-char-constants -Wno-unknown-pragmas";
				WRAPPER_EXTENSION = app;
			};
			dependencies = (
			);
			isa = PBXApplicationTarget;
			name = AuthoringUnitTest;
			productInstallPath = "$(HOME)/Applications";
			productName = UserClientTester;
			productReference = 1758732AFF379DA111CA2CBB;
			productSettingsXML = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>English</string>
	<key>CFBundleExecutable</key>
	<string>AuthoringUnitTest</string>
	<key>CFBundleGetInfoString</key>
	<string>Copyright 2001-2003, Apple Computer Inc.</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>AuthoringUnitTest</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>1.0.0</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>CFBundleVersion</key>
	<string>1.0.0</string>
	<key>NSMainNibFile</key>
	<string>MainMenu</string>
	<key>NSPrincipalClass</key>
	<string>NSApplication</string>
</dict>
</plist>
";
		};
		2A37F4C7FDCFA73011CA2CEA = {
			buildActionMask = 2147483647;
			files = (
				2A37F4C8FDCFA73011CA2CEA,
				05AFF72000AE22C011CE1A69,
				0826EDF600BED9B911CE15BF,
				0826EDFA00BEDBD311CE15BF,
				02E3FB8C00C4150411CE15BF,
				ACCEF86B04D8814100D5E026,
				ACDEE76804D89E1A0001C085,
			);
			isa = PBXHeadersBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
		2A37F4C8FDCFA73011CA2CEA = {
			fileRef = 2A37F4AEFDCFA73011CA2CEA;
			isa = PBXBuildFile;
			settings = {
			};
		};
		2A37F4C9FDCFA73011CA2CEA = {
			buildActionMask = 2147483647;
			files = (
				080E96DBFE201CDB7F000001,
				089C1661FE840EACC02AAC07,
				04F1F35400C6FC1E11CE15BF,
				AC989A4004E99EE600C23B3E,
				AC989A4104E99EE600C23B3E,
				AC989A4204E99EE600C23B3E,
				AC989A4304E99EE600C23B3E,
				AC989A4404E99EE600C23B3E,
				AC989A4504E99EE600C23B3E,
				AC989A4604E99EE600C23B3E,
				AC989A4704E99EE600C23B3E,
				AC989A4804E99EE600C23B3E,
			);
			isa = PBXResourcesBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
		2A37F4CEFDCFA73011CA2CEA = {
			buildActionMask = 2147483647;
			files = (
				2A37F4CFFDCFA73011CA2CEA,
				2A37F4D0FDCFA73011CA2CEA,
				01D59BA700ADEAD811CE1A69,
				0826EDF700BED9B911CE15BF,
				0826EDFB00BEDBD311CE15BF,
				02E3FB8D00C4150411CE15BF,
				ACCEF86C04D8814100D5E026,
				ACDEE76904D89E1A0001C085,
			);
			isa = PBXSourcesBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
		2A37F4CFFDCFA73011CA2CEA = {
			fileRef = 2A37F4ACFDCFA73011CA2CEA;
			isa = PBXBuildFile;
			settings = {
				ATTRIBUTES = (
				);
			};
		};
		2A37F4D0FDCFA73011CA2CEA = {
			fileRef = 2A37F4B0FDCFA73011CA2CEA;
			isa = PBXBuildFile;
			settings = {
				ATTRIBUTES = (
				);
			};
		};
		2A37F4D1FDCFA73011CA2CEA = {
			buildActionMask = 2147483647;
			files = (
				1058C7A9FEA54F5311CA2CBB,
				01D59BAF00AE1C8911CE1A69,
				5C608A3B00FD238705CE15BF,
			);
			isa = PBXFrameworksBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
//2A0
//2A1
//2A2
//2A3
//2A4
//4A0
//4A1
//4A2
//4A3
//4A4
		4A9504D0FFE6A4CB11CA0CBA = {
			buildRules = (
			);
			buildSettings = {
				COPY_PHASE_STRIP = NO;
				GCC_DYNAMIC_NO_PIC = NO;
				GCC_ENABLE_FIX_AND_CONTINUE = NO;
				GCC_GENERATE_DEBUGGING_SYMBOLS = YES;
				GCC_OPTIMIZATION_LEVEL = s;
				OPTIMIZATION_CFLAGS = "-O0";
				ZERO_LINK = NO;
			};
			isa = PBXBuildStyle;
			name = Development;
		};
		4A9504D1FFE6A4CB11CA0CBA = {
			buildRules = (
			);
			buildSettings = {
				COPY_PHASE_STRIP = YES;
				GCC_ENABLE_FIX_AND_CONTINUE = NO;
				ZERO_LINK = NO;
			};
			isa = PBXBuildStyle;
			name = Deployment;
		};
//4A0
//4A1
//4A2
//4A3
//4A4
//5C0
//5C1
//5C2
//5C3
//5C4
		5C608A3B00FD238705CE15BF = {
			fileRef = 0F623CF400BF100E11CE15BF;
			isa = PBXBuildFile;
			settings = {
			};
		};
//5C0
//5C1
//5C2
//5C3
//5C4
//AC0
//AC1
//AC2
//AC3
//AC4
		AC989A3704E99EE600C23B3E = {
			expectedFileType = image.icns;
			isa = PBXFileReference;
			name = "CD-R.icns";
			path = "Images/CD-R.icns";
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A3804E99EE600C23B3E = {
			expectedFileType = image.icns;
			isa = PBXFileReference;
			name = "CD-RW.icns";
			path = "Images/CD-RW.icns";
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A3904E99EE600C23B3E = {
			expectedFileType = image.icns;
			isa = PBXFileReference;
			name = "DVD-R.icns";
			path = "Images/DVD-R.icns";
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A3A04E99EE600C23B3E = {
			expectedFileType = image.icns;
			isa = PBXFileReference;
			name = "DVD-RAM.icns";
			path = "Images/DVD-RAM.icns";
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A3B04E99EE600C23B3E = {
			expectedFileType = image.icns;
			isa = PBXFileReference;
			name = "DVD-RW.icns";
			path = "Images/DVD-RW.icns";
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A3C04E99EE600C23B3E = {
			expectedFileType = image.png;
			isa = PBXFileReference;
			name = "Fibre Channel Interface.png";
			path = "Images/Fibre Channel Interface.png";
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A3D04E99EE600C23B3E = {
			expectedFileType = image.png;
			isa = PBXFileReference;
			name = FireWire.png;
			path = Images/FireWire.png;
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A3E04E99EE600C23B3E = {
			expectedFileType = image.png;
			isa = PBXFileReference;
			name = "SCSI Parallel Interface.png";
			path = "Images/SCSI Parallel Interface.png";
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A3F04E99EE600C23B3E = {
			expectedFileType = image.png;
			isa = PBXFileReference;
			name = USB.png;
			path = Images/USB.png;
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A4004E99EE600C23B3E = {
			fileRef = AC989A3704E99EE600C23B3E;
			isa = PBXBuildFile;
			settings = {
			};
		};
		AC989A4104E99EE600C23B3E = {
			fileRef = AC989A3804E99EE600C23B3E;
			isa = PBXBuildFile;
			settings = {
			};
		};
		AC989A4204E99EE600C23B3E = {
			fileRef = AC989A3904E99EE600C23B3E;
			isa = PBXBuildFile;
			settings = {
			};
		};
		AC989A4304E99EE600C23B3E = {
			fileRef = AC989A3A04E99EE600C23B3E;
			isa = PBXBuildFile;
			settings = {
			};
		};
		AC989A4404E99EE600C23B3E = {
			fileRef = AC989A3B04E99EE600C23B3E;
			isa = PBXBuildFile;
			settings = {
			};
		};
		AC989A4504E99EE600C23B3E = {
			fileRef = AC989A3C04E99EE600C23B3E;
			isa = PBXBuildFile;
			settings = {
			};
		};
		AC989A4604E99EE600C23B3E = {
			fileRef = AC989A3D04E99EE600C23B3E;
			isa = PBXBuildFile;
			settings = {
			};
		};
		AC989A4704E99EE600C23B3E = {
			fileRef = AC989A3E04E99EE600C23B3E;
			isa = PBXBuildFile;
			settings = {
			};
		};
		AC989A4804E99EE600C23B3E = {
			fileRef = AC989A3F04E99EE600C23B3E;
			isa = PBXBuildFile;
			settings = {
			};
		};
		AC989A6304E99F4200C23B3E = {
			children = (
				AC989A3C04E99EE600C23B3E,
				AC989A3D04E99EE600C23B3E,
				AC989A3E04E99EE600C23B3E,
				AC989A3F04E99EE600C23B3E,
				AC989A3704E99EE600C23B3E,
				AC989A3804E99EE600C23B3E,
				AC989A3904E99EE600C23B3E,
				AC989A3A04E99EE600C23B3E,
				AC989A3B04E99EE600C23B3E,
			);
			isa = PBXGroup;
			name = Icons;
			refType = 4;
			sourceTree = "<group>";
		};
		AC989A6604E99F5700C23B3E = {
			children = (
				04F1F35200C6FC1E11CE15BF,
				2A37F4B4FDCFA73011CA2CEA,
			);
			isa = PBXGroup;
			name = Nibs;
			refType = 4;
			sourceTree = "<group>";
		};
		ACCEF86904D8814100D5E026 = {
			expectedFileType = sourcecode.c.h;
			fileEncoding = 30;
			isa = PBXFileReference;
			path = VerticallyCenteredTextFieldCell.h;
			refType = 4;
			sourceTree = "<group>";
		};
		ACCEF86A04D8814100D5E026 = {
			expectedFileType = sourcecode.c.objc;
			fileEncoding = 30;
			isa = PBXFileReference;
			path = VerticallyCenteredTextFieldCell.m;
			refType = 4;
			sourceTree = "<group>";
		};
		ACCEF86B04D8814100D5E026 = {
			fileRef = ACCEF86904D8814100D5E026;
			isa = PBXBuildFile;
			settings = {
			};
		};
		ACCEF86C04D8814100D5E026 = {
			fileRef = ACCEF86A04D8814100D5E026;
			isa = PBXBuildFile;
			settings = {
			};
		};
		ACDEE76604D89E1A0001C085 = {
			expectedFileType = sourcecode.c.h;
			fileEncoding = 30;
			isa = PBXFileReference;
			path = ImageAndTextCell.h;
			refType = 4;
			sourceTree = "<group>";
		};
		ACDEE76704D89E1A0001C085 = {
			expectedFileType = sourcecode.c.objc;
			fileEncoding = 30;
			isa = PBXFileReference;
			path = ImageAndTextCell.m;
			refType = 4;
			sourceTree = "<group>";
		};
		ACDEE76804D89E1A0001C085 = {
			fileRef = ACDEE76604D89E1A0001C085;
			isa = PBXBuildFile;
			settings = {
			};
		};
		ACDEE76904D89E1A0001C085 = {
			fileRef = ACDEE76704D89E1A0001C085;
			isa = PBXBuildFile;
			settings = {
			};
		};
	};
	rootObject = 2A37F4A9FDCFA73011CA2CEA;
}
