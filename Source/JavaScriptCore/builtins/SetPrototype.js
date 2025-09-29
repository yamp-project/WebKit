/*
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

function forEach(callback /*, thisArg */)
{
    "use strict";

    if (!@isSet(this))
        @throwTypeError("Set operation called on non-Set object");

    if (!@isCallable(callback))
        @throwTypeError("Set.prototype.forEach callback must be a function");

    var thisArg = @argument(1);
    var storage = @setStorage(this);
    var entry = 0;

    do {
        storage = @setIterationNext(storage, entry);
        if (storage == @orderedHashTableSentinel)
            break;
        entry = @setIterationEntry(storage) + 1;
        var key = @setIterationEntryKey(storage);

        callback.@call(thisArg, key, key, this);
    } while (true);
}

// https://tc39.es/proposal-set-methods/#sec-getsetrecord (steps 1-7)
@linkTimeConstant
@alwaysInline
function getSetSizeAsInt(other)
{
    if (!@isObject(other))
        @throwTypeError("Set operation expects first argument to be an object");

    var size = @toNumber(other.size);
    if (size !== size) // is NaN?
        @throwTypeError("Set operation expects first argument to have non-NaN 'size' property");

    var sizeInt = @toIntegerOrInfinity(size);
    if (sizeInt < 0)
        @throwRangeError("Set operation expects first argument to have non-negative 'size' property");

    return sizeInt;
}
