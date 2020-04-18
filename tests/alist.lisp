;; This tests that association lists work.

(def alist (alist 42 "foo" nil))

(print_value alist)
(print_value "\n")

(def longer-alist (alist "bar" "baz" alist))
(print_value longer-alist)
(print_value "\n")

;; Test lookup works.
(print_value (aget 42 longer-alist))
(print_value "\n")

(print_value (aget "bar" longer-alist))
(print_value "\n")

(print_value (aget 46 longer-alist))
(print_value "\n")
