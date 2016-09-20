/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "CommandLine.h"
#include <getopt.h>
#include <iostream>

struct option CommandLine::longOptions[] =
{
    {"benchmark", required_argument, 0, 'b'},
    {"parallel", no_argument, 0, 'p'},
    {"heap", required_argument, 0, 'h'},
    {"runs", required_argument, 0, 'r'},
    {0, 0, 0, 0}
};

CommandLine::CommandLine(std::string name, bool parallel)
	: m_benchmarkName(name)
    , m_isParallel(parallel)
    , m_heapSize(0)
    , m_runs(4)
{
}

void CommandLine::printUsage()
{
    std::string fullPath(m_argv[0]);
    size_t pos = fullPath.find_last_of("/") + 1;
    std::string program = fullPath.substr(pos);
    std::cout << "Usage: " << program << " --benchmark benchmark_name [ --parallel ] [ --heap MB ]" << std::endl;
}
