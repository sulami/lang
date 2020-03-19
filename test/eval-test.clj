(ns eval-test
  (:require [eval :as sut]
            [clojure.test :refer [deftest is testing]]))

(deftest eval-atoms-test
  (testing "it returns the vm-state"
    (let [state sut/initial-vm-state]
      (is (= state (sut/eval-ast sut/initial-vm-state [])))))

  (testing "it returns the last return value"
    (is (= 1 (-> (sut/eval-ast sut/initial-vm-state
                               [1])
                 :return-value)))))

(deftest eval-def-test
  (testing "it can def values into the namespace"
    (is (= 1 (-> (sut/eval-ast sut/initial-vm-state
                               [[:def "a" 1]])
                 :ns
                 (get "a")))))

  (testing "it returns the value of the def"
    (is (= 1 (-> (sut/eval-ast sut/initial-vm-state
                               [[:def "a" 1]])
                 :return-value)))))

(deftest eval-if-test
  (testing "it can branch based on bare values"
    (is (= 1 (-> (sut/eval-ast sut/initial-vm-state
                               [[:if true 1 2]])
                 :return-value)))
    (is (= 2 (-> (sut/eval-ast sut/initial-vm-state
                               [[:if false 1 2]])
                 :return-value))))

  (testing "it can branch based on S-expressions"
    (is (= 1 (-> (sut/eval-ast sut/initial-vm-state
                               [[:if [:def "a" true] 1 2]])
                 :return-value)))
    (is (= 2 (-> (sut/eval-ast sut/initial-vm-state
                               [[:if [:def "a" false] 1 2]])
                 :return-value)))))

(deftest eval-symbol-lookup-test
  (testing "it can lookup a symbol"
    (is (= 1 (-> sut/initial-vm-state
                 (assoc-in [:ns "a"] 1)
                 (sut/eval-ast [[:symbol "a"]])
                 :return-value))))

  (testing "it returns nil if a symbol is not found"
    (is (= nil (-> sut/initial-vm-state
                   (assoc-in [:ns "d"] 1)
                   (sut/eval-ast [[:symbol "a"]])
                   :return-value)))))
