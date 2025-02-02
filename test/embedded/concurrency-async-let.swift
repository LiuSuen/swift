// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend -target %target-cpu-apple-macos14 -enable-experimental-feature Embedded -parse-as-library %s %S/Inputs/print.swift -c -o %t/a.o
// RUN: %target-clang -x c -c %S/Inputs/print.c -o %t/print.o
// RUN: %target-clang %t/a.o %t/print.o -o %t/a.out %swift_obj_root/lib/swift/embedded/%target-cpu-apple-macos/libswift_Concurrency.a -dead_strip
// RUN: %target-run %t/a.out | %FileCheck %s

// REQUIRES: swift_in_compiler
// REQUIRES: optimized_stdlib
// REQUIRES: VENDOR=apple
// REQUIRES: OS=macosx

import _Concurrency

func fib(_ n: Int) -> Int {
  var first = 0
  var second = 1
  for _ in 0..<n {
    let temp = first
    first = second
    second = temp + first
  }
  return first
}

@available(SwiftStdlib 5.1, *)
func asyncFib(_ n: Int) async -> Int {
  if n == 0 || n == 1 {
    return n
  }

  async let first = await asyncFib(n-2)
  async let second = await asyncFib(n-1)

  let result = await first + second

  return result
}

@available(SwiftStdlib 5.1, *)
func runFibonacci(_ n: Int) async {
  let result = await asyncFib(n)

  print("")
  print(result == fib(n) ? "OK!" : "???")
  // CHECK: OK!
}

@available(SwiftStdlib 5.1, *)
@main struct Main {
  static func main() async {
    await runFibonacci(10)
  }
}
