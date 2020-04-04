__version__ = '0.1.0'

import subprocess
import tempfile

from llvmlite import ir
import llvmlite.binding as llvm

# Types
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

def compile_syscall(builder, arg):
    sys_write = 0x2000004
    # regs = ','.join(['rdi', 'rsi', 'rdx', 'r10', 'r8', 'r9'][:len(args)])
    regs = ','.join(['rdi', 'rsi', 'rdx', 'r10', 'r8', 'r9'][:1])

    # store the arg on the stack & get a pointer
    ptr = builder.alloca(arg.type)
    builder.store(arg, ptr)

    # TODO types don't line up here
    f_type = ir.FunctionType(T_I64, [arg, ptr])
    builder.asm(f_type,
                "syscall",
                "=r,{rax}," + regs + ",~{dirflag},~{fspr},~{flags}",
                [ptr, ptr], # Args
                True, # Side effects
                name="syscall") # Optional name

def compile_str_func(module, name, s):
    # module = ir.Module(name=name)
    # module.triple = llvm.get_default_triple()

    s_type = ir.ArrayType(T_I8, len(s))
    f_type = ir.FunctionType(s_type , [])
    func = ir.Function(module, f_type, name=name)

    block = func.append_basic_block(name="entry")
    builder = ir.IRBuilder(block)

    s = ir.Constant(s_type, [ord(c) for c in s])
    # sc = compile_syscall(builder, s)

    builder.ret(s)

    return func

def compile_main_func():
    module = ir.Module(name=__file__)
    module.triple = llvm.get_default_triple()

    f_type = ir.FunctionType(T_I32, [])
    func = ir.Function(module, f_type, name="main")

    block = func.append_basic_block(name="entry")
    builder = ir.IRBuilder(block)

    # hello_func = compile_str_func(module, "say_hello", "hello, world!\n")
    # builder.call(hello_func, [])
    lib_func = ir.Function(module,
                           ir.FunctionType(ir.VoidType(), []),
                           name="say_hi")
    builder.call(lib_func, [])
    builder.ret(ir.Constant(T_I32, 42))

    return module

def compile_nebula():
    print("Compiling nebula...")
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
    compile_binary(main_mod)

    # this works, but I don't know how to call it then.
    # str_mod = compile_str_func("say_hello", "hello, world!\n")
    # str_mod = compile_ir(engine, str(str_mod))
    # main_mod.link_in(str_mod, preserve=True)

    # print(main_mod)

if __name__ == '__main__':
    main()
