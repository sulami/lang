__version__ = '0.1.0'

import subprocess
import sys
import tempfile

from llvmlite import ir
import llvmlite.binding as llvm

# Types
T_VOID = ir.VoidType()
T_VOID_PTR = ir.IntType(8).as_pointer()
T_I64 = ir.IntType(64)
T_I32 = ir.IntType(32)
T_I8 = ir.IntType(8)
T_BOOL = ir.IntType(1)
T_F32 = ir.FloatType()
T_F64 = ir.DoubleType()

# Values
NULL_PTR = T_I32(0).inttoptr(T_VOID_PTR)

def init_llvm():
    """Setup the LLVM core."""
    llvm.initialize()
    llvm.initialize_native_target()
    llvm.initialize_native_asmprinter()

def deinit_llvm():
    """Shutdown the LLVM core."""
    # XXX this currently breaks, and I'm unclear if we need it.
    llvm.shutdown()

def compile_execution_engine():
    """
    Compile an ExecutionEngine suitable for JIT code generation on
    the host CPU.  The engine is reusable for an arbitrary number of
    modules.
    """
    # Compile a target machine representing the host
    target = llvm.Target.from_default_triple()
    target_machine = target.create_target_machine()
    # And an execution engine with an empty backing module
    backing_mod = llvm.parse_assembly("")
    engine = llvm.create_mcjit_compiler(backing_mod, target_machine)
    return engine

def compile_ir(engine, llvm_ir):
    """
    Compile the LLVM module string with the given engine.
    The compiled module object is returned.
    """
    mod = llvm.parse_assembly(llvm_ir)
    mod.verify()
    # Now add the mod and make sure it is ready for execution
    engine.add_module(mod)
    engine.finalize_object()
    engine.run_static_constructors()
    return mod

def load_external_function(module, type_spec, name):
    return ir.Function(module, type_spec, name=name)

def store_value(env, value):
    """
    Compiles a set of instructions to store a value and returns an
    anonymised pointer to it.
    """
    ptr = env.builder.alloca(value.type)
    env.builder.store(value, ptr)
    return env.builder.bitcast(ptr, T_VOID_PTR)

def store_string(env, string):
    """
    Compiles a set of instructions to store a string and returns an
    anonymised pointer to it. Properly deals with unicode.
    """
    string += '\0'
    byte_array = bytearray(string.encode('utf8'))
    s_type = ir.ArrayType(T_I8, len(byte_array))
    s = ir.Constant(s_type, byte_array)
    return store_value(env, s)

def call_print_ptr(env, ptr):
    env.call("printf", [ptr])

def call_print_str(env, string):
    ptr = store_string(env, string)
    call_print_ptr(env, ptr)

def call_read_file(env, path, mode):
    path_arg = store_string(env, path)
    mode_arg = store_string(env, mode)
    return env.call("read_file", [path_arg, mode_arg])

class Environment:
    def __init__(self, module, fn):
        self.module = module
        self.fn = fn
        self.lib = dict()
        self.blocks = dict()
        self.builders = dict()
        self.current_block = None
        self.builder = None

    def switch_block(self, name):
        block = self.blocks[name]
        self.current_block = name
        self.builder = self.builders[name]
        self.builder.position_at_end(block)
        return block

    def add_block(self, name):
        block = self.fn.append_basic_block(name)
        self.blocks[name] = block
        self.builders[name] = ir.IRBuilder(block)
        return block

    def declare_fn(self, name, return_type, arg_types, **kwargs):
        self.lib[name] = ir.Function(
            self.module,
            ir.FunctionType(return_type, arg_types, **kwargs),
            name=name
        )

    def call(self, name, args):
        return self.builders[self.current_block].call(
            self.lib[name], args)

def compile_loop(env):
    loop_block = env.add_block('loop')
    exit_block = env.add_block('exit')

    env.builder.branch(loop_block)

    env.switch_block('loop')
    call_print_str(env, 'looping...\n')
    condition = env.call("random_bool", [])
    env.builder.cbranch(condition, loop_block, exit_block)

    env.switch_block('exit')
    call_print_str(env, 'done looping\n')

def compile_cons(env):
    l = NULL_PTR
    for c in ['first', 'second', 'third']:
        v = store_string(env, c)
        l = env.call('cons', [v, l])

    car = env.call('car', [l])
    cadr = env.call('car', [env.call('cdr', [l])])
    caddr = env.call('car', [env.call('cdr', [env.call('cdr', [l])])])
    call_print_ptr(env, car)
    call_print_str(env, '\n')
    call_print_ptr(env, cadr)
    call_print_str(env, '\n')
    call_print_ptr(env, caddr)
    call_print_str(env, '\n')

def compile_main_func():
    module = ir.Module(name='main')
    module.triple = llvm.get_default_triple()

    f_type = ir.FunctionType(T_I32, [])
    func = ir.Function(module, f_type, name="main")

    env = Environment(module, func)
    block = env.add_block('entry')
    env.switch_block('entry')

    # C stdlib
    env.declare_fn("printf", T_VOID, [T_VOID_PTR], var_arg=True)

    # libnebula
    env.declare_fn("init_nebula", T_VOID, [])
    env.declare_fn("read_file", ir.PointerType(T_I8), [T_VOID_PTR, T_VOID_PTR])
    env.declare_fn("random_bool", T_BOOL, [])
    env.declare_fn("cons", T_VOID_PTR, [T_VOID_PTR, T_VOID_PTR])
    env.declare_fn("car", T_VOID_PTR, [T_VOID_PTR])
    env.declare_fn("cdr", T_VOID_PTR, [T_VOID_PTR])

    env.call("init_nebula", [])

    call_print_str(env, "Printing a long string here.\nWith unicode: Hélène ヘレンちゃんは可愛いですね\n")

    call_print_str(env, "Here is some file content:\n")
    file_contents = call_read_file(env, "README.rst", "r")
    call_print_ptr(env, file_contents)

    call_print_str(env, "Here is a loop:\n")
    compile_loop(env)

    call_print_str(env, "Here is a linked list:\n")
    compile_cons(env)

    env.builder.ret(ir.Constant(T_I32, 0))

    return module

def compile_nebula():
    print("Compiling nebula...")
    # TODO don't recompile if it's current
    subprocess.run(["cc",
                    "-Wall",
                    "-fpic",
                    "-c",
                    "-o", "nebula.o",
                    "lang/nebula.c"])

def compile_binary(module):
    with tempfile.NamedTemporaryFile(mode='w', suffix=".ll") as tmp_llvm_ir:
        with tempfile.NamedTemporaryFile(mode='wb', suffix=".o") as tmp_obj:
            # Compile the object
            print("Compiling LLVM IR...")
            tmp_llvm_ir.write(str(module))
            tmp_llvm_ir.flush()
            subprocess.run(["/usr/local/opt/llvm/bin/llc",
                            "--filetype=obj",
                            "-o=" + tmp_obj.name,
                            tmp_llvm_ir.name],
                           check=True)
            # Link & compile the binary
            print("Compiling binary...")
            subprocess.run(["cc",
                            "-Wall",
                            "-o", "out",
                            # "-O3",
                            tmp_obj.name,
                            "nebula.o",],
                           check=True)

def compile_if(env, expression, depth=0):
    assert 3 == len(expression), 'if takes exactly 3 arguments'
    a, b, c = expression
    condition = compile_expression(env, a, depth=depth+1)
    with env.builder.if_else(condition) as (then, otherwise):
        with then:
            compile_expression(env, b, depth=depth+1)
        with otherwise:
            compile_expression(env, c, depth=depth+1)

def compile_function_call(env, expression, depth=0):
    if 'if' == expression[0]:
        return compile_if(env, expression[1:], depth=depth+1)

    # else: function call
    # TODO currently only static function names
    # TODO we might want to cast argument pointers to the correct
    # types if we know them
    args = []
    for exp in expression[1:]:
        args += [compile_expression(env, exp, depth=depth+1)]
    return env.call(expression[0], args)

def compile_constant_string(env, expression):
    # Strip quotation marks
    # Convert escape sequences
    # XXX eval is the hacky way of accomplishing this.
    # It's fine because we can trust our input.
    return store_string(env, eval(expression))

def compile_constant_int(env, expression):
    return ir.Constant(T_I32, int(expression))

def compile_constant_float(env, expression):
    # FIXME smaller floats aren't passed corretly to printf, resulting
    # in misread values.
    return ir.Constant(T_F64, float(expression))

def compile_constant_bool(env, expression):
    return ir.Constant(T_BOOL, 'true' == expression)

def compile_expression(env, expression, depth=0):
    print(' ' * (depth + 1) + 'Compiling expression: ' + str(expression))

    if isinstance(expression, list):
        # function call
        return compile_function_call(env, expression, depth)
    elif expression in ['true', 'false']:
        return compile_constant_bool(env, expression)
    elif '"' == expression[0]:
        # constant string
        return compile_constant_string(env, expression)
    elif '.' in expression:
        # constant float
        return compile_constant_float(env, expression)
    else:
        # constant int
        return compile_constant_int(env, expression)

def compile_ast(ast):
    print('Compiling application...')
    module = ir.Module(name='main')
    module.triple = llvm.get_default_triple()

    f_type = ir.FunctionType(T_I32, [])
    func = ir.Function(module, f_type, name="main")

    env = Environment(module, func)
    block = env.add_block('entry')
    env.switch_block('entry')

    # C stdlib
    env.declare_fn("printf", T_VOID, [T_VOID_PTR], var_arg=True)

    # libnebula
    env.declare_fn("init_nebula", T_VOID, [])
    env.declare_fn("read_file", ir.PointerType(T_I8), [T_VOID_PTR, T_VOID_PTR])
    env.declare_fn("random_bool", T_BOOL, [])
    env.declare_fn("cons", T_VOID_PTR, [T_VOID_PTR, T_VOID_PTR])
    env.declare_fn("car", T_VOID_PTR, [T_VOID_PTR])
    env.declare_fn("cdr", T_VOID_PTR, [T_VOID_PTR])

    env.call("init_nebula", [])

    for expression in ast:
        compile_expression(env, expression)

    env.builder.ret(ir.Constant(T_I32, 0))
    return module

def parse(unparsed, depth=0):
    """
    Parse nested S-expressions. Raises for unbalanced parens.
    Returns a tuple of (unparsed, AST).
    The top level is always a list.
    """
    ast = []
    inside_comment = False
    inside_word = False
    inside_string = False
    while unparsed:
        c = unparsed[0]
        unparsed = unparsed[1:]
        if inside_comment:
            if '\n' == c:
                inside_comment = False
            else:
                continue
        elif ';' == c:
            inside_comment = True
        elif '"' == c:
            if inside_string:
                ast[-1] += c
            else:
                ast += [c]
            inside_string = not inside_string
        elif inside_string:
            ast[-1] += c
        elif '(' == c:
            unparsed, inner_ast = parse(unparsed, depth=depth+1)
            ast += [inner_ast]
        elif ')' == c:
            if depth == 0:
                raise Exception('Unexpected ) while parsing')
            return unparsed, ast
        elif c.isspace():
            inside_word = False
        elif inside_word:
            ast[-1] += c
        else:
            inside_word = True
            ast += c
    if depth != 0:
        raise Exception('Unexpected EOF while parsing')
    return unparsed, ast

def main():
    init_llvm()
    compile_nebula()
    engine = compile_execution_engine()

    if 1 == len(sys.argv):
        # run built-in tests
        main_mod = compile_main_func()
        # print(main_mod)
        main_mod = compile_ir(engine, str(main_mod))
        # print(main_mod)
        compile_binary(main_mod)
        return

    ast = None
    with open(sys.argv[1], 'r') as fp:
        _, ast = parse(fp.read())
    # print(ast)
    main_mod = compile_ast(ast)
    # print(main_mod)
    main_mod = compile_ir(engine, str(main_mod))
    # print(main_mod)
    compile_binary(main_mod)

    # this works, but I don't know how to call it then.
    # str_mod = compile_str_func("say_hello", "hello, world!\n")
    # str_mod = compile_ir(engine, str(str_mod))
    # main_mod.link_in(str_mod, preserve=True)
    print('Compiled successfully!')

if __name__ == '__main__':
    main()
