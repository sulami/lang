import random
import re
import string

from lang.debug import debug
from lang.llvm import *

# Type mapping (for runtime values)
RUNTIME_TYPES = {
    'nil': 0,
    'bool': 1,
    'int': 2,
    'float': 3,
    'char': 4,
    'type': 5,
    'string': 128,
    'cons': 129,
    'function': 130,
    'array': 131,
    'pointer': 132,
    'vector': 133,
    'hashmap': 134,
    'keyword': 135,
}

def store_value(env, value):
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
        self.global_scope = dict()
        self.scopes = [dict()]

    def add_fn(self, name, argc):
        f_type = ir.FunctionType(T_VALUE_STRUCT_PTR, [T_VALUE_STRUCT_PTR for _ in range(argc)])
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
        self.lib[name] = ir.Function(
            self.builder.module,
            ir.FunctionType(return_type, arg_types, **kwargs),
            name=name
        )

    def unbox_value(self, val, out_type=T_I8, load=True):
        debug('out_type', out_type)
        val = self.builder.call(self.lib['unbox_value'], [val])
        if load:
            val = self.builder.bitcast(val, out_type.as_pointer())
            return self.builder.load(val)
        val = self.builder.bitcast(val, out_type)
        return val

    def call(self, name, args, tail=False):
        fn = self.lib[name]
        debug('fn.name', fn.name)
        debug('ftype', fn.ftype)
        debug('fn.args', [a.type for a in fn.args])
        debug('args', [getattr(a, 'type', 'No type') for a in args])

        if len(args) != len(fn.args):
            raise Exception('Arity error: {} expected {} args, got {}'.format(
                fn.name, len(fn.args), len(args)))

        unboxed_args = []
        for i in range(len(args)):
            arg = args[i]
            fn_arg = fn.args[i]
            fn_arg_type = getattr(fn_arg, 'type', None)
            if T_VALUE_STRUCT_PTR == arg.type and arg.type != fn_arg_type:
                arg = self.unbox_value(
                    arg,
                    out_type=fn_arg_type,
                    load=not hasattr(fn_arg_type, 'pointee')
                )
            unboxed_args.append(arg)
        args = unboxed_args

        debug('args afterwards', [getattr(a, 'type', 'no type') for a in args])
        fn_retval = self.builder.call(fn, args, tail=tail)
        if T_VALUE_STRUCT_PTR != fn.ftype.return_type:
            debug('fn return type', fn.ftype.return_type)
            # C FFI, we're not returning a boxed value, so box it.
            if T_VOID == fn.ftype.return_type:
                # Synthesise a nil value for void functions.
                fn_retval = compile_nil(self, None)
            else:
                if fn.ftype.return_type != T_VOID_PTR:
                    fn_retval = store_value(self, fn_retval)
                fn_retval = self.builder.call(
                    self.lib['make_value'],
                    [T_I32(FFI_TYPE_MAPPING[fn.ftype.return_type]), fn_retval]
                )
        return fn_retval


def compile_if(env, expression, depth=0):
    assert 4 == len(expression), 'if takes exactly 3 arguments'
    _, a, b, c = expression
    condition = compile_expression(env, a, depth=depth+1)
    if T_VALUE_STRUCT_PTR == condition.type:
        condition = env.builder.call(env.lib['value_truthy'], [condition])

    previous_block = env.builder.block
    endif_block = env.add_block('endif')

    # Then
    then_block = env.add_block('then')
    then_result = compile_expression(env, b, depth=depth+1)
    # Add another else block so the PHI knows where the branch will
    # come from, even if the inner expression changed blocks. Patch up
    # the inner block to jump there.
    then_inner_block = env.builder.block
    then_end_block = env.add_block('then_end')
    env.builder.branch(endif_block)
    env.builder.position_at_end(then_inner_block)
    env.builder.branch(then_end_block)

    # Else
    else_block = env.add_block('else')
    else_result = compile_expression(env, c, depth=depth+1)
    # See above.
    else_inner_block = env.builder.block
    else_end_block = env.add_block('else_end')
    env.builder.branch(endif_block)
    env.builder.position_at_end(else_inner_block)
    env.builder.branch(else_end_block)

    with env.builder.goto_block(previous_block):
        env.builder.cbranch(condition, then_block, else_block)

    env.builder.position_at_end(endif_block)
    phi = env.builder.phi(T_VALUE_STRUCT_PTR)
    phi.add_incoming(then_result, then_end_block)
    phi.add_incoming(else_result, else_end_block)
    return phi

def compile_native_op(env, expression, depth=0):
    # TODO port this to C and operate on values instead.
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
        return env.call('make_value', [T_I32(RUNTIME_TYPES['bool']), store_value(env, result)])
    return env.call('make_value', [T_I32(RUNTIME_TYPES['int']), store_value(env, result)])

def compile_box(env, expression, depth=0):
    assert 2 == len(expression), 'box takes exactly 1 argument'
    return store_value(env, compile_expression(env, expression[1], depth=depth+1))

def compile_progn(env, expression, depth=0):
    if [] == expression:
        return env.call('make_value', [T_I32(RUNTIME_TYPES['nil']), NULL_PTR])
    else:
        retval = None
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

def compile_lambda(env, expression, name=None, depth=0):
    assert 3 <= len(expression), 'lambda takes at least 3 arguments'
    fn_name = name or '{}'.format(''.join(random.choice(string.ascii_lowercase)
                                          for _ in range(6)))
    fn_name = 'fn_' + fn_name
    previous_block = env.current_block_name()
    args = expression[1]
    fn = env.add_fn(fn_name, len(args))

    body = (expression[2:])
    fn_args = fn.args
    new_scope = dict(zip ([arg for arg in args], fn.args))
    env.scopes.append(new_scope)
    retval = compile_progn(env, body, depth=depth+1)
    env.builder.ret(retval)
    env.scopes.pop()

    env.builder.position_at_end(env.blocks[previous_block])
    fn_ptr = env.builder.bitcast(fn, T_VOID_PTR)
    fn_value = env.call(
        'make_function', [store_value(env, make_string(fn_name)), fn_ptr]
    )
    return fn_value

def compile_defun(env, expression, depth=0):
    assert 4 <= len(expression), 'defun takes at least 3 arguments'
    fn_name = expression.pop(1)
    gv = ir.GlobalVariable(env.builder.module, T_VALUE_STRUCT_PTR, fn_name)
    gv.linkage = 'internal'
    env.global_scope[fn_name] = gv
    fn = compile_lambda(env, expression, name=fn_name, depth=depth+1)
    env.builder.store(fn , gv)
    return fn

def compile_def(env, expression, depth=0):
    assert 3 == len(expression), 'def takes exactly two arguments'
    _, name, val = expression
    evaled_value = compile_expression(env, val, depth=depth+1)
    debug('evaled_value', evaled_value)
    gv = ir.GlobalVariable(env.builder.module, T_VALUE_STRUCT_PTR, name)
    # It can't be constant because we have to write to it, as we don't
    # know the value at compile-time.
    # gv.global_constant = True
    # gv.initializer = evaled_value
    gv.linkage = 'internal'
    env.builder.store(evaled_value , gv)
    env.global_scope[name] = gv
    debug('gv', gv)
    return gv

def compile_recur(env, expression, depth=0):
    args = [compile_expression(env, arg) for arg in expression[1:]]
    current_fn_name = env.builder.function.name
    return env.call(current_fn_name, args, tail=True)

def compile_declare(env, expression, depth=0):
    _, name, return_type, arg_types = expression
    env.declare_fn(
        name,
        FFI_TYPES[return_type],
        [FFI_TYPES[t] for t in arg_types]
    )

def compile_function_call(env, expression, depth=0):
    if 'declare' == expression[0]:
        return compile_declare(env, expression, depth=depth+1)

    if 'lambda' == expression[0]:
        return compile_lambda(env, expression, depth=depth+1)

    if 'recur' == expression[0]:
        return compile_recur(env, expression, depth=depth+1)

    if 'def' == expression[0]:
        return compile_def(env, expression, depth=depth+1)

    if 'defun' == expression[0]:
        return compile_defun(env, expression, depth=depth+1)

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
    args = []
    for exp in expression[1:]:
        args += [compile_expression(env, exp, depth=depth+1)]

    if isinstance(expression[0], list):
        # Function is a function call? Eval it.
        fn_value = compile_expression(env, expression[0])
    elif expression[0] in env.lib:
        # If it's a global function just jump there.
        return env.call(expression[0], args)
    elif compile_symbol(env, expression[0]):
        # Try looking it up in the local scope first.
        fn_value = compile_symbol(env, expression[0])
    else:
        raise Exception("Couldn't find function: " + expression[0])

    # There is a lot of pointer following and struct indexing going on
    # here until we finally get to the function pointer.
    primitive_ptr = env.builder.call(env.lib['unbox_value'], [fn_value])
    fn_struct_ptr = env.builder.bitcast(primitive_ptr, T_FUNCTION_PTR)
    fn_struct_ptr = env.builder.load(fn_struct_ptr)
    fn_struct_ptr = env.builder.extract_value(fn_struct_ptr, 0)
    fn_struct_ptr = env.builder.bitcast(fn_struct_ptr, T_FUNCTION_PTR)
    fn_ptr_ptr = env.builder.gep(fn_struct_ptr, [T_I32(0), T_I32(1)])

    fn_ptr = env.builder.load(fn_ptr_ptr)
    fn_ptr_type = ir.FunctionType(
        T_VALUE_STRUCT_PTR,
        [T_VALUE_STRUCT_PTR for _ in range(len(args))]
    ).as_pointer()
    fn_ptr = env.builder.bitcast(fn_ptr, fn_ptr_type)
    return env.builder.call(fn_ptr, args)

def compile_constant_string(env, expression):
    # Strip quotation marks
    # Convert escape sequences
    # XXX eval is the hacky way of accomplishing this.
    # It's fine because we can trust our input.
    val = make_string(eval(expression))
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, RUNTIME_TYPES['string'])
    runtime_value = env.builder.call(env.lib['make_value'], [typ, vptr])
    return runtime_value

def compile_constant_int(env, expression):
    val = ir.Constant(T_I32, int(expression))
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, RUNTIME_TYPES['int'])
    runtime_value = env.builder.call(env.lib['make_value'], [typ, vptr])
    return runtime_value

def compile_constant_float(env, expression):
    # XXX Smaller floats aren't passed correctly to printf, resulting
    # in misread values. This has to do with us not having explicit
    # type casts, and not knowing what to cast to for variadic
    # function arguments.
    val = ir.Constant(T_F32, float(expression))
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, RUNTIME_TYPES['float'])
    runtime_value = env.call('make_value', [typ, vptr])
    return runtime_value

def compile_constant_bool(env, expression):
    val = ir.Constant(T_BOOL, 'true' == expression)
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, RUNTIME_TYPES['bool'])
    runtime_value = env.builder.call(env.lib['make_value'], [typ, vptr])
    return runtime_value

def compile_nil(env, expression):
    vptr = NULL_PTR
    typ = ir.Constant(T_I32, RUNTIME_TYPES['nil'])
    runtime_value = env.builder.call(env.lib['make_value'], [typ, vptr])
    return runtime_value

def compile_keyword(env, expression):
    # Basically just a string with a different type.
    val = make_string(expression[1:])
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, RUNTIME_TYPES['keyword'])
    runtime_value = env.builder.call(env.lib['make_value'], [typ, vptr])
    return runtime_value

def compile_constant_char(env, expression):
    # Chars are encoded as integers.
    if 2 == len(expression):
        val = ir.Constant(T_I32, ord(expression[1]))
    else:
        char_mapping = {
            'newline': '\n',
            'space': ' ',
            'tab': '\t',
            '(': '(',
            ')': ')',
            '"': '"',
        }
        val = ir.Constant(T_I32, ord(char_mapping[expression[1:]]))
    vptr = store_value(env, val)
    typ = ir.Constant(T_I32, RUNTIME_TYPES['char'])
    runtime_value = env.builder.call(env.lib['make_value'], [typ, vptr])
    return runtime_value

def compile_symbol(env, expression):
    debug('trying to resolve', expression)
    for scope in reversed(env.scopes):
        debug('searching scope:', scope.keys())
        if expression in scope:
            return scope[expression]
    debug('Could not find symbol ' + expression + ' in local scope')
    if expression in env.global_scope:
        debug('Found symbol ' + expression + ' in global scope:')
        debug(env.global_scope[expression])
        return env.builder.load(env.global_scope[expression])
    raise Exception('Could not find symbol ' + expression + ' in global scope')

def compile_expression(env, expression, depth=0):
    debug(' ' * (depth + 1) + 'Compiling expression: ' + str(expression))

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
    elif re.match('^\\\\(\w|space|newline|tab|\(|\)|"|;|\\\\)$', expression):
        # char (NOTE: this regex above matches "\c")
        return compile_constant_char(env, expression)
    elif re.match('^-?[0-9]+\.[0-9]+$', expression):
        # constant float
        return compile_constant_float(env, expression)
    elif re.match('^-?[0-9]+$', expression):
        # constant int
        return compile_constant_int(env, expression)
    elif re.match('^:[a-zA-Z_-]+$', expression):
        # keyword
        return compile_keyword(env, expression)
    else:
        # symbol
        return compile_symbol(env, expression)

def compile_main(ast):
    module = ir.Module(name='main')
    module.triple = llvm.get_default_triple()

    # LLVM main function
    f_type = ir.FunctionType(T_I32, [T_I32, T_VOID_PTR.as_pointer()])
    main_fn = ir.Function(module, f_type, name='main')
    main_entry_block = main_fn.append_basic_block('entry')
    env = Environment(module, main_entry_block)

    # Declare libnebula. These are used in the compiler and have to be
    # declared before usercode starts.
    env.declare_fn("nebula_main", T_I32, [T_I32, T_VOID_PTR.as_pointer()])
    env.declare_fn('make_value', T_VALUE_STRUCT_PTR, [T_I32, T_VOID_PTR])
    env.declare_fn('unbox_value', T_PRIMITIVE_PTR, [T_VALUE_STRUCT_PTR])
    env.declare_fn('make_function', T_VALUE_STRUCT_PTR, [T_VOID_PTR, T_VOID_PTR])
    env.declare_fn('cons', T_VALUE_STRUCT_PTR, [T_VALUE_STRUCT_PTR, T_VALUE_STRUCT_PTR])

    # Dispatch to runtime main function
    argc, argv = main_fn.args
    env.builder.ret(env.builder.call(env.lib["nebula_main"], [argc, argv]))

    # User code implicit main
    usercode_f_type = ir.FunctionType(T_I32, [T_I32, T_VOID_PTR.as_pointer()])
    usercode_main_fn = ir.Function(env.builder.module, usercode_f_type, name='usercode_main')
    usercode_main_entry_block = usercode_main_fn.append_basic_block('entry')
    env.builder.position_at_end(usercode_main_entry_block)
    argc, argv = usercode_main_fn.args

    # Setup loof blocks
    loop_block = env.add_block('loop')
    exit_block = env.add_block('usercode')

    # Jump to the loop block
    env.builder.position_at_end(usercode_main_entry_block)
    ctr_ptr = env.builder.alloca(argc.type)
    env.builder.store(argc, ctr_ptr)
    cons = env.call('make_value', [T_I32(RUNTIME_TYPES['nil']), NULL_PTR])
    cons_ptr = env.builder.alloca(cons.type)
    env.builder.store(cons, cons_ptr)
    env.builder.branch(loop_block)

    # In loop: add argv to cons, decrement argc
    env.builder.position_at_end(loop_block)
    current_ctr = env.builder.load(ctr_ptr)
    new_ctr = env.builder.sub(current_ctr, T_I32(1))
    new_ctr_value = env.call(
        'make_value',
        [T_I32(RUNTIME_TYPES['int']), store_value(env, new_ctr)]
    )
    this_argv = env.builder.gep(argv, [new_ctr])
    this_argv = env.builder.load(this_argv)
    this_argv_value = env.call('make_value', [T_I32(RUNTIME_TYPES['string']), this_argv])
    new_cons = env.call('cons', [this_argv_value, env.builder.load(cons_ptr)])
    env.builder.store(new_cons, cons_ptr)
    env.builder.store(new_ctr, ctr_ptr)
    condition = env.builder.icmp_signed('<', T_I32(0), new_ctr)
    env.builder.cbranch(condition, loop_block, exit_block)

    # Done looping
    env.builder.position_at_end(exit_block)
    # This prints out the argv cons
    # env.call('print_value', [env.builder.load(cons_ptr)])
    # env.call('print_value', [
    #     env.call('make_value', [
    #         T_I32(RUNTIME_TYPES['string']),
    #         store_value(env, make_string('\n'))
    #     ])
    # ])
    env.scopes[0]['argv'] = env.builder.load(cons_ptr)

    # Compile user code
    for expression in ast:
        compile_expression(env, expression)
    env.builder.ret(T_I32(0))

    return module
