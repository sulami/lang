__version__ = '0.1.0'

import re
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

def store_value(env, value, string=False):
    """
    Compiles a set of instructions to store a value and returns an
    anonymised pointer to it.
    """
    ptr = env.builder.alloca(value.type)
    env.builder.store(value, ptr)
    return env.builder.bitcast(ptr, T_VOID_PTR)

def make_string(value):
    """
    Returns a string constant. Encodes unicode & appends a zero byte.
    """
    value += '\0'
    byte_array = bytearray(value.encode('utf8'))
    value_type = ir.ArrayType(T_I8, len(byte_array))
    return ir.Constant(value_type, byte_array)

class Environment:
    def __init__(self, module, fn):
        self.module = module
        self.fn = fn
        self.lib = dict()
        self.blocks = dict()
        self.builders = dict()
        self.current_block = None
        self.builder = None
        self.scopes = []

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
        self.lib['c/' + name] = ir.Function(
            self.module,
            ir.FunctionType(return_type, arg_types, **kwargs),
            name=name
        )

    def call(self, name, args):
        return self.builders[self.current_block].call(
            self.lib[name], args)

# def compile_loop(env):
#     loop_block = env.add_block('loop')
#     exit_block = env.add_block('exit')

#     env.builder.branch(loop_block)

#     env.switch_block('loop')
#     call_print_str(env, 'looping...\n')
#     condition = env.call("random_bool", [])
#     env.builder.cbranch(condition, loop_block, exit_block)

#     env.switch_block('exit')
#     call_print_str(env, 'done looping\n')

# def compile_cons(env):
#     l = NULL_PTR
#     for c in ['first', 'second', 'third']:
#         v = store_value(env, make_string(c))
#         l = env.call('cons', [v, l])

#     car = env.call('car', [l])
#     cadr = env.call('car', [env.call('cdr', [l])])
#     caddr = env.call('car', [env.call('cdr', [env.call('cdr', [l])])])
#     call_print_ptr(env, car)
#     call_print_str(env, '\n')
#     call_print_ptr(env, cadr)
#     call_print_str(env, '\n')
#     call_print_ptr(env, caddr)
#     call_print_str(env, '\n')

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
    assert 4 == len(expression), 'if takes exactly 3 arguments'
    _, a, b, c = expression
    condition = compile_expression(env, a, depth=depth+1)
    with env.builder.if_else(condition) as (then, otherwise):
        with then:
            compile_expression(env, b, depth=depth+1)
        with otherwise:
            compile_expression(env, c, depth=depth+1)

def compile_native_op(env, expression, depth=0):
    # XXX currently only 2-arity
    assert 3 == len(expression), expression[0] + ' takes exactly 2 arguments'
    a, b, c = expression
    lhs = compile_expression(env, b, depth=depth+1)
    rhs = compile_expression(env, c, depth=depth+1)
    if '+' == a:
        return env.builder.add(lhs, rhs)
    if '-' == a:
        return env.builder.sub(lhs, rhs)
    if '*' == a:
        return env.builder.mul(lhs, rhs)
    if '/' == a:
        return env.builder.sdiv(lhs, rhs)

def compile_box(env, expression, depth=0):
    assert 2 == len(expression), 'box takes exactly 1 argument'
    return store_value(env, compile_expression(env, expression[1], depth=depth+1))

def compile_progn(env, expression, depth=0):
    retval = None  # TODO should probably be nil instead
    for exp in expression:
        retval = compile_expression(env, exp, depth=depth+1)
    return retval

def compile_let(env, expression, depth=0):
    assert 3 <= len(expression), 'let takes at least 2 arguments'

    bindings = expression[1]
    new_scope = dict()
    for binding in bindings:
        assert 2 == len(binding), 'let bindings must be exactly 2 elements'
        k, v = binding
        new_scope[k] = compile_expression(env, v, depth=depth+1)
    env.scopes.append(new_scope)

    body = expression[2:]
    retval = compile_progn(env, body, depth=depth+1)
    env.scopes.pop()
    return retval

def compile_function_call(env, expression, depth=0):
    if 'progn' == expression[0]:
        return compile_progn(env, expression[1:], depth=depth+1)

    if 'let' == expression[0]:
        return compile_let(env, expression, depth=depth+1)

    if 'box' == expression[0]:
        return compile_box(env, expression, depth=depth+1)

    if 'if' == expression[0]:
        return compile_if(env, expression, depth=depth+1)

    if expression[0] in ['+', '-', '*', '/']:
        return compile_native_op(env, expression, depth=depth+1)

    # else: function call
    # TODO currently only static function names
    # TODO we might want to cast argument pointers to the correct
    # types if we know them
    args = []
    for exp in expression[1:]:
        args += [compile_expression(env, exp, depth=depth+1)]
    fn = env.lib[expression[0]]
    return env.call(expression[0], args)

def compile_constant_string(env, expression):
    # Strip quotation marks
    # Convert escape sequences
    # XXX eval is the hacky way of accomplishing this.
    # It's fine because we can trust our input.
    return store_value(env, make_string(eval(expression)))

def compile_constant_int(env, expression):
    return ir.Constant(T_I32, int(expression))

def compile_constant_float(env, expression):
    # FIXME smaller floats aren't passed corretly to printf, resulting
    # in misread values.
    return ir.Constant(T_F64, float(expression))

def compile_constant_bool(env, expression):
    return ir.Constant(T_BOOL, 'true' == expression)

def compile_nil(env, expression):
    return ir.Constant(T_VOID_PTR, NULL_PTR)

def compile_symbol(env, expression):
    for scope in reversed(env.scopes):
        if expression in scope:
            return scope[expression]
    raise Exception('Could not find symbol ' + expression + ' in local scope')

def compile_expression(env, expression, depth=0):
    print(' ' * (depth + 1) + 'Compiling expression: ' + str(expression))

    if isinstance(expression, list):
        # function call
        return compile_function_call(env, expression, depth)
    elif 'nil' == expression:
        return compile_nil(env, expression)
    elif expression in ['true', 'false']:
        return compile_constant_bool(env, expression)
    elif '"' == expression[0]:
        # constant string
        return compile_constant_string(env, expression)
    elif re.match('^[0-9]+.[0-9]+$',  expression):
        # constant float
        return compile_constant_float(env, expression)
    elif re.match('^[0-9]+$',  expression):
        # constant int
        return compile_constant_int(env, expression)
    else:
        # symbol
        return compile_symbol(env, expression)

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
    env.declare_fn("alist", T_VOID_PTR, [T_VOID_PTR, T_VOID_PTR, T_VOID_PTR])
    env.declare_fn("aget", T_VOID_PTR, [T_VOID_PTR, T_VOID_PTR])
    env.declare_fn("unbox", T_VOID_PTR, [T_VOID_PTR])

    env.call("c/init_nebula", [])

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
