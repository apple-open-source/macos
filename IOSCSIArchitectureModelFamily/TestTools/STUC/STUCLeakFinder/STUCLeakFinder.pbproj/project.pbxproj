// !$*UTF8*$!
{
	archiveVersion = 1;
	classes = {
	};
	objectVersion = 39;
	objects = {
		014CEA520018CE5811CA2923 = {
			buildRules = (
			);
			buildSettings = {
				COPY_PHASE_STRIP = NO;
				GCC_DYNAMIC_NO_PIC = NO;
				GCC_ENABLE_FIX_AND_CONTINUE = YES;
				GCC_GENERATE_DEBUGGING_SYMBOLS = YES;
				GCC_OPTIMIZATION_LEVEL = 0;
				OPTIMIZATION_CFLAGS = "-O0";
				ZERO_LINK = YES;
			};
			isa = PBXBuildStyle;
			name = Development;
		};
		014CEA530018CE5811CA2923 = {
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
			expectedFileType = "compiled.mach-o.executable";
			fallbackIsa = PBXFileReference;
			isa = PBXExecutableFileReference;
			path = STUCLeakFinder;
			refType = 3;
			sourceTree = BUILT_PRODUCTS_DIR;
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
			name = LeakFinder;
			refType = 4;
			sourceTree = "<group>";
		};
		08FB7795FE84155DC02AAC07 = {
			children = (
				08FB7796FE84155DC02AAC07,
				F554831F02455E83013850A9,
				F554832002455E83013850A9,
			);
			isa = PBXGroup;
			name = Source;
			refType = 4;
			sourceTree = "<group>";
		};
		08FB7796FE84155DC02AAC07 = {
			expectedFileType = sourcecode.c.c;
			isa = PBXFileReference;
			path = STUCLeakFinder.c;
			refType = 4;
			sourceTree = "<group>";
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
				OTHER_CFLAGS = "";
				OTHER_LDFLAGS = "";
				OTHER_REZFLAGS = "";
				PRODUCT_NAME = STUCLeakFinder;
				REZ_EXECUTABLE = YES;
				SECTORDER_FLAGS = "";
				WARNING_CFLAGS = "-Wmost -Wno-four-char-constants -Wno-unknown-pragmas";
			};
			dependencies = (
			);
			isa = PBXToolTarget;
			name = LeakFinder;
			productInstallPath = "$(HOME)/bin";
			productName = LeakFinder;
			productReference = 034768E8FF38A79811DB9C8B;
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
				08FB77A2FE84155DC02AAC07,
			);
			isa = PBXSourcesBuildPhase;
			runOnlyForDeploymentPostprocessing = 0;
		};
		08FB77A2FE84155DC02AAC07 = {
			fileRef = 08FB7796FE84155DC02AAC07;
			isa = PBXBuildFile;
			settings = {
				ATTRIBUTES = (
				);
			};
		};
		08FB77A3FE84155DC02AAC07 = {
			buildActionMask = 2147483647;
			files = (
				F55488BE02455E84013850A9,
				F55488BF02455E84013850A9,
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
			sourceTree = "<group>";
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
		F554831F02455E83013850A9 = {
			expectedFileType = wrapper.framework;
			fallbackIsa = PBXFileReference;
			isa = PBXFrameworkReference;
			name = CoreFoundation.framework;
			path = /System/Library/Frameworks/CoreFoundation.framework;
			refType = 0;
			sourceTree = "<absolute>";
		};
		F554832002455E83013850A9 = {
			expectedFileType = wrapper.framework;
			fallbackIsa = PBXFileReference;
			isa = PBXFrameworkReference;
			name = IOKit.framework;
			path = /System/Library/Frameworks/IOKit.framework;
			refType = 0;
			sourceTree = "<absolute>";
		};
		F55488BE02455E84013850A9 = {
			fileRef = F554831F02455E83013850A9;
			isa = PBXBuildFile;
			settings = {
			};
		};
		F55488BF02455E84013850A9 = {
			fileRef = F554832002455E83013850A9;
			isa = PBXBuildFile;
			settings = {
			};
		};
	};
	rootObject = 08FB7793FE84155DC02AAC07;
}
