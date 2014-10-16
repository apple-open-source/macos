import os
from numpy.distutils.ccompiler import simple_version_match
from numpy.distutils.fcompiler import FCompiler

compilers = ['F2CCompiler']

class F2CCompiler(FCompiler):

    compiler_type = 'f2c'
    description = 'f2c (Fortran to C Translator)'
    # ex:
    # f2c (Fortran to C Translator) version 20100827.
    version_match = simple_version_match(
                      start=r'.* f2c \(Fortran to C Translator\) version ', pat=r'\d+')

    archflags = []
    if 'ARCHFLAGS' in os.environ:
        archflags = os.environ['ARCHFLAGS'].split()
    executables = {
        'version_cmd'  : ["<F77>", "-v"],
        'compiler_f77' : ["f77"],
        'linker_so'    : ["cc", "-bundle", "-undefined dynamic_lookup"] + archflags,
        'archiver'     : ["ar", "-cr"],
        'ranlib'       : ["ranlib"]
        }
    module_include_switch = '-I'
    libraries = ['f2c']

    def get_flags_opt(self):
        return ['-g','-Os']
    def get_flags_debug(self):
        return ['-g']

if __name__ == '__main__':
    from distutils import log
    log.set_verbosity(2)
    from numpy.distutils.fcompiler import new_fcompiler
    compiler = new_fcompiler(compiler='f2c')
    compiler.customize()
    print(compiler.get_version())
