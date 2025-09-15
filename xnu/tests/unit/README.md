# XNU user-space unit-tests

This folder contains unit-tests for in-kernel functionality, build as a user-space process

### Building a test:
```
> make -C tests/unit SDKROOT=macosx.internal <test-name>
```
This will build XNU as a library and link it into a test executable.  
`<test-name>` is the name of the test executable. There should be a corresponding `<test-name>.c`
Examples for `<test-name>`: `example_test_osfmk`, `example_test_bsd`

Useful customization for the make command:
- `VERBOSE=YES`  - Show more of the build commands
- `BUILD_WERROR=0`  - When building XNU, Do not convert warnings to errors
- `SKIP_XNU=1`  - Don't try to rebuild XNU
- `KERNEL_CONFIG=release`  - Build XNU in in release rather than 'development'
- `PRODUCT_CONFIG=...`  - Build XNU for a device other than the default. Only macos devices are supported 
- `BUILD_CODE_COVERAGE=1`  - Build with coverage support, see section below
- `FIBERS_PREEMPTION=1`  - Build with memory operations instrumentation to simulate preemption, see section below
- `BUILD_ASAN=1`  - Build with AddressSanitizer support
- `BUILD_UBSAN=1`  - Build with UndefinedBehaviourSanitizer support
- `BUILD_TSAN=1`  - Build with ThreadSanitizer support

### Running a test
The darwintest executable is created in `tests/unit/build/sym/`. To run all tests in an executable:
```text
> ./tests/unit/build/sym/<test-name>
```

### Creating a new test
- Add a `<test-name>.c` file in this directory with the test code.
- In the added .c file, add a line that looks like `#define UT_MODULE osfmk`
This determines the context in which the test is going to be built. This should be 
either "bsd" or "osfmk", depending on where the tested code resides. See example_test_bsd.c, example_test_osfmk.c.

### Building all tests
To build and run all the unit tests executables do:
```
> make -C tests/unit SDKROOT=macosx.internal install
> ./tests/unit/build/sym/run_unittests.sh
```
Another option is to run through the main Makefile:
```
> make SDKROOT=macosx.internal xnu_unittests
> ./BUILD/sym/run_unittests.sh
```
This is what the xnu_unittests build alias build. Notice that the output folder is different from the first option. 

## Debugging a test
```
> xcrun -sdk macosx.internal lldb ./tests/unit/build/sym/<test-name>
(lldb) run <test-case>
```
Notice that if the test executable contains more than one `T_DECL()`s, libdarwintest is going to run each `T_DECL()`
in a separate child process, so invoking `run` in lldb without the name of a specific `T_DECL()` will debug just the top
level process and not stop on breakpoints.
For a better debugging experience wrap debugged code with 
```
#pragma clang attribute push(__attribute__((noinline, optnone)), apply_to=function)
...
#pragma clang attribute pop
```
or annotate individual functions with `__attribute__((noinline, optnone))`

The unit-tests Makefile is able to generate files that allow easy debugging experience with various IDEs
```
> make SDKROOT=macosx.internal cmds_json
```
This make target adds the unit-tests executables that were built since the last `clean` to the `compile_commands.json`
file at the root of the repository so that IDEs that support this file (VSCode, CLion) know about the tests .c files 
as well as the XNU .c files.

### Debugging with Xcode
```
> make SDKROOT=macosx.internal proj_xcode
```
This reads the `compile_commands.json` file and generates an Xcode project named `ut_xnu_proj.xcodeproj` with all of 
XNU and the unit-tests source files, and running schemes for the test targets.
To debug using this project:
- Start Xcode, open the `ut_xnu_proj.xcodeproj` project
- At the top bar, select the runnning scheme named after the test executable name (`<test-name>`)
- In the same menu, press "Edit Scheme", go to "Run"->"Arguments" and add as an argument the name of the `T_DECL()`
to debug
- Again at the top bar, to the right of the name of the scheme press `My Mac (arm64e)` to open the Location menu
- Select `My Mac (arm64)` (instead of `My Mac (arm64e)`)
- Set a breakpoint in the test
- Press the Play button at the top bar

### Debugging with VSCode
```
> make SDKROOT=macosx.internal proj_vscode
```
This reads the `compile_commands.json` file and generates a `.vscode/launch.json` file for VSCode to know about
the executables to run.
(if you have such existing file it will be overwritten)
To debug in VSCode:
- (one time setup) Install the "LLDB DAP" extension
  - the "LLDB DAP" extension uses the lldb from the currently selected Xcode.app
- Open the XNU root folder
- Press the "Run and Debug" tab at the left bar
- Select the test executable name from the top menu (`<test-name>`)
- Press the gear icon next to it to edit launch.json
- In "args", write the name of the `T_DECL()` to debug
- Press the green play arrow next to the test name

### Debugging with CLion
```
> make SDKROOT=macosx.internal proj_clion
```
This reads the `compile_commands.json` file and edits the files in `.idea` for CLion to know about
the executables to run.
To debug in CLion you need CLion version 2025.1.3 or above which supports custom external lldb
- (one time setup) Add a new custom-lldb toolchain:
  - Open Settings -> "Build, Executaion, Deployment" -> Toolchains
  - Press the "+" icon above the list
  - Name the new toolchain "System"
  - At the bottom, next to "Debugger:" add the path to an installed Xcode.app
  - (it doesn't have to be the Xcode.app which is currently selected or the one which is used to build XNU)
- Open the XNU root folder
- At the top right select the test executable name (`<test-name>`) from the menu
- Press the menu again "Edit Configurations..."
- Next to "Program arguments:" write the name of the `T_DECL()` to debug
- Press the bug icon to at the top right to debug


## Running Coverage Analysis
1. Run the unit-test make command with the coverage option:
```
> make -C tests/unit SDKROOT=macosx.internal BUILD_CODE_COVERAGE=1 <test-name>
```
This will build XNU, the mocks dylib and the test executable with coverage instrumentation.
2. Run the unit-test and tell the coverage lib where to save the .profraw file:
```
> LLVM_PROFILE_FILE="coverage_data.profraw" ./tests/unit/build/sym/<test-name>
```
3. Convert the .profraw file to .profdata file:
```
> xcrun -sdk macosx.internal llvm-profdata merge -sparse coverage_data.profraw -o coverage_data.profdata
```
4. Generate reports

High-level per-file textual report:
```
> xcrun -sdk macosx.internal llvm-cov report ./tests/unit/build/sym/libkernel.development.t6020.dylib -instr-profile=coverage_data.profdata
```
Low-level per-line html pages in a directory structure:
```
> xcrun -sdk macosx.internal llvm-cov show ./tests/unit/build/sym/libkernel.development.t6020.dylib -instr-profile=coverage_data.profdata --format=html -output-dir ./_cov_html
> open ./_cov_html/index.html
```
Mind that both of these commands take the binary for which we want to show information for, in this case, the XNU dylib.
If you want to show the coverage for the unit-test executable, put that instead. It's also possible to specify multiple binaries with `-object` argument.

Both these commands can take `-sources` argument followed by the list of source files to limit the source files that would show in the report.
The names need to be the real paths of the files (relative or absolute), not just the path part that appears in the `report` output.
5. To check the coverage of a single function add `-name=<func-name>` to the `show` command.
6. To manually filter out functions from the report, for instance if the source file contains test functions which
are not interesting for coverage statistics:
- Add `-show-functions` to the `report` command and redirect the output to a file.
- From the output, take only the function names with:
`cat report_output.txt | cut -d " " -f1 | sort | uniq > func_names.txt`
- Edit the file and remove the functions names that are not needed.
Mind that in this list, static functions appear with the filename prefixed.
- Add the prefix `allowlist_fun:` to every line in the file:
`cat func_names.txt | sed 's/^/allowlist_fun:/' > allow_list.txt`
- Add the argument `-name-allowlist=allow_list.txt` to the `show` command.

See more documentation:
https://clang.llvm.org/docs/SourceBasedCodeCoverage.html
https://llvm.org/docs/CommandGuide/llvm-cov.html

## Deterministic threading with fibers
The mocks library provides a fibers implementation that can be used by tests including the header files in `mocks/fibers/`.

To access mocks that replace locking and scheduling APIs like lck_mtx_t and waitq functions, the test file must include `mocks/mock_thread.h`
and use the `UT_USE_FIBERS(1)` macro in the global scope.

By default, the context switch points are placed the entry and exit of the fibers API (e.g. before and after mutex lock) but preemption can be simulated using compiler instrumentation.
If you add `FIBERS_PREEMPTION=1` to the make command line, every memory load and store in the XNU library and in your test file will be instrumentated to be
a possible context switch point for the deterministic scheduler.

In addition, a data race detector can be enabled when the test is using fibers with preemption simulation.
The checker is a probabilistic data race sanitizer based on the [DataCollider](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Erickson.pdf) algorithm and can be used as
a replacement of ThreadSanitizer (that works with the fibers implementation but there can be false positives) or in combination.
The checker can be enabled with the macro `UT_FIBERS_USE_CHECKER(1)` in the global scope of the test file or setting the `FIBERS_CHECK_RACES` env var when executing a test with fibers.

For an example test using fibers read `fibers_test`.


## FAQ
- Q: I'm trying to call function X but I get a linker error "Undefined symbols for architecture arm64e: X referenced from..."
- A: This is likely due to the function being declared as hidden, either using `__private_extern__` at
the function declaration or a `#pragma GCC visibility push(hidden)`/`#pragma GCC visibility pop` pair around
where it's defined. You can verify this by doing:
`nm -m tests/unit/build/obj/libkernel.development.t6020.dylib | grep <function-name>`
and verifying that the function in questions appears next to a lower-case `t` to mean it's a private symbol
(as opposed to a capital `T` which means it's exported symbol, or it not appearing at all which means there is
no such function).
To fix that, simply change `__private_extern__` to `__exported_hidden` or the `#pragma` pair with
`__exported_push_hidden`/`__exported_pop`. These keep the visibility the same (hidden) for normal XNU build but
drop to the default (visible) for the user-mode build.


- Q: How to build XNU on-desk if it builds warnings with warnings which are converted to errors?
- A: In the make command line add `BUILD_WERROR=0`


- Q: lldb startup takes a long time and shows many errors about loading symbols
- A: try doing `dsymForUUID --disable` to disable automaic symbol loading
