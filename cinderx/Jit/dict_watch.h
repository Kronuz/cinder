// Copyright (c) Meta Platforms, Inc. and affiliates. (http://www.meta.com)

#pragma once

#include "Python.h"

#include "cinderx/Jit/global_cache.h"

namespace jit {

// Checks if a given key of a dict is watched by the given cache.
bool isWatchedDictKey(PyObject* dict, PyObject* key, GlobalCache cache);

// Watch the given key of the given dict. The cache's update() method will be
// called when the key's value in the dict is changed or removed. The cache's
// disable() method will be called if the dict becomes unwatchable.
void watchDictKey(PyObject* dict, PyObject* key, GlobalCache cache);

// Unsubscribe from the given key of the given dict.
void unwatchDictKey(PyObject* dict, PyObject* key, GlobalCache cache);

// Called when the value at a key is modified (value will contain the new
// value) or deleted (value will be nullptr).
void notifyDictKey(PyObject* dict, PyObject* key, PyObject* value);

// Called when a dict is cleared, rather than sending individual notifications
// for every key. The dict is still in a watched state, and further callbacks
// for it will be invoked as appropriate.
void notifyDictClear(PyObject* dict);

// Called when a dict has changed in a way that is incompatible with watching,
// or is about to be freed. No more callbacks will be invoked for this dict.
void notifyDictUnwatch(PyObject* dict);

} // namespace jit
