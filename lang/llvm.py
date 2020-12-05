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
T_FUNCTION = ir.LiteralStructType([T_VOID_PTR, T_VOID_PTR])
T_FUNCTION_PTR = T_FUNCTION.as_pointer()

# Values
NULL_PTR = T_I32(0).inttoptr(T_VOID_PTR)

FFI_TYPES = {
    'value': T_VALUE_STRUCT_PTR,
    'void': T_VOID,
    'void_ptr': T_VOID_PTR,
    'bool': T_BOOL,
    'i32': T_I32,
    'i64': T_I64,
    'string': T_VOID_PTR,
    'ptr_ptr': T_VOID_PTR.as_pointer(),
}

FFI_TYPE_MAPPING = {
    T_VALUE_STRUCT_PTR: 132,
    T_VOID: 0,
    T_VOID_PTR: 132,
    T_BOOL: 1,
    T_I32: 2,
    T_I64: 2,
    T_VOID_PTR.as_pointer(): 132,
}

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
