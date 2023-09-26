// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "number_microprops.h"
#include "unicode/numberformatter.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

SymbolsWrapper::SymbolsWrapper(const SymbolsWrapper &other) {
    doCopyFrom(other);
}

SymbolsWrapper::SymbolsWrapper(SymbolsWrapper &&src) U_NOEXCEPT {
    doMoveFrom(std::move(src));
}

SymbolsWrapper &SymbolsWrapper::operator=(const SymbolsWrapper &other) {
    if (this == &other) {
        return *this;
    }
    doCleanup();
    doCopyFrom(other);
    return *this;
}

SymbolsWrapper &SymbolsWrapper::operator=(SymbolsWrapper &&src) U_NOEXCEPT {
    if (this == &src) {
        return *this;
    }
    doCleanup();
    doMoveFrom(std::move(src));
    return *this;
}

SymbolsWrapper::~SymbolsWrapper() {
    doCleanup();
}

void SymbolsWrapper::setTo(const DecimalFormatSymbols &dfs) {
    doCleanup();
    fType = SYMPTR_DFS;
    fPtr.dfs = new DecimalFormatSymbols(dfs);
}

void SymbolsWrapper::setTo(const NumberingSystem *ns) {
    doCleanup();
    fType = SYMPTR_NS;
    fPtr.ns = ns;
}

void SymbolsWrapper::doCopyFrom(const SymbolsWrapper &other) {
    fType = other.fType;
    switch (fType) {
    case SYMPTR_NONE:
        // No action necessary
        break;
    case SYMPTR_DFS:
        // Memory allocation failures are exposed in copyErrorTo()
        if (other.fPtr.dfs != nullptr) {
            fPtr.dfs = new DecimalFormatSymbols(*other.fPtr.dfs);
        } else {
            fPtr.dfs = nullptr;
        }
        break;
#if APPLE_ICU_CHANGES
// rdar:/
    case SYMPTR_DFS_SHALLOWCOPY: // Apple rdar://49955427
        // DecimalFormatSymbols pointer copy, do not clone
        fPtr.dfs = other.fPtr.dfs;
        break;
#endif  // APPLE_ICU_CHANGES
    case SYMPTR_NS:
        // Memory allocation failures are exposed in copyErrorTo()
        if (other.fPtr.ns != nullptr) {
            fPtr.ns = new NumberingSystem(*other.fPtr.ns);
        } else {
            fPtr.ns = nullptr;
        }
        break;
    }
}

void SymbolsWrapper::doMoveFrom(SymbolsWrapper &&src) {
    fType = src.fType;
    switch (fType) {
    case SYMPTR_NONE:
        // No action necessary
        break;
    case SYMPTR_DFS:
#if APPLE_ICU_CHANGES
// rdar:/
    case SYMPTR_DFS_SHALLOWCOPY:
#endif  // APPLE_ICU_CHANGES
        fPtr.dfs = src.fPtr.dfs;
        src.fPtr.dfs = nullptr;
        break;
    case SYMPTR_NS:
        fPtr.ns = src.fPtr.ns;
        src.fPtr.ns = nullptr;
        break;
    }
}

void SymbolsWrapper::doCleanup() {
    switch (fType) {
    case SYMPTR_NONE:
        // No action necessary
        break;
    case SYMPTR_DFS:
        delete fPtr.dfs;
        break;
#if APPLE_ICU_CHANGES
// rdar:/
    case SYMPTR_DFS_SHALLOWCOPY: // Apple rdar://49955427
        // No action necessary
        break;
#endif  // APPLE_ICU_CHANGES
    case SYMPTR_NS:
        delete fPtr.ns;
        break;
    }
}

bool SymbolsWrapper::isDecimalFormatSymbols() const {
    return fType == SYMPTR_DFS;
}

bool SymbolsWrapper::isNumberingSystem() const {
    return fType == SYMPTR_NS;
}

const DecimalFormatSymbols *SymbolsWrapper::getDecimalFormatSymbols() const {
    U_ASSERT(fType == SYMPTR_DFS);
    return fPtr.dfs;
}

const NumberingSystem *SymbolsWrapper::getNumberingSystem() const {
    U_ASSERT(fType == SYMPTR_NS);
    return fPtr.ns;
}

#if APPLE_ICU_CHANGES
// rdar:/
// Apple rdar://49955427
void SymbolsWrapper::setDFSShallowCopy(UBool shallow) {
    if (shallow) {
        if (fType == SYMPTR_DFS) {
            fType = SYMPTR_DFS_SHALLOWCOPY;
        }
    } else if (fType == SYMPTR_DFS_SHALLOWCOPY) {
        fType = SYMPTR_DFS;
    }
}
#endif  // APPLE_ICU_CHANGES

#endif /* #if !UCONFIG_NO_FORMATTING */
