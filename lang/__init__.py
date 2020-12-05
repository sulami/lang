import sys

from lang.compiler import compile_main, compile_ir
from lang.debug import debug
from lang.llvm import init_llvm, compile_execution_engine
from lang.parser import parse

__version__ = '0.1.0'

def main():
    init_llvm()
    engine = compile_execution_engine()
    ast = None
    source_file = sys.argv[1]
    with open(source_file, 'r') as fp:
        _, ast = parse(fp.read())
    debug(ast)
    main_mod = compile_main(ast)
    debug(main_mod)
    # debug(main_mod)
    with open(source_file.split('.')[0].split('/')[-1] + '.ll', 'w') as llvm_ir:
        llvm_ir.write(str(main_mod))
        llvm_ir.flush()
    main_mod = compile_ir(engine, str(main_mod))

if __name__ == '__main__':
    main()
