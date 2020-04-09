;; This tests that we can call a variadic function in C, like printf,
;; and we cast arguments to the right types, or at least reasonable
;; ones. Given that this is C, you can shoot youself in the foot
;; alright.

(c/printf "An int is: %d\n" 42)
