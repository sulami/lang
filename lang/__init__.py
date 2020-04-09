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

T_VALUE_STRUCT = ir.LiteralStructType([T_I32, T_VOID_PTR])
T_VALUE_STRUCT_PTR = T_VALUE_STRUCT.as_pointer()
# This is a union in C, but LLVM does not do unions, so it's a struct
# with a field the size of the largest union member, which we cast to
# the type we want.
T_PRIMITIVE = ir.LiteralStructType([T_I64])
T_PRIMITIVE_PTR = T_PRIMITIVE.as_pointer()

# Type mapping (from AST)
TYPES = {
    'void': T_VOID_PTR,
    'int': T_I32,
    'float': T_F64,
    'bool': T_BOOL,
    'string': T_VOID_PTR,
}

# Type mapping (for runtime values)
RUNTIME_TYPES = {
    'void': 0,
    'bool': 1,
    'int': 2,
    'float': 3,
    # T_STRING: 100,
}

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

def fq_block_name(fn, block):
    return fn.name + '__' + block.name

class Environment:
    def __init__(self, module, block):
        self.builder = ir.IRBuilder(block)
        self.lib = dict()
        self.blocks = {
            fq_block_name(self.builder.function, block): block,
        }
        self.scopes = [dict()]

    def add_fn(self, name, ret_type, arg_types):
        f_type = ir.FunctionType(T_VALUE_STRUCT_PTR, [T_VALUE_STRUCT_PTR for _ in arg_types])
        func = ir.Function(self.builder.module, f_type, name=name)
        self.lib[name] = func
        entry = self.add_block('entry', fn=func)
        return func

    def current_block_name(self):
        return fq_block_name(self.builder.function, self.builder.block)

    def add_block(self, name, fn=None):
        fn = fn or self.builder.function
        block = fn.append_basic_block(name)
        fqn = fq_block_name(fn, block)
        self.blocks[fqn] = block
        self.builder.position_at_start(block)
        return block

    def declare_fn(self, name, return_type, arg_types, **kwargs):
        self.lib['c/' + name] = ir.Function(
            self.builder.module,
            ir.FunctionType(return_type, arg_types, **kwargs),
            name=name
        )

    def unbox_value(self, val, out_type):
        val = self.builder.call(self.lib['c/unbox_value'], [val])
        if out_type:
            val = self.builder.bitcast(val, out_type.as_pointer())
        else:
            val = self.builder.bitcast(val, T_VOID_PTR)
        val = self.builder.load(val)
        return val

    def call(self, name, args):
        fn = self.lib[name]
        print('fn.name', fn.name)
        print('ftype', fn.ftype)
        print('fn.args', [a.type for a in fn.args])
        print('args', [getattr(a, 'type', 'No type') for a in args])

        unboxed_args = []
        if name.startswith('c/'):
            # external call, unbox
            for i in range(len(args)):
                arg = args[i]
                fn_arg = fn.args[i] if i < len(fn.args) else None
                if T_VALUE_STRUCT_PTR == getattr(arg, 'type', None):
                    arg = self.unbox_value(arg, getattr(fn_arg, 'type', None))
                unboxed_args.append(arg)
            args = unboxed_args

        print('args afterwards', [getattr(a, 'type', 'no type') for a in args])
        fn_retval = self.builder.call(fn, args)
        if T_VOID == fn.ftype.return_type:
            # Can't return void as a value, so coerce to nil. Only
            # applicable to C FFI.
            return ir.Constant(T_VOID_PTR, NULL_PTR)
        return fn_retval

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
    # XXX if needs to box/unbox its values because the return type
    # can be varying.
    assert 4 == len(expression), 'if takes exactly 3 arguments'
    _, a, b, c = expression
    condition = compile_expression(env, a, depth=depth+1)
    condition = env.unbox_value(condition, T_BOOL)

    previous_block = env.builder.block
    endif_block = env.add_block('endif')

    then_block = env.add_block('then')
    then_result = compile_expression(env, b, depth=depth+1)
    env.builder.branch(endif_block)

    else_block = env.add_block('else')
    else_result = compile_expression(env, c, depth=depth+1)
    env.builder.branch(endif_block)

    with env.builder.goto_block(previous_block):
        env.builder.cbranch(condition, then_block, else_block)

    env.builder.position_at_end(endif_block)
    phi = env.builder.phi(T_VOID_PTR)
    phi.add_incoming(then_result, then_block)
    phi.add_incoming(else_result, else_block)
    # XXX we don't actually know the type
    retval = env.call('c/make_value', [T_I32(3), store_value(env, phi)])
    return retval

def compile_native_op(env, expression, depth=0):
    # XXX currently only 2-arity, use macros to reduce
    assert 3 == len(expression), expression[0] + ' takes exactly 2 arguments'
    a, b, c = expression
    lhs = compile_expression(env, b, depth=depth+1)
    lhs = env.unbox_value(lhs, T_I32)
    rhs = compile_expression(env, c, depth=depth+1)
    rhs = env.unbox_value(rhs, T_I32)
    result = None
    if '+' == a:
        result = env.builder.add(lhs, rhs)
    if '-' == a:
        result = env.builder.sub(lhs, rhs)
    if '*' == a:
        result = env.builder.mul(lhs, rhs)
    if '/' == a:
        result = env.builder.sdiv(lhs, rhs)
    if a in ['<', '<=', '==', '!=', '>=', '>']:
        result = env.builder.icmp_signed(a, lhs, rhs)
    return env.call('c/make_value', [T_I32(3), store_value(env, result)])

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

def compile_defn(env, expression, depth=0):
    assert 4 <= len(expression), 'defn takes at least 3 arguments'

    assert 2 == len(expression[1]), 'defn type/name must be exactly 2 elements'
    ret_type, fn_name = expression[1]
    ret_type = TYPES[ret_type]

    previous_block = env.current_block_name()

    args = expression[2]
    arg_types = []
    for arg in args:
        assert 2 == len(arg), 'defn arguments must be exactly 2 elements'
        arg_types.append(TYPES[arg[0]])
    fn = env.add_fn(fn_name, ret_type, arg_types)

    body = (expression[3:])
    fn_args = fn.args
    new_scope = dict(zip ([arg[1] for arg in args], fn.args))
    env.scopes.append(new_scope)
    retval = compile_progn(env, body, depth=depth+1)
    env.builder.ret(retval)
    env.scopes.pop()

    env.builder.position_at_end(env.blocks[previous_block])
    return fn

def compile_def(env, expression, depth=0):
    assert 3 == len(expression), 'def takes exactly two arguments'
    _, name, val = expression
    evaled_value = compile_expression(env, val, depth=depth+1)
    env.scopes[-1][name] = evaled_value
    return evaled_value

def compile_function_call(env, expression, depth=0):
    if 'def' == expression[0]:
        return compile_def(env, expression, depth=depth+1)

    if 'defn' == expression[0]:
        return compile_defn(env, expression, depth=depth+1)

    if 'progn' == expression[0]:
        return compile_progn(env, expression[1:], depth=depth+1)

    if 'let' == expression[0]:
        return compile_let(env, expression, depth=depth+1)

    if 'box' == expression[0]:
        return compile_box(env, expression, depth=depth+1)

    if 'if' == expression[0]:
        return compile_if(env, expression, depth=depth+1)

    if expression[0] in ['+', '-', '*', '/',
                         '<', '<=', '==', '!=', '>=', '>']:
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
    val = make_string(eval(expression))
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, 100)
    runtime_value = env.builder.call(env.lib['c/make_value'], [typ, vptr])
    return runtime_value

def compile_constant_int(env, expression):
    val = ir.Constant(T_I32, int(expression))
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, 2)
    runtime_value = env.builder.call(env.lib['c/make_value'], [typ, vptr])
    return runtime_value

def compile_constant_float(env, expression):
    # XXX Smaller floats aren't passed correctly to printf, resulting
    # in misread values. This has to do with us not having explicit
    # type casts, and not knowing what to cast to for variadic
    # function arguments.
    val = ir.Constant(T_F64, float(expression))
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, 3)
    runtime_value = env.call('c/make_value', [typ, vptr])
    return runtime_value

def compile_constant_bool(env, expression):
    val = ir.Constant(T_BOOL, 'true' == expression)
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, 2)
    runtime_value = env.builder.call(env.lib['c/make_value'], [typ, vptr])
    return runtime_value

def compile_nil(env, expression):
    return ir.Constant(T_VOID_PTR, NULL_PTR)

def compile_symbol(env, expression):
    for scope in reversed(env.scopes):
        if expression in scope:
            this = scope[expression]
            return this
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
    main_fn = ir.Function(module, f_type, name='main')
    main_entry_block = main_fn.append_basic_block('entry')
    env = Environment(module, main_entry_block)

    # C stdlib
    env.declare_fn("printf", T_VOID, [T_VOID_PTR], var_arg=True)

    # libnebula
    env.declare_fn("init_nebula", T_VOID, [])
    env.declare_fn('make_value', T_VALUE_STRUCT_PTR, [T_I32, T_VOID_PTR])
    env.declare_fn('unbox_value', T_PRIMITIVE_PTR, [T_VALUE_STRUCT_PTR])
    env.declare_fn("read_file", T_VOID_PTR, [T_VOID_PTR, T_VOID_PTR])
    env.declare_fn('print_int', T_VOID, [T_I32])
    env.declare_fn('print_bool', T_VOID, [T_BOOL])
    env.declare_fn("random_bool", T_BOOL, [])
    env.declare_fn("not", T_BOOL, [T_BOOL])
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
