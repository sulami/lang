;; This thests that we can branch based on a random value, which LLVM
;; can't optimise away.

;; In this case we use C types in all positions.

(if (c/random_bool)
    (c/printf "Random was true\n")
    (c/printf "Random was false\n"))

;; In this case we use boxed values as return types.

(if (c/random_bool)
    42
    46)

;; In this case we use the return value for FFI.

(c/print_int (if (c/random_bool)
                 42
                 46))
