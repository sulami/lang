__version__ = '0.1.0'

from llvmlite import ir
import llvmlite.binding as llvm

def init_llvm():
    """Setup the LLVM core."""
    llvm.initialize()
    llvm.initialize_native_target()
    llvm.initialize_native_asmprinter()

def deinit_llvm():
    """Shutdown the LLVM core."""
    llvm.shutdown()

def create_execution_engine():
    """
    Create an ExecutionEngine suitable for JIT code generation on
    the host CPU.  The engine is reusable for an arbitrary number of
    modules.
    """
    # Create a target machine representing the host
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

def create_add_func():
    integer = ir.IntType(32)
    ftype = ir.FunctionType(integer, [])
    module = ir.Module(name=__file__)
    module.triple = llvm.get_default_triple()
    func = ir.Function(module, ftype, name="main")
    block = func.append_basic_block(name="entry")
    builder = ir.IRBuilder(block)
    result = ir.Constant(integer, 42)
    builder.ret(result)
    return module

def main():
    init_llvm()
    add_mod = create_add_func()
    engine = create_execution_engine()
    mod = compile_ir(engine, str( add_mod))
    print(mod)
    # deinit_llvm()

if __name__ == '__main__':
    main()
