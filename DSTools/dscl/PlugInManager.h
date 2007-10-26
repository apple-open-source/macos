#pragma once

#import "PathManager.h"

bool dscl_PlugInDispatch(int argc, char* argv[], BOOL interactive, u_int32_t dsid, PathManager* engine, tDirStatus* status);
void dscl_PlugInShowUsage(FILE* fp);

