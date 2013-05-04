//===--- Cleanup.h - Declarations for SIL Cleanup Generation ----*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines the Cleanup and CleanupManager classes.
//
//===----------------------------------------------------------------------===//

#ifndef CLEANUP_H
#define CLEANUP_H

#include "JumpDest.h"
#include "swift/SIL/SILLocation.h"

namespace swift {
  class SILBasicBlock;
  class SILFunction;
  class SILValue;
  
namespace Lowering {
  class SILGenFunction;
  class CleanupOutflows;

/// The valid states that a cleanup can be in.
enum class CleanupState {
  /// The cleanup is inactive but may be activated later.
  Dormant,
  
  /// The cleanup is currently active.
  Active,
  
  /// The cleanup is inactive and will not be activated later.
  Dead
};

class LLVM_LIBRARY_VISIBILITY Cleanup {
  unsigned allocatedSize;
  CleanupState state;
  
  friend class CleanupManager;
protected:
  Cleanup() {}
  virtual ~Cleanup() {}
  
public:
  /// Return the allocated size of this object.  This is required by
  /// DiverseStack for iteration.
  size_t allocated_size() const { return allocatedSize; }
  
  CleanupState getState() const { return state; }
  void setState(CleanupState newState) { state = newState; }
  bool isActive() const { return state == CleanupState::Active; }
  bool isDead() const { return state == CleanupState::Dead; }

  virtual void emit(SILGenFunction &Gen) = 0;
};

class CleanupManager;
template<CleanupsDepth CleanupManager::*SCOPE>
class ScopeImpl;

class LLVM_LIBRARY_VISIBILITY CleanupManager {
  template<CleanupsDepth CleanupManager::*SCOPE>
  friend class ScopeImpl;

  SILGenFunction &Gen;
  
  /// Stack - Currently active cleanups in this scope tree.
  DiverseStack<Cleanup, 128> Stack;
  
  CleanupsDepth InnermostScope;
  CleanupsDepth ReturnScope;
  
  void popAndEmitTopCleanup();
  void popAndEmitTopDeadCleanups(CleanupsDepth end);
  
  Cleanup &initCleanup(Cleanup &cleanup, size_t allocSize, CleanupState state);
  void setCleanupState(Cleanup &cleanup, CleanupState state);
  
  void endScope(CleanupsDepth depth);
  
public:
  CleanupManager(SILGenFunction &Gen)
    : Gen(Gen), InnermostScope(Stack.stable_end()),
      ReturnScope(Stack.stable_end()) {
  }
  
  /// Return a stable reference to the current cleanup.
  CleanupsDepth getCleanupsDepth() const {
    return Stack.stable_begin();
  }
  
  /// emitBranchAndCleanups - Emit a branch to the given jump destination,
  /// threading out through any cleanups we need to run. This does not pop the
  /// cleanup stack.
  void emitBranchAndCleanups(JumpDest Dest);
  
  /// emitReturnAndCleanups - Emit a return from the current function, threading
  /// out through all active cleanups that need to run. This does not pop the
  /// cleanup stack.
  void emitReturnAndCleanups(SILLocation loc, SILValue returnValue);
  
  /// emitCleanupsForReturn - Emit all the cleanups necessary before a return
  /// branch (but not the branch itself).
  void emitCleanupsForReturn(SILLocation loc);
  
  /// pushCleanup - Push a new cleanup.
  template<class T, class... A>
  T &pushCleanupInState(CleanupState state,
                        A &&... args) {
    assert(state != CleanupState::Dead);

#ifndef NDEBUG
    CleanupsDepth oldTop = Stack.stable_begin();
#endif
    
    T &cleanup = Stack.push<T, A...>(::std::forward<A>(args)...);
    T &result = static_cast<T&>(initCleanup(cleanup, sizeof(T), state));
    
#ifndef NDEBUG
    auto newTop = Stack.begin(); ++newTop;
    assert(newTop == Stack.find(oldTop));
#endif
    return result;
  }
  template<class T, class... A>
  T &pushCleanup(A &&... args) {
    return pushCleanupInState<T, A...>(CleanupState::Active,
                                       ::std::forward<A>(args)...);
  }

  /// Set the state of the cleanup at the given depth.
  /// The transition must be non-trivial and legal.
  void setCleanupState(CleanupsDepth depth, CleanupState state);
  
  /// Scope RAII manager for local scopes.
  using Scope = ScopeImpl<&CleanupManager::InnermostScope>;
  /// Scope RAII manager for force-inlining.
  using InliningScope = ScopeImpl<&CleanupManager::ReturnScope>;
};

} // end namespace Lowering
} // end namespace swift

#endif

