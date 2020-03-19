(ns parser-test
  (:require [parser :as sut]
            [clojure.test :refer [deftest is testing]]))

(deftest parser-test
  (testing "it parses empty strings"
    (let [result (sut/parse (sut/->initial-state ""))]
      (is (sut/successful-parse? result))
      (is (empty? (:ast result)))))

  (testing "it parses a bare token"
    (let [result (sut/parse (sut/->initial-state "foobar"))]
      (is (sut/successful-parse? result))
      (is (= 1 (count (:ast result))))))

  (testing "it parses several bare tokens"
    (let [result (sut/parse (sut/->initial-state "foo bar"))]
      (is (sut/successful-parse? result))
      (is (= 2 (count (:ast result))))))

  (testing "it parses an S-expression"
    (let [result (sut/parse (sut/->initial-state "(foo)"))]
      (is (sut/successful-parse? result))
      (is (= 1 (count (:ast result))))
      (is (= 1 (-> result :ast first count)))))

  (testing "it parses several S-expressions"
    (let [result (sut/parse (sut/->initial-state "(foo) (bar)"))]
      (is (sut/successful-parse? result))
      (is (= 2 (count (:ast result))))
      (is (every? #(-> % count (= 1)) (:ast result)))))

  (testing "it parses nested S-expressions"
    (let [result (sut/parse (sut/->initial-state "(foo (bar) baz)"))]
      (is (sut/successful-parse? result))
      (is (= 1 (count (:ast result))))
      (is (= 3 (-> result :ast first count)))
      (is (= 1 (-> result :ast first second count)))))

  (testing "it rejects unclosed S-expressions"
    (let [result (sut/parse (sut/->initial-state "foo (bar"))]
      (is (not (sut/successful-parse? result)))))

  (testing "it rejects unopened S-expressions"
    (let [result (sut/parse (sut/->initial-state "foo) bar"))]
      (is (not (sut/successful-parse? result))))))
