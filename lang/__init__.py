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

def call_print_str(env, string):
    string += '\0'
    byte_array = bytearray(string.encode('utf8'))
    s_type = ir.ArrayType(T_I8, len(byte_array))
    s = ir.Constant(s_type, byte_array)
    s_ptr = env.builder.alloca(s_type)
    env.builder.store(s, s_ptr)
    arg = env.builder.bitcast(s_ptr, T_VOID_PTR)
    env.call("printf", [arg])

def call_print_ptr(env, ptr):
    env.call("printf", [ptr])

def call_read_file(env, path, mode):
    # TODO args don't handle unicode yet
    path += '\0'
    path_type = ir.ArrayType(T_I8, len(path))
    path_ptr = env.builder.alloca(path_type)
    path_const = ir.Constant(path_type, bytearray(path.encode('utf8')))
    env.builder.store(path_const, path_ptr)

    mode += '\0'
    mode_type = ir.ArrayType(T_I8, len(mode))
    mode_ptr = env.builder.alloca(mode_type)
    mode_const = ir.Constant(mode_type, bytearray(mode.encode('utf8')))
    env.builder.store(mode_const, mode_ptr)

    path_arg = env.builder.bitcast(path_ptr, T_VOID_PTR)
    mode_arg = env.builder.bitcast(mode_ptr, T_VOID_PTR)
    return env.call("read_file", [path_arg, mode_arg])

class Environment:
    def __init__(self, module, builder):
        self.module = module
        self.builder = builder
        self.lib = dict()

    def add_func(self, name, func):
        self.lib[name] = func

    def call(self, name, args):
        return self.builder.call(self.lib[name], args)

def compile_main_func():
    module = ir.Module(name=__file__)
    module.triple = llvm.get_default_triple()

    f_type = ir.FunctionType(T_I32, [])
    func = ir.Function(module, f_type, name="main")

    block = func.append_basic_block(name="entry")
    builder = ir.IRBuilder(block)

    env = Environment(module, builder)
    env.add_func("init_nebula",
                 ir.Function(module,
                             ir.FunctionType(T_VOID, []),
                             name="init_nebula"))
    env.add_func("printf",
                 ir.Function(module,
                             ir.FunctionType(T_VOID, [T_VOID_PTR], var_arg=True),
                             name="printf"))
    env.add_func("read_file",
                 ir.Function(module,
                             ir.FunctionType(ir.PointerType(T_I8),
                                             [T_VOID_PTR, T_VOID_PTR]),
                             name="read_file"))

    env.call("init_nebula", [])

    call_print_str(env, "Printing a long string here.\nWith unicode: Hélène\n")

    file_contents = call_read_file(env, "README.rst", "r")
    call_print_ptr(env, file_contents)

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

    # print(main_mod)

if __name__ == '__main__':
    main()
