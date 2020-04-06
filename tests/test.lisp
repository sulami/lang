;; This is a comment

;; C functions are in the c pseudo-namespace.
(c/printf "Hello from Lisp!\n")

(c/printf "Printing some unicode: %s\n" "今日は")

(c/printf "An integer is: %d\n" 42)

(c/printf "A float is: %f\n" 3.14159)

(c/printf "A boolean is: %d or %d\n" true false)

(if (c/random_bool)
    (c/printf "If is true\n")
    (c/printf "If is false\n"))

nil ;; is a value as well

;; Maths operators are builtins right now
(c/printf "Calulcations: %d, %d, %d, %d\n"
          (+ 10 3)
          (- 10 3)
          (* 10 3)
          (/ 10 3)) ;; Integer division
