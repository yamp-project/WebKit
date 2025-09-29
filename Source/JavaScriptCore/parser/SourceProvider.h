/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <JavaScriptCore/CachedBytecode.h>
#include <JavaScriptCore/CodeBlockHash.h>
#include <JavaScriptCore/CodeSpecializationKind.h>
#include <JavaScriptCore/SourceOrigin.h>
#include <JavaScriptCore/SourceTaintedOrigin.h>
#include <wtf/RefCounted.h>
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

namespace JSC {

class SourceCode;
class UnlinkedFunctionExecutable;
class UnlinkedFunctionCodeBlock;

enum class SourceProviderSourceType : uint8_t {
    Program,
    Module,
    WebAssembly,
    JSON,
    ImportMap,
};

using BytecodeCacheGenerator = Function<RefPtr<CachedBytecode>()>;

class SourceProvider : public ThreadSafeRefCounted<SourceProvider> {
public:
    static const intptr_t nullID = 1;

    JS_EXPORT_PRIVATE SourceProvider(const SourceOrigin&, String&& sourceURL, String&& preRedirectURL, SourceTaintedOrigin, const TextPosition& startPosition, SourceProviderSourceType);

    JS_EXPORT_PRIVATE virtual ~SourceProvider();

    virtual unsigned hash() const = 0;
    virtual StringView source() const = 0;
    virtual RefPtr<CachedBytecode> cachedBytecode() const { return nullptr; }
    virtual void cacheBytecode(const BytecodeCacheGenerator&) const { }
    virtual void updateCache(const UnlinkedFunctionExecutable*, const SourceCode&, CodeSpecializationKind, const UnlinkedFunctionCodeBlock*) const { }
    virtual void commitCachedBytecode() const { }

    StringView getRange(int start, int end) const LIFETIME_BOUND
    {
        return source().substring(start, end - start);
    }

    const SourceOrigin& sourceOrigin() const { return m_sourceOrigin; }

    // This is NOT the path that should be used for computing relative paths from a script. Use SourceOrigin's URL for that, the values may or may not be the same...
    const String& sourceURL() const { return m_sourceURL; }
    const String& sourceURLStripped();
    const String& preRedirectURL() const { return m_preRedirectURL; }
    const String& sourceURLDirective() const { return m_sourceURLDirective; }
    const String& sourceMappingURLDirective() const { return m_sourceMappingURLDirective; }

    TextPosition startPosition() const { return m_startPosition; }
    SourceProviderSourceType sourceType() const { return m_sourceType; }
    bool isModuleType() const { return m_sourceType == SourceProviderSourceType::Module || m_sourceType == SourceProviderSourceType::JSON; }

    SourceID asID()
    {
        if (!m_id)
            getID();
        return m_id;
    }

    void setSourceURLDirective(const String& sourceURLDirective) { m_sourceURLDirective = sourceURLDirective; }
    void setSourceMappingURLDirective(const String& sourceMappingURLDirective) { m_sourceMappingURLDirective = sourceMappingURLDirective; }
    void setSourceTaintedOrigin(SourceTaintedOrigin taintedness) { m_taintedness = taintedness; }

    SourceTaintedOrigin sourceTaintedOrigin() const { return m_taintedness; }
    bool couldBeTainted() const { return m_taintedness != SourceTaintedOrigin::Untainted; }

    JS_EXPORT_PRIVATE void lockUnderlyingBuffer();
    JS_EXPORT_PRIVATE void unlockUnderlyingBuffer();
    JS_EXPORT_PRIVATE virtual CodeBlockHash codeBlockHashConcurrently(int startOffset, int endOffset, CodeSpecializationKind);

private:
    JS_EXPORT_PRIVATE virtual void lockUnderlyingBufferImpl();
    JS_EXPORT_PRIVATE virtual void unlockUnderlyingBufferImpl();

    JS_EXPORT_PRIVATE void getID();

    std::atomic<unsigned> m_lockingCount { 0 };
    SourceProviderSourceType m_sourceType;
    SourceOrigin m_sourceOrigin;
    String m_sourceURL;
    String m_sourceURLStripped;
    String m_preRedirectURL;
    String m_sourceURLDirective;
    String m_sourceMappingURLDirective;
    TextPosition m_startPosition;
    SourceID m_id { 0 };
    SourceTaintedOrigin m_taintedness;
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StringSourceProvider);
class StringSourceProvider : public SourceProvider {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StringSourceProvider, StringSourceProvider);
public:
    static Ref<StringSourceProvider> create(const String& source, const SourceOrigin& sourceOrigin, String sourceURL, SourceTaintedOrigin taintedness, const TextPosition& startPosition = TextPosition(), SourceProviderSourceType sourceType = SourceProviderSourceType::Program)
    {
        return adoptRef(*new StringSourceProvider(source, sourceOrigin, taintedness, WTFMove(sourceURL), startPosition, sourceType));
    }

    unsigned hash() const override
    {
        return m_source.get().hash();
    }

    StringView source() const override
    {
        return m_source.get();
    }

protected:
    StringSourceProvider(const String& source, const SourceOrigin& sourceOrigin, SourceTaintedOrigin taintedness, String&& sourceURL, const TextPosition& startPosition, SourceProviderSourceType sourceType)
        : SourceProvider(sourceOrigin, WTFMove(sourceURL), String(), taintedness, startPosition, sourceType)
        , m_source(source.isNull() ? *StringImpl::empty() : *source.impl())
    {
    }

private:
    const Ref<StringImpl> m_source;
};

#if ENABLE(WEBASSEMBLY)
class BaseWebAssemblySourceProvider : public SourceProvider {
public:
    virtual const uint8_t* data() = 0;
    virtual size_t size() const = 0;
protected:
    JS_EXPORT_PRIVATE BaseWebAssemblySourceProvider(const SourceOrigin&, String&& sourceURL);
};

class WebAssemblySourceProvider final : public BaseWebAssemblySourceProvider {
public:
    static Ref<WebAssemblySourceProvider> create(Vector<uint8_t>&& data, const SourceOrigin& sourceOrigin, String sourceURL)
    {
        return adoptRef(*new WebAssemblySourceProvider(WTFMove(data), sourceOrigin, WTFMove(sourceURL)));
    }

    unsigned hash() const final
    {
        return m_source.impl()->hash();
    }

    StringView source() const final
    {
        return m_source;
    }

    const uint8_t* data() final
    {
        return m_data.span().data();
    }

    size_t size() const final
    {
        return m_data.size();
    }

    const Vector<uint8_t>& dataVector() const
    {
        return m_data;
    }

private:
    WebAssemblySourceProvider(Vector<uint8_t>&& data, const SourceOrigin& sourceOrigin, String&& sourceURL)
        : BaseWebAssemblySourceProvider(sourceOrigin, WTFMove(sourceURL))
        , m_source("[WebAssembly source]"_s)
        , m_data(WTFMove(data))
    {
    }

    String m_source;
    Vector<uint8_t> m_data;
};
#endif

// RAII class for managing a source provider's underlying buffer.
class SourceProviderBufferGuard {
public:
    explicit SourceProviderBufferGuard(SourceProvider* sourceProvider)
        : m_sourceProvider(sourceProvider)
    {
        if (m_sourceProvider)
            m_sourceProvider->lockUnderlyingBuffer();
    }

    ~SourceProviderBufferGuard()
    {
        if (m_sourceProvider)
            m_sourceProvider->unlockUnderlyingBuffer();
    }

    SourceProvider* provider() { return m_sourceProvider; }

private:
    // This must not be RefPtr. It is possible that this is used by the concurrent compiler and
    // we are ensuring that this does not go away with different mechanism. But SourceProvider etc. can have main-thread-only affinity.
    SourceProvider* m_sourceProvider { nullptr };
};

} // namespace JSC
