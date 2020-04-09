;; This thests that we can branch based on a random value, which LLVM
;; can't optimise away.

(if (c/random_bool)
    (c/printf "Random was true\n")
    (c/printf "Random was false\n"))
