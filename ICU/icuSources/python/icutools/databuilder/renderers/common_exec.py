# Copyright (C) 2018 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

from . import *
from .. import *
from .. import utils
from ..request_types import *

import pathlib
import os
import shutil
import subprocess
import sys

def run(build_dirs, requests, common_vars, verbose=True, **kwargs):
    for bd in build_dirs:
        makedirs(bd.format(**common_vars))
    for request in requests:
        status = run_helper(request, common_vars, verbose=verbose, **kwargs)
        if status != 0:
            print("!!! ERROR executing above command line: exit code %d" % status)
            return 1
    if verbose:
        print("All data build commands executed")
    return 0

def makedirs(dirs):
    """makedirs compatible between Python 2 and 3"""
    try:
        # Python 3 version
        os.makedirs(dirs, exist_ok=True)
    except TypeError as e:
        # Python 2 version
        try:
            os.makedirs(dirs)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise e

def run_helper(request, common_vars, platform, tool_dir, verbose, tool_cfg=None, **kwargs):
    # [The block below was added for APPLE_XCODE_BUILD]
    # The block of code below checks the mod dates of the files involved in the request--
    # if all of the output files exist and all of them are newer than the newest input file,
    # we don't run the tool.  If the request's input or output file list is empty, we run the
    # tool.  This applies to index and resource-list requests, which depend on other requests
    # and strictly don't need to run when the things they depend on haven't been updated, but
    # they're quick, so I didn't worry about this.
    if len(request.all_input_files()) > 0 and len(request.all_output_files()) > 0:
        input_paths = [pathlib.Path(utils.dir_for(input_file).format(**common_vars), input_file.filename) for input_file in request.all_input_files()]
        newest_input_mod_date = max([os.path.getmtime(input_path) for input_path in input_paths])
        output_paths = [pathlib.Path(utils.dir_for(output_file).format(**common_vars), output_file.filename) for output_file in request.all_output_files()]
        oldest_output_mod_date = min([(os.path.getmtime(output_path) if os.path.exists(output_path) else 0) for output_path in output_paths])
        if newest_input_mod_date <= oldest_output_mod_date:
            return 0

    if isinstance(request, PrintFileRequest):
        output_path = "{DIRNAME}/{FILENAME}".format(
            DIRNAME = utils.dir_for(request.output_file).format(**common_vars),
            FILENAME = request.output_file.filename,
        )
        if verbose:
            print("Printing to file: %s" % output_path)
        with open(output_path, "w") as f:
            f.write(request.content)
        return 0
    if isinstance(request, CopyRequest):
        input_path = "{DIRNAME}/{FILENAME}".format(
            DIRNAME = utils.dir_for(request.input_file).format(**common_vars),
            FILENAME = request.input_file.filename,
        )
        output_path = "{DIRNAME}/{FILENAME}".format(
            DIRNAME = utils.dir_for(request.output_file).format(**common_vars),
            FILENAME = request.output_file.filename,
        )
        if verbose:
            print("Copying file to: %s" % output_path)
        shutil.copyfile(input_path, output_path)
        return 0
    if isinstance(request, VariableRequest):
        # No-op
        return 0

    assert isinstance(request.tool, IcuTool)
    if platform == "windows":
        cmd_template = "{TOOL_DIR}/{TOOL}/{TOOL_CFG}/{TOOL}.exe {{ARGS}}".format(
            TOOL_DIR = tool_dir,
            TOOL_CFG = tool_cfg,
            TOOL = request.tool.name,
            **common_vars
        )
    elif platform == "unix":
        cmd_template = "{TOOL_DIR}/{TOOL} {{ARGS}}".format(
            TOOL_DIR = tool_dir,
            TOOL = request.tool.name,
            **common_vars
        )
    elif platform == "bazel":
        cmd_template = "{TOOL_DIR}/{TOOL}/{TOOL} {{ARGS}}".format(
            TOOL_DIR = tool_dir,
            TOOL = request.tool.name,
            **common_vars
        )
    else:
        raise ValueError("Unknown platform: %s" % platform)

    if isinstance(request, RepeatedExecutionRequest):
        can_skip_files = not("EXTRA_OPTION" in request.format_with and "--usePoolBundle" in request.format_with["EXTRA_OPTION"])
        can_skip_files = can_skip_files and request.category != "translit"
        for loop_vars in utils.repeated_execution_request_looper(request):
        	# [The block below was added for APPLE_XCODE_BUILD]
            # The mod-date checks at the top of this function determine whether to execute an entire request--
            # if the request is a repeated execution request, it'll still allow all iterations of the request
            # to execute.  So we do an extra check here to skip any iterations of the repeated request where the
            # output file exists and is newer than the input file.  (This logic doesn't work if we're creating resource
            # bundles using the --write-pool-bundle/--use-pool-bundle options-- when we're using the pool bundle, we
            # have to rebuild EVERYTHING if any of the input files have changed.  It also doesn't work for the translit
            # rules, which have many input files.)
            if can_skip_files:
                (_, _, input_file, output_file) = loop_vars
                input_path = pathlib.Path(utils.dir_for(input_file).format(**common_vars), input_file.filename)
                output_path = pathlib.Path(utils.dir_for(output_file).format(**common_vars), output_file.filename)
                if os.path.exists(output_path) and os.path.getmtime(output_path) > os.path.getmtime(input_path):
                    continue

            command_line = utils.format_repeated_request_command(
                request,
                cmd_template,
                loop_vars,
                common_vars
            )
            if platform == "windows":
                # Note: this / to \ substitution may be too aggressive?
                command_line = command_line.replace("/", "\\")
            returncode = run_shell_command(command_line, platform, verbose)
            if returncode != 0:
                return returncode
        return 0
    if isinstance(request, SingleExecutionRequest):
        command_line = utils.format_single_request_command(
            request,
            cmd_template,
            common_vars
        )
        if platform == "windows":
            # Note: this / to \ substitution may be too aggressive?
            command_line = command_line.replace("/", "\\")
        returncode = run_shell_command(command_line, platform, verbose)
        return returncode
    assert False

def run_shell_command(command_line, platform, verbose):
    changed_windows_comspec = False
    # If the command line length on Windows exceeds the absolute maximum that CMD supports (8191), then
    # we temporarily switch over to use PowerShell for the command, and then switch back to CMD.
    # We don't want to use PowerShell for everything though, as it tends to be slower.
    if (platform == "windows"):
        previous_comspec = os.environ["COMSPEC"]
        # Add 7 to the length for the argument /c with quotes.
        # For example:  C:\WINDOWS\system32\cmd.exe /c "<command_line>"
        if ((len(previous_comspec) + len(command_line) + 7) > 8190):
            if verbose:
                print("Command length exceeds the max length for CMD on Windows, using PowerShell instead.")
            os.environ["COMSPEC"] = 'powershell'
            changed_windows_comspec = True
    if verbose:
        print("Running: %s" % command_line)
        returncode = subprocess.call(
            command_line,
            shell = True
        )
    else:
        # Pipe output to /dev/null in quiet mode
        with open(os.devnull, "w") as devnull:
            returncode = subprocess.call(
                command_line,
                shell = True,
                stdout = devnull,
                stderr = devnull
            )
    if changed_windows_comspec:
        os.environ["COMSPEC"] = previous_comspec
    return returncode
