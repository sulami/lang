__version__ = '0.1.0'

import subprocess
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

def store_string(env, string):
    """
    Compiles a set of instructions to store a string and returns an
    anonymised pointer to it. Properly deals with unicode.
    """
    string += '\0'
    byte_array = bytearray(string.encode('utf8'))
    s_type = ir.ArrayType(T_I8, len(byte_array))
    s = ir.Constant(s_type, byte_array)
    s_ptr = env.builder.alloca(s_type)
    env.builder.store(s, s_ptr)
    return env.builder.bitcast(s_ptr, T_VOID_PTR)

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
    def __init__(self, module, fn, builder):
        self.module = module
        self.builder = builder
        self.fn = fn
        self.lib = dict()

    def declare_fn(self, name, return_type, arg_types, **kwargs):
        self.lib[name] = ir.Function(
            self.module,
            ir.FunctionType(return_type, arg_types, **kwargs),
            name=name
        )

    def call(self, name, args):
        return self.builder.call(self.lib[name], args)

def compile_if(env):
    condition = env.call("random_bool", [])
    with env.builder.if_else(condition) as (then, otherwise):
        with then:
            call_print_str(env, "true\n")
        with otherwise:
            call_print_str(env, "false\n")

def compile_main_func():
    module = ir.Module(name=__file__)
    module.triple = llvm.get_default_triple()

    f_type = ir.FunctionType(T_I32, [])
    func = ir.Function(module, f_type, name="main")

    block = func.append_basic_block(name="entry")
    builder = ir.IRBuilder(block)

    env = Environment(module, func, builder)
    env.declare_fn("init_nebula", T_VOID, [])
    env.declare_fn("printf", T_VOID, [T_VOID_PTR], var_arg=True)
    env.declare_fn("read_file", ir.PointerType(T_I8), [T_VOID_PTR, T_VOID_PTR])
    env.declare_fn("random_bool", T_BOOL, [])

    env.call("init_nebula", [])

    call_print_str(env, "Printing a long string here.\nWith unicode: Hélène\n")

    file_contents = call_read_file(env, "README.rst", "r")
    call_print_ptr(env, file_contents)

    compile_if(env)

    builder.ret(ir.Constant(T_I32, 0))

    return module

def compile_nebula():
    print("Compiling nebula...")
    # TODO don't recompile if it's current
    subprocess.run(["cc",
                    "-Wall",
                    "-shared",
                    "-fpic",
                    "-o", "libnebula.so",
                    "lang/nebula.c"])
    llvm.load_library_permanently("libnebula.so")

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
                            "-L/Users/sulami/build/lang",
                            "-lnebula",
                            tmp_obj.name],
                           check=True)

def main():
    init_llvm()
    compile_nebula()
    engine = compile_execution_engine()

    main_mod = compile_main_func()
    main_mod = compile_ir(engine, str(main_mod))
    # print(main_mod)
    compile_binary(main_mod)

    # this works, but I don't know how to call it then.
    # str_mod = compile_str_func("say_hello", "hello, world!\n")
    # str_mod = compile_ir(engine, str(str_mod))
    # main_mod.link_in(str_mod, preserve=True)

if __name__ == '__main__':
    main()
