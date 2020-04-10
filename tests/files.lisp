;; This tests that we can read a file and print the contents.

(c/print_value (c/read_file "tests/files.lisp" "r"))
