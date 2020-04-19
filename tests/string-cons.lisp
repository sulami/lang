;; This tests string->cons conversion.

(print_value (string_to_cons "abc"))
(print_value "\n")

;; The reverse.
(print_value (cons_to_string (string_to_cons "abc")))
(print_value "\n")
