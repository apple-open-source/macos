// !$*UTF8*$!
{
	archiveVersion = 1;
	classes = {
	};
	objectVersion = 38;
	objects = {
		014CEA520018CE5811CA2923 = {
			buildRules = (
			);
			buildSettings = {
				COPY_PHASE_STRIP = NO;
				OPTIMIZATION_CFLAGS = "-O0";
			};
			isa = PBXBuildStyle;
			name = Development;
		};
		014CEA530018CE5811CA2923 = {
			buildRules = (
			);
			buildSettings = {
				COPY_PHASE_STRIP = YES;
			};
			isa = PBXBuildStyle;
			name = Deployment;
		};
//010
//011
//012
//013
//014
//030
//031
//032
//033
//034
		034768E8FF38A79811DB9C8B = {
			isa = PBXExecutableFileReference;
			path = USBNotificationTool;
			refType = 3;
		};
//030
//031
//032
//033
//034
//080
//081
//082
//083
//084
		08FB7793FE84155DC02AAC07 = {
			buildStyles = (
				014CEA520018CE5811CA2923,
				014CEA530018CE5811CA2923,
			);
			hasScannedForEncodings = 1;
			isa = PBXProject;
			mainGroup = 08FB7794FE84155DC02AAC07;
			projectDirPath = "";
			targets = (
				08FB779FFE84155DC02AAC07,
			);
		};
		08FB7794FE84155DC02AAC07 = {
			children = (
				08FB7795FE84155DC02AAC07,
				1AB674ADFE9D54B511CA2CBB,
			);
			isa = PBXGroup;
			name = USBNotificationTool;
			refType = 4;
		};
		08FB7795FE84155DC02AAC07 = {
			children = (
				F5946A30022578C5010162FA,
				F56A772201B3043401000164,
				F56A772401B3044901000164,
			);
			isa = PBXGroup;
			name = Source;
			refType = 4;
		};
		08FB779FFE84155DC02AAC07 = {
			buildPhases = (
				08FB77A0FE84155DC02AAC07,
				08FB77A1FE84155DC02AAC07,
				08FB77A3FE84155DC02AAC07,
				08FB77A5FE84155DC02AAC07,
			);
			buildSettings = {
				FRAMEWORK_SEARCH_PATHS = "";
				HEADER_SEARCH_PATHS = "";
				INSTALL_PATH = "$(HOME)/bin";
				LIBRARY_SEARCH_PATHS = "";
				OTHER_CFLAGS = "-W -Wall -Wno-unused";
				OTHER_LDFLAGS = "";
				OTHER_REZFLAGS = "";
				PRODUCT_NAME = USBNotificationTool;
				REZ_EXECUTABLE = YES;
				SECTORDER_FLAGS = "";
				WARNING_CFLAGS = "-Wmost -Wno-four-char-constants -Wno-unknown-pragmas";
			};
			dependencies = (
			);
			isa = PBXToolTarget;
			name = USBNotificationTool;
			productInstallPath = "$(HOME)/bin";
			productName = USBNotificationTool;
			productReference = 034768E8FF38A79811DB9C8B;
			shouldUseHeadermap = 1;
		};
		08FB77A0FE84155DC02AAC07 = {
			buildActionMask = 2147483647;
			files = (
			);
			isa = PBXHeadersBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
		08FB77A1FE84155DC02AAC07 = {
			buildActionMask = 2147483647;
			files = (
				F5946A31022578C5010162FA,
			);
			isa = PBXSourcesBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
		08FB77A3FE84155DC02AAC07 = {
			buildActionMask = 2147483647;
			files = (
				F56A772301B3043401000164,
				F56A772501B3044901000164,
			);
			isa = PBXFrameworksBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
		08FB77A5FE84155DC02AAC07 = {
			buildActionMask = 2147483647;
			files = (
			);
			isa = PBXRezBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
//080
//081
//082
//083
//084
//1A0
//1A1
//1A2
//1A3
//1A4
		1AB674ADFE9D54B511CA2CBB = {
			children = (
				034768E8FF38A79811DB9C8B,
			);
			isa = PBXGroup;
			name = Products;
			refType = 4;
		};
//1A0
//1A1
//1A2
//1A3
//1A4
//F50
//F51
//F52
//F53
//F54
		F56A772201B3043401000164 = {
			isa = PBXFrameworkReference;
			name = CoreFoundation.framework;
			path = /System/Library/Frameworks/CoreFoundation.framework;
			refType = 0;
		};
		F56A772301B3043401000164 = {
			fileRef = F56A772201B3043401000164;
			isa = PBXBuildFile;
			settings = {
			};
		};
		F56A772401B3044901000164 = {
			isa = PBXFrameworkReference;
			name = IOKit.framework;
			path = /System/Library/Frameworks/IOKit.framework;
			refType = 0;
		};
		F56A772501B3044901000164 = {
			fileRef = F56A772401B3044901000164;
			isa = PBXBuildFile;
			settings = {
			};
		};
		F5946A30022578C5010162FA = {
			fileEncoding = 30;
			isa = PBXFileReference;
			lineEnding = 0;
			path = USBNotificationExample.c;
			refType = 4;
		};
		F5946A31022578C5010162FA = {
			fileRef = F5946A30022578C5010162FA;
			isa = PBXBuildFile;
			settings = {
			};
		};
	};
	rootObject = 08FB7793FE84155DC02AAC07;
}
