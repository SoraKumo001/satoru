mergeInto(LibraryManager.library, {
  saveSetjmp: function (env, label, stackPtr) {
    return _emscripten_setjmp(env, label, stackPtr);
  },
  testSetjmp: function (env, label) {
    return _emscripten_test_setjmp(env, label);
  },
});
