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

#include "config.h"
#include "WasmDebugServer.h"

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "CallFrame.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "Options.h"
#include "VM.h"
#include "WasmBreakpointManager.h"
#include "WasmExecutionHandler.h"
#include "WasmIPIntSlowPaths.h"
#include "WasmMemoryHandler.h"
#include "WasmModule.h"
#include "WasmModuleManager.h"
#include "WasmQueryHandler.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <wtf/Compiler.h>
#if OS(WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <wtf/Assertions.h>
#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/Threading.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {
namespace Wasm {

static inline StringView getErrorReply(ProtocolError error)
{
    switch (error) {
    case ProtocolError::InvalidPacket:
        return "E01"_s;
    case ProtocolError::InvalidAddress:
        return "E02"_s;
    case ProtocolError::InvalidRegister:
        return "E03"_s;
    case ProtocolError::MemoryError:
        return "E04"_s;
    case ProtocolError::UnknownCommand:
        return "E05"_s;
    default:
        return "E00"_s;
    }
}

DebugServer& DebugServer::singleton()
{
    static NeverDestroyed<DebugServer> instance;
    return instance.get();
}

DebugServer::DebugServer()
    : m_queryHandler(makeUnique<QueryHandler>(*this))
    , m_memoryHandler(makeUnique<MemoryHandler>(*this))
{
}

bool DebugServer::start(VM* vm)
{
    if (isState(State::Running) || isState(State::Starting)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Server already running or is starting");
        return true;
    }

    setState(State::Starting);

    if (!createAndBindServerSocket())
        return false;

    RELEASE_ASSERT(isSocketValid(m_serverSocket));
    auto ownerThread = vm->ownerThread();
    if (!ownerThread || !*ownerThread) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] VM has no valid owner thread");
        closeSocket(m_serverSocket);
        return false;
    }

    m_vm = vm;
    m_mutatorThreadId = (*ownerThread)->uid();

    m_instanceManager = makeUnique<ModuleManager>(*vm);
    m_breakpointManager = makeUnique<BreakpointManager>();
    m_executionHandler = makeUnique<ExecutionHandler>(*this, *m_instanceManager, *m_breakpointManager);

    startAcceptThread();

    setState(State::Running);
    return true;
}

void DebugServer::stop()
{
    if (isState(State::Stopped) || isState(State::Stopping)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Server already stopped or is stopping");
        return;
    }

    setState(State::Stopping);

    closeSocket(m_serverSocket);
    closeSocket(m_clientSocket);
    if (RefPtr thread = m_acceptThread) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Waiting for accept thread to terminate...");
        thread->waitForCompletion();
        m_acceptThread = nullptr;
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Accept thread terminated");
    }

    // FIXME: Here we just enforce resetting everything.
    resetAll();

    setState(State::Stopped);
}

void DebugServer::setState(State state)
{
    switch (state) {
    case State::Stopped:
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] LLDB Server is stopped");
        break;
    case State::Starting:
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Starting LLDB Server...");
        break;
    case State::Running:
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] WASM Debug Server listening. Connect with: lldb -o 'gdb-remote localhost:", m_port);
        break;
    case State::Stopping:
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Stopping LLDB Server...");
        break;
    }
    m_state.store(state);
}

bool DebugServer::isState(State state) const
{
    bool result = m_state.load() == state;
    if (result && state == State::Running)
        RELEASE_ASSERT(isSocketValid(m_serverSocket));
    return m_state.load() == state;
}

void DebugServer::resetAll()
{
    m_port = defaultPort;
    closeSocket(m_serverSocket);
    closeSocket(m_clientSocket);
    m_noAckMode = false;

    m_vm = nullptr;
    m_mutatorThreadId = 0;
    m_debugServerThreadId = 0;

    m_queryHandler = nullptr;
    m_memoryHandler = nullptr;
    m_executionHandler = nullptr;

    m_instanceManager = nullptr;
    m_breakpointManager = nullptr;
    m_acceptThread = nullptr;
}

bool DebugServer::needToHandleBreakpoints() const
{
    return isConnected() && m_breakpointManager && m_breakpointManager->hasBreakpoints();
}

union SocketAddress {
    sockaddr_in in;
    sockaddr generic;

    SocketAddress()
        : in {}
    {
    }
    explicit SocketAddress(const sockaddr_in& addr)
        : in(addr)
    {
    }
};

bool DebugServer::createAndBindServerSocket()
{
    // 1. Create socket
    m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (!isSocketValid(m_serverSocket)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Failed to create socket");
        return false;
    }

    // 2. Set socket options for better reusability
    int opt = 1;
#if OS(WINDOWS)
    const char* optPtr = reinterpret_cast<const char*>(&opt);
#else
    const void* optPtr = &opt;
#endif
    if (setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, optPtr, sizeof(opt)) < 0) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Warning: Failed to set SO_REUSEADDR");
        // Continue anyway, this is not critical
    }

    // 3. Bind to address and port
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_port);
    SocketAddress bindAddress(address);
    if (bind(m_serverSocket, &bindAddress.generic, sizeof(sockaddr_in)) < 0) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Failed to bind socket to port ", m_port);
        closeSocket(m_serverSocket);
        return false;
    }

    // 4. Start listening
    if (listen(m_serverSocket, 1) < 0) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Failed to listen on socket");
        closeSocket(m_serverSocket);
        return false;
    }

    return true;
}

void DebugServer::startAcceptThread()
{
    m_acceptThread = WTF::Thread::create("WasmDebugServer", [this]() {
        m_debugServerThreadId = Thread::currentSingleton().uid();

        while (isState(State::Running)) {
            dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Waiting for client connections...");
            SocketAddress clientAddress;
            socklen_t clientLen = sizeof(clientAddress.in);
            SocketType clientSocket = accept(m_serverSocket, &clientAddress.generic, &clientLen);
            if (isSocketValid(clientSocket)) {
                m_clientSocket = clientSocket;
                handleClient();
            } else
                dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Accept failed, continuing...");
        }
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Accept thread exiting");
    });
}

void DebugServer::closeSocket(SocketType& socket)
{
    ASSERT(&socket == &m_serverSocket || &socket == &m_clientSocket);
    if (isSocketValid(socket)) {
#if OS(WINDOWS)
        ::closesocket(socket);
#else
        ::close(socket);
#endif
        socket = invalidSocketValue;
    }
}

void DebugServer::handleClient()
{
    RELEASE_ASSERT(isSocketValid(m_clientSocket));

    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] LLDB connected, starting client handler - process continues running normally");

    // Send initial acknowledgment - LLDB expects this immediately
    sendAck();

    constexpr size_t INITIAL_RECV_BUFFER_SIZE = 4096;
    auto receiveBuffer = makeUniqueArray<char>(INITIAL_RECV_BUFFER_SIZE);

    while (true) {
        auto bytesRead = recv(m_clientSocket, receiveBuffer.get(), INITIAL_RECV_BUFFER_SIZE - 1, 0);
        if (bytesRead <= 0) {
            dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Client disconnected (bytesRead=", bytesRead, ")");
            break;
        }

        StringView data(receiveBuffer.get(), bytesRead, true);
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Received raw: ", data, " (", bytesRead, " bytes)");

        if (bytesRead == 1) {
            // Handle interrupt character (Reference [1] in wasm/debugger/README.md)
            if (data[0] == 0x03) {
                dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Received Ctrl+C interrupt - triggering stack overflow");
                m_executionHandler->interrupt();
                continue;
            }

            // Handle ACK/NACK characters (Reference [2] in wasm/debugger/README.md)
            if (data[0] == '+' || data[0] == '-')
                continue;
        }

        // Handle packet format: $<data>#<checksum>
        Vector<StringView> parts = splitWithDelimiters(data, "$#"_s);
        if (parts.size() != 3)
            continue;
        handlePacket(parts[1]);
    }

    // FIXME: Currently client disconnect, kill, and quit commands just stop the client session only for easy debugging purposes.
    // Eventually we need to introduce various stop states, e.g., termination.
    m_executionHandler->reset();
    m_breakpointManager->clearAllBreakpoints();
    closeSocket(m_clientSocket);
    m_noAckMode = false;
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] LLDB disconnected");
}

void DebugServer::handlePacket(StringView packet)
{
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Processing packet: ", packet);

    sendAck();

    if (packet.isEmpty()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Empty packet received");
        sendErrorReply(ProtocolError::InvalidPacket);
        return;
    }

    switch (packet[0]) {
    case 'q':
    case 'Q':
    case 'j':
        // Handle all query packets (q*, Q*) and JSON packets (j*)
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Routing query packet to QueryHandler");
        m_queryHandler->handleGeneralQuery(packet);
        break;
    // See reference [3] in wasm/debugger/README.md
    case 'm':
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Routing memory read packet to MemoryHandler");
        m_memoryHandler->read(packet);
        break;
    case 'M':
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Routing memory write packet to MemoryHandler");
        m_memoryHandler->write(packet);
        break;
    case 'c':
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Routing continue packet to ExecutionHandler");
        m_executionHandler->resume();
        break;
    case 's':
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Routing legacy step packet to ExecutionHandler");
        m_executionHandler->step();
        break;
    case 'Z':
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Routing set breakpoint packet to ExecutionHandler");
        m_executionHandler->setBreakpoint(packet);
        break;
    case 'z':
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Routing remove breakpoint packet to ExecutionHandler");
        m_executionHandler->removeBreakpoint(packet);
        break;
    case '?':
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Routing halt reason query to ExecutionHandler");
        m_executionHandler->interrupt();
        break;
    case 'k':
        dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Kill/detach request");
        closeSocket(m_clientSocket);
        break;
    default:
        sendReplyNotSupported(packet);
    }
}

void DebugServer::sendReply(StringView reply) { m_executionHandler->sendReply(reply); }

void DebugServer::sendAck()
{
    // Send '+' ACK character to acknowledge packet receipt
    // Reference: [2] in wasm/debugger/README.md
    if (m_noAckMode)
        return;
    sendReply("+"_s);
}

void DebugServer::sendReplyOK()
{
    // Send 'OK' reply to indicate successful completion
    // Reference: [3] and [4] in wasm/debugger/README.md
    sendReply("OK"_s);
}

void DebugServer::sendReplyNotSupported(StringView packet)
{
    // Send empty reply to indicate feature/command not supported
    // Reference: [5] in wasm/debugger/README.md
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Unsupported packet: ", packet);
    sendReply(""_s);
}

void DebugServer::sendErrorReply(ProtocolError error)
{
    // Send 'E NN' error reply with specific error code
    // Reference: [5] in wasm/debugger/README.md
    sendReply(getErrorReply(error));
}

void DebugServer::trackInstance(JSWebAssemblyInstance* instance)
{
    if (!m_instanceManager)
        return;
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Tracking WebAssembly instance: ", RawPointer(instance));
    uint32_t instanceId = m_instanceManager->registerInstance(instance);
    if (isConnected()) {
        UNUSED_VARIABLE(instanceId);
        // FIXME: Should notify LLDB with new module library.
    }
}

void DebugServer::trackModule(Module& module)
{
    if (!m_instanceManager)
        return;
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Tracking WebAssembly module: ", RawPointer(&module));
    uint32_t moduleId = m_instanceManager->registerModule(module);
    if (isConnected()) {
        UNUSED_VARIABLE(moduleId);
        // FIXME: Should notify LLDB with new module library.
    }
}

void DebugServer::untrackModule(Module& module)
{
    if (!m_instanceManager)
        return;
    dataLogLnIf(Options::verboseWasmDebugger(), "[Debugger] Untracking WebAssembly module: ", RawPointer(&module));
    m_instanceManager->unregisterModule(module);
}

bool DebugServer::interruptRequested() const { return m_vm && m_vm->isWasmStopWorldActive(); }

bool DebugServer::stopCode(CallFrame* callFrame, JSWebAssemblyInstance* instance, IPIntCallee* callee, uint8_t* pc, uint8_t* mc, IPInt::IPIntLocal* locals, IPInt::IPIntStackEntry* stack) { return m_executionHandler->stopCode(callFrame, instance, callee, pc, mc, locals, stack); }

void DebugServer::setInterruptBreakpoint(JSWebAssemblyInstance* instance, IPIntCallee* callee) { return m_executionHandler->setInterruptBreakpoint(instance, callee); }

}
} // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
