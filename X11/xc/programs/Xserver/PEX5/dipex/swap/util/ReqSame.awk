# $Xorg: ReqSame.awk,v 1.4 2001/02/09 02:04:17 xorgcvs Exp $
##
## Copyright 1996, 1998  The Open Group
##
## Permission to use, copy, modify, distribute, and sell this software and its
## documentation for any purpose is hereby granted without fee, provided that
## the above copyright notice appear in all copies and that both that
## copyright notice and this permission notice appear in supporting
## documentation.
##
## The above copyright notice and this permission notice shall be included in
## all copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
## OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
## AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
## CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
##
## Except as contained in this notice, the name of The Open Group shall not be
## used in advertising or otherwise to promote the sale, use or other dealings
##
###########################################################################
## Copyright 1990, 1991 by Sun Microsystems, Inc.
## 
##                         All Rights Reserved
## 
## Permission to use, copy, modify, and distribute this software and its 
## documentation for any purpose and without fee is hereby granted, 
## provided that the above copyright notice appear in all copies and that
## both that copyright notice and this permission notice appear in 
## supporting documentation, and that the names of Sun Microsystems
## and The Open Group not be used in advertising or publicity 
## pertaining to distribution of the software without specific, written 
## prior permission.  
## 
## SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
## INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
## SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
## WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
## ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.
## 
###########################################################################
## Finds names that use the same swapping routine and renames them.
##
## Usage:
##	awk -f ReqSame.awk Requests.h > Requests.ci
##
BEGIN {
    num = 0; name = ""; lend = 0; oname = ""; rend = 0; D=0 }
 $1$2 == "#ifdefSWAP_FUNC_PREFIX" {
    D=1
    print "#include \"convReq.h\"" }
 D == 0 {
  if ($4 ~ /SWAP_FUNC_PREFIX/) {
    lend = index($0,"(PEX")
    rend = index($0,")")
    name = substr($0,(lend+4),(rend-lend-4))
    oname = name
    if (name == "FreeLookupTable") {
	name = "GenericResourceRequest" }
    else if (name == "GetDefinedIndices") {
	name = "GenericResourceRequest" }
    else if (name == "FreePipelineContext") {
	name = "GenericResourceRequest" }
    else if (name == "FreeRenderer") {
	name = "GenericResourceRequest" }
    else if (name == "EndRendering") {
	name = "GenericResourceRequest" }
    else if (name == "EndStructure") {
	name = "GenericResourceRequest" }
    else if (name == "CreateStructure") {
	name = "GenericResourceRequest" }
    else if (name == "GetRendererDynamics") {
	name = "GenericResourceRequest" }
    else if (name == "CreateNameSet") {
	name = "GenericResourceRequest" }
    else if (name == "FreeNameSet") {
	name = "GenericResourceRequest" }
    else if (name == "FreeSearchContext") {
	name = "GenericResourceRequest" }
    else if (name == "SearchNetwork") {
	name = "GenericResourceRequest" }
    else if (name == "GetNameSet") {
	name = "GenericResourceRequest" }
    else if (name == "FreePhigsWks") {
	name = "GenericResourceRequest" }
    else if (name == "RedrawAllStructures") {
	name = "GenericResourceRequest" }
    else if (name == "UpdateWorkstation") {
	name = "GenericResourceRequest" }
    else if (name == "ExecuteDeferredActions") {
	name = "GenericResourceRequest" }
    else if (name == "UnpostAllStructures") {
	name = "GenericResourceRequest" }
    else if (name == "GetWksPostings") {
	name = "GenericResourceRequest" }
    else if (name == "FreePickMeasure") {
	name = "GenericResourceRequest" }
    else if (name == "CloseFont") {
	name = "GenericResourceRequest"}
    else if (name == "GetDescendants") {
	name = "GetAncestors" } 
    else {
	name = ""
    }}
  if (name == "") {
    print $0 }
  else {
    print substr($0,1,lend) "PEX" name substr($0,rend,(length($0))) "\t/*" oname "*/" }
  name = "" }

 D == 1 { if ($0 ~ /;/) { D=0 } }
END { }
