;; This tests that we can read a file and print the contents.

(print_value (read_file "tests/files.lisp" "r"))
