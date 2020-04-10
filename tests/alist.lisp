;; This tests that association lists work.

(def alist (c/alist 42 "foo" nil))

(c/print_value alist)
(c/print_value "\n")

(def longer-alist (c/alist "bar" "baz" alist))
(c/print_value longer-alist)
(c/print_value "\n")

;; Test lookup works.
(c/print_value (c/aget 42 longer-alist))
(c/print_value "\n")

(c/print_value (c/aget "bar" longer-alist))
(c/print_value "\n")

(c/print_value (c/aget 46 longer-alist))
(c/print_value "\n")
