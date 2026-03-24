// © 2025 and later: Unicode, Inc. and others.
// License & terms of use: https://www.unicode.org/copyright.html

// utfstringtest.cpp
// created: 2025jul18 Markus W. Scherer

#include <string>

// Test header-only ICU C++ APIs. Do not use other ICU C++ APIs.
// Non-default configuration:
#define U_SHOW_CPLUSPLUS_API 0
// Default configuration:
#define U_SHOW_CPLUSPLUS_HEADER_API 1 // Apple change (rdar://166411027): make sure we turn this on for this test

#include "unicode/utypes.h"
#include "unicode/utfstring.h"
#include "intltest.h"

class UTFStringTest : public IntlTest {
public:
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) override {
        if (exec) { logln("TestSuite UTFStringTest: "); }
        TESTCASE_AUTO_BEGIN;

        TESTCASE_AUTO(testAppendOrFFFD);
        TESTCASE_AUTO(testAppendUnsafe);
        TESTCASE_AUTO(testEncodeOrFFFD);
        TESTCASE_AUTO(testEncodeUnsafe);

        TESTCASE_AUTO_END;
    }

    void testAppendOrFFFD();
    void testAppendUnsafe();
    void testEncodeOrFFFD();
    void testEncodeUnsafe();
};

extern IntlTest *createUTFStringTest() {
    return new UTFStringTest();
}

#define CHARS(s) reinterpret_cast<const char *>(s)

void UTFStringTest::testAppendOrFFFD() {
    using icu::header::utfstring::appendOrFFFD;
    {
        std::string s;
        s.push_back(u8'_');
        appendOrFFFD(s, U'4').push_back(u8'|');
        appendOrFFFD(s, U'ç').push_back(u8'|');
        appendOrFFFD(s, U'カ').push_back(u8'|');
        appendOrFFFD(s, 0xd800).push_back(u8'|');
        appendOrFFFD(s, 0xdfff).push_back(u8'|');
        appendOrFFFD(s, U'４').push_back(u8'|');
        appendOrFFFD(s, U'🚴').push_back(u8'|');
        appendOrFFFD(s, 0x110000).push_back(u8'|');
        appendOrFFFD(s, -1).push_back(u8'.');
        assertTrue("string", s == CHARS(u8"_4|ç|カ|\uFFFD|\uFFFD|４|🚴|\uFFFD|\uFFFD."));
    }
#if U_CPLUSPLUS_VERSION >= 20
    {
        std::u8string s;
        s.push_back(u8'_');
        appendOrFFFD(s, U'4').push_back(u8'|');
        appendOrFFFD(s, U'ç').push_back(u8'|');
        appendOrFFFD(s, U'カ').push_back(u8'|');
        appendOrFFFD(s, 0xd800).push_back(u8'|');
        appendOrFFFD(s, 0xdfff).push_back(u8'|');
        appendOrFFFD(s, U'４').push_back(u8'|');
        appendOrFFFD(s, U'🚴').push_back(u8'|');
        appendOrFFFD(s, 0x110000).push_back(u8'|');
        appendOrFFFD(s, -1).push_back(u8'.');
        assertTrue("string", s == u8"_4|ç|カ|\uFFFD|\uFFFD|４|🚴|\uFFFD|\uFFFD.");
    }
#endif
    {
        std::u16string s;
        s.push_back(u'_');
        appendOrFFFD(s, U'4').push_back(u'|');
        appendOrFFFD(s, U'ç').push_back(u'|');
        appendOrFFFD(s, U'カ').push_back(u'|');
        appendOrFFFD(s, 0xd800).push_back(u'|');
        appendOrFFFD(s, 0xdfff).push_back(u'|');
        appendOrFFFD(s, U'４').push_back(u'|');
        appendOrFFFD(s, U'🚴').push_back(u'|');
        appendOrFFFD(s, 0x110000).push_back(u'|');
        appendOrFFFD(s, -1).push_back(u'.');
        assertEquals("u16string", u"_4|ç|カ|\uFFFD|\uFFFD|４|🚴|\uFFFD|\uFFFD.", s);
    }
    {
        std::u32string s;
        s.push_back(u'_');
        appendOrFFFD(s, U'4').push_back(u'|');
        appendOrFFFD(s, U'ç').push_back(u'|');
        appendOrFFFD(s, U'カ').push_back(u'|');
        appendOrFFFD(s, 0xd800).push_back(u'|');
        appendOrFFFD(s, 0xdfff).push_back(u'|');
        appendOrFFFD(s, U'４').push_back(u'|');
        appendOrFFFD(s, U'🚴').push_back(u'|');
        appendOrFFFD(s, 0x110000).push_back(u'|');
        appendOrFFFD(s, -1).push_back(u'.');
        assertTrue("u32string", s == U"_4|ç|カ|\uFFFD|\uFFFD|４|🚴|\uFFFD|\uFFFD.");
    }
}

void UTFStringTest::testAppendUnsafe() {
    using icu::header::utfstring::appendUnsafe;
    {
        std::string s;
        s.push_back(u8'_');
        appendUnsafe(s, U'4').push_back(u8'|');
        appendUnsafe(s, U'ç').push_back(u8'|');
        appendUnsafe(s, U'カ').push_back(u8'|');
        appendUnsafe(s, U'４').push_back(u8'|');
        appendUnsafe(s, U'🚴').push_back(u8'.');
        assertTrue("string", s == CHARS(u8"_4|ç|カ|４|🚴."));
    }
#if U_CPLUSPLUS_VERSION >= 20
    {
        std::u8string s;
        s.push_back(u8'_');
        appendUnsafe(s, U'4').push_back(u8'|');
        appendUnsafe(s, U'ç').push_back(u8'|');
        appendUnsafe(s, U'カ').push_back(u8'|');
        appendUnsafe(s, U'４').push_back(u8'|');
        appendUnsafe(s, U'🚴').push_back(u8'.');
        assertTrue("string", s == u8"_4|ç|カ|４|🚴.");
    }
#endif
    {
        std::u16string s;
        s.push_back(u'_');
        appendUnsafe(s, U'4').push_back(u'|');
        appendUnsafe(s, U'ç').push_back(u'|');
        appendUnsafe(s, U'カ').push_back(u'|');
        appendUnsafe(s, U'４').push_back(u'|');
        appendUnsafe(s, U'🚴').push_back(u'.');
        assertEquals("u16string", u"_4|ç|カ|４|🚴.", s);
    }
    {
        std::u32string s;
        s.push_back(u'_');
        appendUnsafe(s, U'4').push_back(u'|');
        appendUnsafe(s, U'ç').push_back(u'|');
        appendUnsafe(s, U'カ').push_back(u'|');
        appendUnsafe(s, U'４').push_back(u'|');
        appendUnsafe(s, U'🚴').push_back(u'.');
        assertTrue("u32string", s == U"_4|ç|カ|４|🚴.");
    }
}

void UTFStringTest::testEncodeOrFFFD() {
    using icu::header::utfstring::encodeOrFFFD;
    {
        assertTrue("string 4", encodeOrFFFD<std::string>(U'4') == CHARS(u8"4"));
        assertTrue("string ced", encodeOrFFFD<std::string>(U'ç') == CHARS(u8"ç"));
        assertTrue("string ka", encodeOrFFFD<std::string>(U'カ') == CHARS(u8"カ"));
        assertTrue("string D800", encodeOrFFFD<std::string>(0xd800) == CHARS(u8"\uFFFD"));
        assertTrue("string DFFF", encodeOrFFFD<std::string>(0xdfff) == CHARS(u8"\uFFFD"));
        assertTrue("string fw4", encodeOrFFFD<std::string>(U'４') == CHARS(u8"４"));
        assertTrue("string bike", encodeOrFFFD<std::string>(U'🚴') == CHARS(u8"🚴"));
        assertTrue("string high", encodeOrFFFD<std::string>(0x110000) == CHARS(u8"\uFFFD"));
        assertTrue("string neg", encodeOrFFFD<std::string>(-1) == CHARS(u8"\uFFFD"));
    }
#if U_CPLUSPLUS_VERSION >= 20
    {
        assertTrue("u8string 4", encodeOrFFFD<std::u8string>(U'4') == u8"4");
        assertTrue("u8string ced", encodeOrFFFD<std::u8string>(U'ç') == u8"ç");
        assertTrue("u8string ka", encodeOrFFFD<std::u8string>(U'カ') == u8"カ");
        assertTrue("u8string D800", encodeOrFFFD<std::u8string>(0xd800) == u8"\uFFFD");
        assertTrue("u8string DFFF", encodeOrFFFD<std::u8string>(0xdfff) == u8"\uFFFD");
        assertTrue("u8string fw4", encodeOrFFFD<std::u8string>(U'４') == u8"４");
        assertTrue("u8string bike", encodeOrFFFD<std::u8string>(U'🚴') == u8"🚴");
        assertTrue("u8string high", encodeOrFFFD<std::u8string>(0x110000) == u8"\uFFFD");
        assertTrue("u8string neg", encodeOrFFFD<std::u8string>(-1) == u8"\uFFFD");
    }
#endif
    {
        assertTrue("u16string 4", encodeOrFFFD<std::u16string>(U'4') == u"4");
        assertTrue("u16string ced", encodeOrFFFD<std::u16string>(U'ç') == u"ç");
        assertTrue("u16string ka", encodeOrFFFD<std::u16string>(U'カ') == u"カ");
        assertTrue("u16string D800", encodeOrFFFD<std::u16string>(0xd800) == u"\uFFFD");
        assertTrue("u16string DFFF", encodeOrFFFD<std::u16string>(0xdfff) == u"\uFFFD");
        assertTrue("u16string fw4", encodeOrFFFD<std::u16string>(U'４') == u"４");
        assertTrue("u16string bike", encodeOrFFFD<std::u16string>(U'🚴') == u"🚴");
        assertTrue("u16string high", encodeOrFFFD<std::u16string>(0x110000) == u"\uFFFD");
        assertTrue("u16string neg", encodeOrFFFD<std::u16string>(-1) == u"\uFFFD");
    }
    {
        assertTrue("u32string 4", encodeOrFFFD<std::u32string>(U'4') == U"4");
        assertTrue("u32string ced", encodeOrFFFD<std::u32string>(U'ç') == U"ç");
        assertTrue("u32string ka", encodeOrFFFD<std::u32string>(U'カ') == U"カ");
        assertTrue("u32string D800", encodeOrFFFD<std::u32string>(0xd800) == U"\uFFFD");
        assertTrue("u32string DFFF", encodeOrFFFD<std::u32string>(0xdfff) == U"\uFFFD");
        assertTrue("u32string fw4", encodeOrFFFD<std::u32string>(U'４') == U"４");
        assertTrue("u32string bike", encodeOrFFFD<std::u32string>(U'🚴') == U"🚴");
        assertTrue("u32string high", encodeOrFFFD<std::u32string>(0x110000) == U"\uFFFD");
        assertTrue("u32string neg", encodeOrFFFD<std::u32string>(-1) == U"\uFFFD");
    }
}

void UTFStringTest::testEncodeUnsafe() {
    using icu::header::utfstring::encodeUnsafe;
    {
        assertTrue("string 4", encodeUnsafe<std::string>(U'4') == CHARS(u8"4"));
        assertTrue("string ced", encodeUnsafe<std::string>(U'ç') == CHARS(u8"ç"));
        assertTrue("string ka", encodeUnsafe<std::string>(U'カ') == CHARS(u8"カ"));
        assertTrue("string fw4", encodeUnsafe<std::string>(U'４') == CHARS(u8"４"));
        assertTrue("string bike", encodeUnsafe<std::string>(U'🚴') == CHARS(u8"🚴"));
    }
#if U_CPLUSPLUS_VERSION >= 20
    {
        assertTrue("u8string 4", encodeUnsafe<std::u8string>(U'4') == u8"4");
        assertTrue("u8string ced", encodeUnsafe<std::u8string>(U'ç') == u8"ç");
        assertTrue("u8string ka", encodeUnsafe<std::u8string>(U'カ') == u8"カ");
        assertTrue("u8string fw4", encodeUnsafe<std::u8string>(U'４') == u8"４");
        assertTrue("u8string bike", encodeUnsafe<std::u8string>(U'🚴') == u8"🚴");
    }
#endif
    {
        assertTrue("u16string 4", encodeUnsafe<std::u16string>(U'4') == u"4");
        assertTrue("u16string ced", encodeUnsafe<std::u16string>(U'ç') == u"ç");
        assertTrue("u16string ka", encodeUnsafe<std::u16string>(U'カ') == u"カ");
        assertTrue("u16string fw4", encodeUnsafe<std::u16string>(U'４') == u"４");
        assertTrue("u16string bike", encodeUnsafe<std::u16string>(U'🚴') == u"🚴");
    }
    {
        assertTrue("u32string 4", encodeUnsafe<std::u32string>(U'4') == U"4");
        assertTrue("u32string ced", encodeUnsafe<std::u32string>(U'ç') == U"ç");
        assertTrue("u32string ka", encodeUnsafe<std::u32string>(U'カ') == U"カ");
        assertTrue("u32string fw4", encodeUnsafe<std::u32string>(U'４') == U"４");
        assertTrue("u32string bike", encodeUnsafe<std::u32string>(U'🚴') == U"🚴");
    }
}
