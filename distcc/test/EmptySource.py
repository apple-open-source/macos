import testdistcc

class EmptySource_Case(testdistcc.Compilation_Case):
    """Check compilation of empty source file

    It must be treated as preprocessed source, otherwise cpp will
    insert a # line, which will give a false pass.  """
    
    def source(self):
        return ''

    def runTest(self):
        self.compile()

    def compile(self):
        self.runCmd("distcc cc -c %s" % self.sourceFilename())

    def sourceFilename(self):
        return "testtmp.i"
    
