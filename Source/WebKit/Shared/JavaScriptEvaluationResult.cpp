/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JavaScriptEvaluationResult.h"

#include "APIArray.h"
#include "APIDictionary.h"
#include "APIJSHandle.h"
#include "APINumber.h"
#include "APISerializedNode.h"
#include "APIString.h"
#include "WKSharedAPICast.h"
#include "WebFrame.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionDetails.h>
#include <WebCore/JSWebKitJSHandle.h>
#include <WebCore/JSWebKitSerializedNode.h>
#include <WebCore/ScriptWrappableInlines.h>
#include <WebCore/SerializedScriptValue.h>

namespace WebKit {

class JavaScriptEvaluationResult::JSExtractor {
public:
    Map takeMap() { return WTFMove(m_map); }
    std::optional<JSObjectID> addObjectToMap(JSGlobalContextRef, JSValueRef);
private:
    std::optional<Value> toValue(JSGlobalContextRef, JSValueRef);

    Map m_map;
    HashMap<Protected<JSValueRef>, JSObjectID> m_objectsInMap;
};

class JavaScriptEvaluationResult::JSInserter {
public:
    using Dictionaries = Vector<std::pair<ObjectMap, Protected<JSObjectRef>>>;
    using Arrays = Vector<std::pair<Vector<JSObjectID>, Protected<JSValueRef>>>;
    JSValueRef toJS(JSGlobalContextRef, Value&&);
    Dictionaries takeDictionaries() { return WTFMove(m_dictionaries); }
    Arrays takeArrays() { return WTFMove(m_arrays); }
private:
    Dictionaries m_dictionaries;
    Arrays m_arrays;
};

class JavaScriptEvaluationResult::APIExtractor {
public:
    Map takeMap() { return WTFMove(m_map); }
    JSObjectID addObjectToMap(API::Object&);
private:
    Value toValue(API::Object&);

    HashMap<Ref<API::Object>, JSObjectID> m_objectsInMap;
    Map m_map;
};

class JavaScriptEvaluationResult::APIInserter {
public:
    using Dictionaries = Vector<std::pair<ObjectMap, Ref<API::Dictionary>>>;
    using Arrays = Vector<std::pair<Vector<JSObjectID>, Ref<API::Array>>>;
    RefPtr<API::Object> toAPI(Value&&);
    Dictionaries takeDictionaries() { return WTFMove(m_dictionaries); }
    Arrays takeArrays() { return WTFMove(m_arrays); }
private:
    Dictionaries m_dictionaries;
    Arrays m_arrays;
};

JavaScriptEvaluationResult::JavaScriptEvaluationResult(JSObjectID root, Map&& map)
    : m_map(WTFMove(map))
    , m_root(root) { }

RefPtr<API::Object> JavaScriptEvaluationResult::APIInserter::toAPI(Value&& root)
{
    return WTF::switchOn(WTFMove(root), [] (EmptyType) -> RefPtr<API::Object> {
        return nullptr;
    }, [] (bool value) -> RefPtr<API::Object> {
        return API::Boolean::create(value);
    }, [] (double value) -> RefPtr<API::Object> {
        return API::Double::create(value);
    }, [] (String&& value) -> RefPtr<API::Object> {
        return API::String::create(value);
    }, [] (Seconds value) -> RefPtr<API::Object> {
        return API::Double::create(value.seconds());
    }, [&] (Vector<JSObjectID>&& vector) -> RefPtr<API::Object> {
        Ref array = API::Array::create();
        m_arrays.append({ WTFMove(vector), array });
        return { WTFMove(array) };
    }, [&] (ObjectMap&& map) -> RefPtr<API::Object> {
        Ref dictionary = API::Dictionary::create();
        m_dictionaries.append({ WTFMove(map), dictionary });
        return { WTFMove(dictionary) };
    }, [] (UniqueRef<JSHandleInfo>&& info) -> RefPtr<API::Object> {
        return API::JSHandle::create(WTFMove(info.get()));
    }, [] (UniqueRef<WebCore::SerializedNode>&& node) -> RefPtr<API::Object> {
        return API::SerializedNode::create(WTFMove(node.get()));
    });
}

static bool isSerializable(API::Object* object)
{
    if (!object)
        return false;

    switch (object->type()) {
    case API::Object::Type::String:
    case API::Object::Type::Boolean:
    case API::Object::Type::Double:
    case API::Object::Type::UInt64:
    case API::Object::Type::Int64:
    case API::Object::Type::JSHandle:
    case API::Object::Type::SerializedNode:
        return true;
    case API::Object::Type::Array:
        return std::ranges::all_of(downcast<API::Array>(object)->elements(), [] (const RefPtr<API::Object>& element) {
            return isSerializable(element.get());
        });
    case API::Object::Type::Dictionary:
        return std::ranges::all_of(downcast<API::Dictionary>(object)->map(), [] (const KeyValuePair<String, RefPtr<API::Object>>& pair) {
            return isSerializable(pair.value.get());
        });
    default:
        return false;
    }
}

auto JavaScriptEvaluationResult::APIExtractor::toValue(API::Object& object) -> Value
{
    switch (object.type()) {
    case API::Object::Type::String:
        return downcast<API::String>(object).string();
    case API::Object::Type::Boolean:
        return downcast<API::Boolean>(object).value();
    case API::Object::Type::Double:
        return downcast<API::Double>(object).value();
    case API::Object::Type::UInt64:
        return static_cast<double>(downcast<API::UInt64>(object).value());
    case API::Object::Type::Int64:
        return static_cast<double>(downcast<API::Int64>(object).value());
    case API::Object::Type::JSHandle:
        return makeUniqueRef<JSHandleInfo>(downcast<API::JSHandle>(object).info());
    case API::Object::Type::SerializedNode:
        return makeUniqueRef<WebCore::SerializedNode>(downcast<API::SerializedNode>(object).coreSerializedNode());
    case API::Object::Type::Array: {
        Vector<JSObjectID> vector;
        for (RefPtr element : downcast<API::Array>(object).elements()) {
            if (element)
                vector.append(addObjectToMap(*element));
        }
        return { WTFMove(vector) };
    }
    case API::Object::Type::Dictionary: {
        ObjectMap map;
        for (auto& [key, value] : downcast<API::Dictionary>(object).map()) {
            if (RefPtr protectedValue = value)
                map.set(addObjectToMap(API::String::create(key).get()), addObjectToMap(*protectedValue));
        }
        return { WTFMove(map) };
    }
    default:
        // This object has been null checked and went through isSerializable which only supports these types.
        ASSERT_NOT_REACHED();
        return EmptyType::Undefined;
    }
}

std::optional<JavaScriptEvaluationResult> JavaScriptEvaluationResult::extract(API::Object* object)
{
    if (!object)
        return jsUndefined();
    if (!isSerializable(object))
        return std::nullopt;
    APIExtractor extractor;
    auto root = extractor.addObjectToMap(*object);
    return JavaScriptEvaluationResult { root, extractor.takeMap() };
}

JSObjectID JavaScriptEvaluationResult::APIExtractor::addObjectToMap(API::Object& object)
{
    auto it = m_objectsInMap.find(object);
    if (it != m_objectsInMap.end())
        return it->value;

    auto identifier = JSObjectID::generate();
    m_objectsInMap.set(object, identifier);
    m_map.add(identifier, toValue(object));
    return identifier;
}

RefPtr<API::Object> JavaScriptEvaluationResult::toAPI()
{
    HashMap<JSObjectID, RefPtr<API::Object>> instantiatedObjects;
    APIInserter inserter;

    for (auto&& [identifier, value] : std::exchange(m_map, { }))
        instantiatedObjects.add(identifier, inserter.toAPI(WTFMove(value)));

    for (auto [vector, array] : inserter.takeArrays()) {
        for (auto identifier : vector) {
            if (RefPtr object = instantiatedObjects.get(identifier))
                Ref { array }->append(object.releaseNonNull());
        }
    }

    for (auto [map, dictionary] : inserter.takeDictionaries()) {
        for (auto [keyIdentifier, valueIdentifier] : map) {
            RefPtr key = dynamicDowncast<API::String>(instantiatedObjects.get(keyIdentifier));
            if (!key)
                continue;
            RefPtr value = instantiatedObjects.get(valueIdentifier);
            if (!value)
                continue;
            Ref { dictionary }->add(key->string(), WTFMove(value));
        }
    }

    return std::exchange(instantiatedObjects, { }).take(m_root);
}

std::optional<JSObjectID> JavaScriptEvaluationResult::JSExtractor::addObjectToMap(JSGlobalContextRef context, JSValueRef object)
{
    ASSERT(context);
    ASSERT(object);

    Protected<JSValueRef> jsValue(context, object);
    auto it = m_objectsInMap.find(jsValue);
    if (it != m_objectsInMap.end())
        return it->value;

    auto identifier = JSObjectID::generate();
    m_objectsInMap.set(WTFMove(jsValue), identifier);
    if (auto value = toValue(context, object)) {
        m_map.add(identifier, WTFMove(*value));
        return identifier;
    }
    m_objectsInMap.remove(Protected<JSValueRef>(context, object));
    return std::nullopt;
}

std::optional<JavaScriptEvaluationResult> JavaScriptEvaluationResult::extract(JSGlobalContextRef context, JSValueRef value)
{
    if (!context || !value) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    JSExtractor extractor;
    if (auto root = extractor.addObjectToMap(context, value))
        return JavaScriptEvaluationResult { *root, extractor.takeMap() };
    return std::nullopt;
}

// Similar to JSValue's valueToObjectWithoutCopy.
auto JavaScriptEvaluationResult::JSExtractor::toValue(JSGlobalContextRef context, JSValueRef value) -> std::optional<Value>
{
    using namespace WebCore;

    if (!JSValueIsObject(context, value)) {
        if (JSValueIsBoolean(context, value))
            return JSValueToBoolean(context, value);
        if (JSValueIsNumber(context, value)) {
            value = JSValueMakeNumber(context, JSValueToNumber(context, value, 0));
            return JSValueToNumber(context, value, 0);
        }
        if (JSValueIsString(context, value)) {
            auto* globalObject = ::toJS(context);
            JSC::JSValue jsValue = ::toJS(globalObject, value);
            return jsValue.toWTFString(globalObject);
        }
        if (JSValueIsNull(context, value))
            return EmptyType::Null;
        return EmptyType::Undefined;
    }

    JSObjectRef object = JSValueToObject(context, value, 0);
    JSC::JSGlobalObject* globalObject = ::toJS(context);
    JSC::JSObject* jsObject = ::toJS(globalObject, object).toObject(globalObject);

    if (auto* info = jsDynamicCast<JSWebKitJSHandle*>(jsObject)) {
        RELEASE_ASSERT(globalObject->template inherits<WebCore::JSDOMGlobalObject>());
        auto* domGlobalObject = jsCast<WebCore::JSDOMGlobalObject*>(globalObject);
        RefPtr document = dynamicDowncast<Document>(domGlobalObject->scriptExecutionContext());
        RefPtr frame = WebFrame::webFrame(document->frameID());
        Ref ref { info->wrapped() };
        return makeUniqueRef<JSHandleInfo>(ref->identifier(), frame->info(), ref->windowFrameIdentifier());
    }

    if (auto* node = jsDynamicCast<JSWebKitSerializedNode*>(jsObject)) {
        Ref serializedNode { node->wrapped() };
        return makeUniqueRef<SerializedNode>(serializedNode->serializedNode());
    }

    if (JSValueIsDate(context, object))
        return Seconds(JSValueToNumber(context, object, 0) / 1000.0);

    if (JSValueIsArray(context, object)) {
        SUPPRESS_UNCOUNTED_ARG JSValueRef lengthPropertyName = JSValueMakeString(context, adopt(JSStringCreateWithUTF8CString("length")).get());
        JSValueRef lengthValue = JSObjectGetPropertyForKey(context, object, lengthPropertyName, nullptr);
        double lengthDouble = JSValueToNumber(context, lengthValue, nullptr);
        if (lengthDouble < 0 || lengthDouble > static_cast<double>(std::numeric_limits<size_t>::max()))
            return EmptyType::Undefined;

        size_t length = lengthDouble;
        Vector<JSObjectID> vector;
        if (!vector.tryReserveInitialCapacity(length))
            return EmptyType::Undefined;

        for (size_t i = 0; i < length; ++i) {
            if (auto identifier = addObjectToMap(context, JSObjectGetPropertyAtIndex(context, object, i, nullptr)))
                vector.append(*identifier);
        }
        return { WTFMove(vector) };
    }

    switch (SerializedScriptValue::deserializationBehavior(*jsObject)) {
    case SerializedScriptValue::DeserializationBehavior::Fail:
        return std::nullopt;
    case SerializedScriptValue::DeserializationBehavior::Succeed:
        break;
    case SerializedScriptValue::DeserializationBehavior::LegacyMapToNull:
        return EmptyType::Null;
    case SerializedScriptValue::DeserializationBehavior::LegacyMapToUndefined:
        return EmptyType::Undefined;
    case SerializedScriptValue::DeserializationBehavior::LegacyMapToEmptyObject:
        return { { ObjectMap { } } };
    }

    JSPropertyNameArrayRef names = JSObjectCopyPropertyNames(context, object);
    size_t length = JSPropertyNameArrayGetCount(names);
    ObjectMap map;
    for (size_t i = 0; i < length; i++) {
        JSRetainPtr<JSStringRef> key = JSPropertyNameArrayGetNameAtIndex(names, i);
        SUPPRESS_UNCOUNTED_ARG auto keyID = addObjectToMap(context, JSValueMakeString(context, key.get()));
        SUPPRESS_UNCOUNTED_ARG auto valueID = addObjectToMap(context, JSObjectGetPropertyForKey(context, object, JSValueMakeString(context, key.get()), nullptr));
        if (keyID && valueID)
            map.add(*keyID, *valueID);
    }
    JSPropertyNameArrayRelease(names);
    return { WTFMove(map) };
}

JSValueRef JavaScriptEvaluationResult::JSInserter::toJS(JSGlobalContextRef context, Value&& root)
{
    auto globalObjectTuple = [] (auto context) {
        auto* lexicalGlobalObject = ::toJS(context);
        RELEASE_ASSERT(lexicalGlobalObject->template inherits<WebCore::JSDOMGlobalObject>());
        auto* domGlobalObject = jsCast<WebCore::JSDOMGlobalObject*>(lexicalGlobalObject);
        RefPtr document = dynamicDowncast<WebCore::Document>(domGlobalObject->scriptExecutionContext());
        RELEASE_ASSERT(document);
        return std::make_tuple(lexicalGlobalObject, domGlobalObject, WTFMove(document));
    };

    return WTF::switchOn(WTFMove(root), [&] (EmptyType emptyType) -> JSValueRef {
        switch (emptyType) {
        case EmptyType::Undefined:
            return JSValueMakeUndefined(context);
        case EmptyType::Null:
            return JSValueMakeNull(context);
        }
    }, [&] (bool value) -> JSValueRef {
        return JSValueMakeBoolean(context, value);
    }, [&] (double value) -> JSValueRef {
        return JSValueMakeNumber(context, value);
    }, [&] (String&& value) -> JSValueRef {
        auto string = OpaqueJSString::tryCreate(WTFMove(value));
        return JSValueMakeString(context, string.get());
    }, [&] (Seconds value) -> JSValueRef {
        JSValueRef argument = JSValueMakeNumber(context, value.value() * 1000.0);
        return JSObjectMakeDate(context, 1, &argument, 0);
    }, [&] (Vector<JSObjectID>&& vector) -> JSValueRef {
        JSValueRef array = JSObjectMakeArray(context, 0, nullptr, 0);
        m_arrays.append({ WTFMove(vector), Protected<JSValueRef>(context, array) });
        return array;
    }, [&] (ObjectMap&& map) -> JSValueRef {
        JSObjectRef dictionary = JSObjectMake(context, 0, 0);
        m_dictionaries.append({ WTFMove(map), Protected<JSObjectRef>(context, dictionary) });
        return dictionary;
    }, [&] (UniqueRef<JSHandleInfo>&& info) -> JSValueRef {
        auto [originalDocument, object] = WebCore::WebKitJSHandle::objectForIdentifier(info.get().identifier);
        if (!object)
            return JSValueMakeUndefined(context);
        auto [lexicalGlobalObject, domGlobalObject, document] = globalObjectTuple(context);
        if (document.get() != originalDocument.get())
            return JSValueMakeUndefined(context);
        return ::toRef(object);
    }, [&] (UniqueRef<WebCore::SerializedNode>&& serializedNode) -> JSValueRef {
        auto [lexicalGlobalObject, domGlobalObject, document] = globalObjectTuple(context);
        return ::toRef(lexicalGlobalObject, WebCore::SerializedNode::deserialize(WTFMove(serializedNode.get()), lexicalGlobalObject, domGlobalObject, *document));
    });
}

Protected<JSValueRef> JavaScriptEvaluationResult::toJS(JSGlobalContextRef context)
{
    HashMap<JSObjectID, Protected<JSValueRef>> instantiatedJSObjects;
    JSInserter inserter;

    for (auto&& [identifier, value] : std::exchange(m_map, { }))
        instantiatedJSObjects.add(identifier, Protected<JSValueRef>(context, inserter.toJS(context, WTFMove(value))));

    for (auto& [vector, array] : inserter.takeArrays()) {
        JSObjectRef jsArray = JSValueToObject(context, array.get(), 0);
        for (size_t index = 0; index < vector.size(); ++index) {
            auto identifier = vector[index];
            if (Protected<JSValueRef> element = instantiatedJSObjects.get(identifier))
                JSObjectSetPropertyAtIndex(context, jsArray, index, element.get(), 0);
        }
    }

    for (auto& [map, dictionary] : inserter.takeDictionaries()) {
        for (auto [keyIdentifier, valueIdentifier] : map) {
            Protected<JSValueRef> key = instantiatedJSObjects.get(keyIdentifier);
            if (!key)
                continue;
            ASSERT(JSValueIsString(context, key.get()));
            SUPPRESS_UNCOUNTED_ARG auto keyString = adopt(JSValueToStringCopy(context, key.get(), nullptr));
            if (!keyString)
                continue;
            Protected<JSValueRef> value = instantiatedJSObjects.get(valueIdentifier);
            if (!value)
                continue;
            SUPPRESS_UNCOUNTED_ARG JSObjectSetProperty(context, dictionary.get(), keyString.get(), value.get(), 0, 0);
        }
    }

    return std::exchange(instantiatedJSObjects, { }).take(m_root);
}

JavaScriptEvaluationResult::JavaScriptEvaluationResult(JavaScriptEvaluationResult&&) = default;

JavaScriptEvaluationResult& JavaScriptEvaluationResult::operator=(JavaScriptEvaluationResult&&) = default;

JavaScriptEvaluationResult::~JavaScriptEvaluationResult() = default;

String JavaScriptEvaluationResult::toString() const
{
    auto it = m_map.find(m_root);
    if (it == m_map.end())
        return { };
    auto* string = std::get_if<String>(&it->value);
    if (!string)
        return { };
    return *string;
}

JavaScriptEvaluationResult JavaScriptEvaluationResult::jsUndefined()
{
    auto root = JSObjectID::generate();
    Map map;
    map.set(root, EmptyType::Undefined);
    return { root, WTFMove(map) };
}

} // namespace WebKit
