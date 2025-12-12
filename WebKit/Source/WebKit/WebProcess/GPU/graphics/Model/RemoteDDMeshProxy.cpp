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
#include "RemoteDDMeshProxy.h"

#if ENABLE(GPU_PROCESS)

#include "ModelConvertToBackingContext.h"
#include "RemoteDDMeshMessages.h"
#include <WebCore/DDMeshDescriptor.h>
#include <WebCore/DDTextureDescriptor.h>
#include <WebCore/DDUpdateMeshDescriptor.h>
#include <WebCore/DDUpdateTextureDescriptor.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::DDModel {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteDDMeshProxy);

RemoteDDMeshProxy::RemoteDDMeshProxy(Ref<RemoteGPUProxy>&& root, ConvertToBackingContext& convertToBackingContext, DDModelIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_root(WTFMove(root))
{
}

RemoteDDMeshProxy::~RemoteDDMeshProxy()
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::Destruct());
    UNUSED_VARIABLE(sendResult);
#endif
}

void RemoteDDMeshProxy::addMesh(const WebCore::DDModel::DDMeshDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::AddMesh(descriptor));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteDDMeshProxy::update(const WebCore::DDModel::DDUpdateMeshDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::Update(descriptor));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteDDMeshProxy::render()
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::Render());
    UNUSED_PARAM(sendResult);
#endif
}

void RemoteDDMeshProxy::setLabelInternal(const String& label)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
#else
    UNUSED_PARAM(label);
#endif
}

void RemoteDDMeshProxy::addTexture(const WebCore::DDModel::DDTextureDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::AddTexture(descriptor));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteDDMeshProxy::updateTexture(const WebCore::DDModel::DDUpdateTextureDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::UpdateTexture(descriptor));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteDDMeshProxy::addMaterial(const WebCore::DDModel::DDMaterialDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::AddMaterial(descriptor));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void RemoteDDMeshProxy::updateMaterial(const WebCore::DDModel::DDUpdateMaterialDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto sendResult = send(Messages::RemoteDDMesh::UpdateMaterial(descriptor));
    UNUSED_PARAM(sendResult);
#else
    UNUSED_PARAM(descriptor);
#endif
}

}

#endif // ENABLE(GPU_PROCESS)
