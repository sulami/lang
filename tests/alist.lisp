;; This tests that association lists work.

(def a-list (alist 42 "foo" nil))

(print_value a-list)
(print_value "\n")

(def longer-alist (alist "bar" "baz" a-list))
(print_value longer-alist)
(print_value "\n")

;; Test lookup works.
(print_value (aget 42 longer-alist))
(print_value "\n")

(print_value (aget "bar" longer-alist))
(print_value "\n")

(print_value (aget 46 longer-alist))
(print_value "\n")
