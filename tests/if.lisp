;; This thests that we can branch based on a random value, which LLVM
;; can't optimise away.

;; In this case we use C types in all positions.

(if (random_bool)
    (print_value "Random was true\n")
    (print_value "Random was false\n"))

;; In this case we use boxed values as return types.

(if (random_bool)
    42
    46)

;; In this case we use the return value for FFI.

(print_value (if (random_bool)
                   42
                   46))
(print_value "\n")
