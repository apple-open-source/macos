import test_names_parser as parser
import unittest

class TestExtractNamesFromCHeader(unittest.TestCase):
    def help_test_parser(self, c_header, expected):
        actual = parser.extract_test_names_from_c_header(c_header)
        self.assertEqual(actual, expected)

    def test_integration(self):
        c_header = "DEFINE_TEST(test1)\n \
        //DEFINE_TEST(test2)\n \
        DEFINE_TEST(test3)\n\
        /*DEFINE_TEST(test4)\nDEFINE_TEST(test5)\nDEFINE_TEST(test6)\n*/DEFINE_TEST(test7)"
        expected = ["test1", "test3", "test7"]
        self.help_test_parser(c_header, expected)

    # def test_integration_manually(self):
    #     with open("./libarchive/libarchive/test/list.h", 'r') as file:
    #         c_header = file.read()
    #         file.close
    #     test_names = parser.extract_test_names_from_c_header(c_header)
    #     exp_length = 519
    #     self.assertEqual(len(test_names),exp_length)


class TestRemoveCommentsFromCHeader(unittest.TestCase):
    def help_test_parser(self, c_header, expected):
        actual = parser.remove_comments_from_c_header(c_header)
        self.assertEqual(actual, expected)
    
    def test_onlyLineComment_returnEmpty(self):
        self.help_test_parser(c_header = "//Line Comment\t\n", expected = "")

    def test_onlyBlockComment_returnEmpty(self):
        self.help_test_parser(c_header = "/*Block\nBlock\n*/", expected = "")

    def test_onlyLineAndBlockComment_returnEmpty(self):
        self.help_test_parser(c_header = "//Line Comment\t\n/*Block\nBlock\n*/", expected = "")

    def test_lineAndBlockCommentsWithCode_returnCode(self):
        self.help_test_parser(c_header = "Code//Line Comment\t\nCode\n/*Block\nBlock\n*/", expected = "CodeCode\n")


class TestRemoveLineCommentFromCHeader(unittest.TestCase):
    def help_test_parser(self, c_header, expected):
        actual = parser.remove_line_comments_from_c_header(c_header)
        self.assertEqual(actual, expected)

    def test_empty_returnEmpty(self):
        self.help_test_parser(c_header =  "", expected = "")

    def test_emptyComment_returnEmpty(self):
        self.help_test_parser(c_header =  "//\n", expected = "")

    def test_onlyRegularComment_returnEmpty(self):
        self.help_test_parser(c_header = "//A \t\n", expected = "")

    def test_commentInSameLineWithCode_returnCode(self):
        self.help_test_parser(c_header = "Code //Comment\n", expected = "Code ")
    
    def test_commentInDifferentLineWithCode_returnCode(self):
        self.help_test_parser(c_header = "Code\n//Comment\n//Comment\nCode", expected = "Code\nCode")

class TestRemoveBlockCommentFromCHeader(unittest.TestCase):
    def help_test_parser(self, c_header, expected):
        actual = parser.remove_block_comments_from_c_header(c_header)
        self.assertEqual(actual, expected)
    
    def test_empty_returnEmpty(self):
        self.help_test_parser(c_header =  "", expected = "")

    def test_emptyComment_returnEmpty(self):
        self.help_test_parser(c_header =  "/**/", expected = "")

    def test_regularWordComment_returnEmpty(self):
        self.help_test_parser(c_header =  "/*ABC\t */", expected = "")

    def test_newLineComment_returnEmpty(self):
        self.help_test_parser(c_header =  "/*ABC\n*/", expected = "")

    def test_multipleLineComment_returnEmpty(self):
        self.help_test_parser(c_header =  "/*ABC\nABC\n*/", expected = "")
    
    def test_multipleLineCommentWithCode_returnCode(self):
        self.help_test_parser(c_header =  "Code\n/*ABC\nABC\n*/Code", expected = "Code\nCode")

    def test_mixingBlockCommentAndCodeReturnCode(self):
        self.help_test_parser(c_header =  "/*Block*/Code/*Block*/", expected = "Code")


class TestExtractTestNameFromCHeaderNoComments(unittest.TestCase):
    def help_test_parser(self, c_header, expected):
        actual = parser.extract_test_names_from_c_header_without_comments(c_header)
        self.assertEqual(actual, expected)

    def test_empty_returnEmpty(self):
        self.help_test_parser(c_header =  "", expected = [])

    def test_emptyDefine_returnEmpty(self):
        self.help_test_parser(c_header =  "DEFINE_TEST()", expected = [])

    def test_singleTestDifine_returnSingle(self):
        self.help_test_parser(c_header =  "DEFINE_TEST(test)", expected = ["test"])

    def test_manyTestDifine_returnMany(self):
        self.help_test_parser(c_header =  "DEFINE_TEST(test1)\nDEFINE_TEST(test2)", expected = ["test1","test2"])

if __name__ == '__main__':
    unittest.main()