mergeInto(LibraryManager.library, {
  saveSetjmp: function(env, label, stackPtr) {
    // 最新の Emscripten では内部的な setjmp 処理に委譲
    return _emscripten_setjmp(env, label, stackPtr);
  },
  testSetjmp: function(env, label) {
    return _emscripten_test_setjmp(env, label);
  }
});