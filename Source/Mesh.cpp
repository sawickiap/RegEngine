#include "BaseUtils.hpp"
#include "Mesh.hpp"
#include "Renderer.hpp"

static const D3D12_INPUT_ELEMENT_DESC g_MeshInputElements[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

const D3D12_INPUT_ELEMENT_DESC* Vertex::GetInputElements()
{
    return g_MeshInputElements;
}

const uint32_t Vertex::GetInputElementCount()
{
    return (uint32_t)_countof(g_MeshInputElements);
}

void Mesh::Init(
    const wstr_view& name,
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType,
    D3D12_PRIMITIVE_TOPOLOGY topology,
    std::span<const Vertex> vertices,
    std::span<const IndexType> indices)
{
    assert(!m_VertexBuffer);
    assert(vertices.size() > 0 && vertices.data());

    m_TopologyType = topologyType;
    m_Topology = topology;
    m_VertexCount = (uint32_t)vertices.size();
    m_IndexCount = (uint32_t)indices.size();

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

    // Create vertex buffer.
    {
        const UINT64 vbSize = m_VertexCount * sizeof(Vertex);
        D3D12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
        CHECK_HR(g_Renderer->GetMemoryAllocator()->CreateResource(
            &allocDesc,
            &vbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            &m_VertexBuffer,
            IID_NULL, NULL)); // riidResource, ppvResource
        SetD3D12ObjectName(m_VertexBuffer->GetResource(), std::format(L"{} - vertex buffer", name));
    }

    // Map and fill vertex buffer.
    {
        CD3DX12_RANGE readEmptyRange{0, 0};
        void* vbMappedPtr = nullptr;
        CHECK_HR(m_VertexBuffer->GetResource()->Map(0, D3D12_RANGE_NONE, &vbMappedPtr));
        memcpy(vbMappedPtr, vertices.data(), vertices.size() * sizeof(Vertex));
        m_VertexBuffer->GetResource()->Unmap(0, D3D12_RANGE_ALL);
    }

    if(m_IndexCount > 0)
    {
        assert(indices.data());
        // Create index buffer.
        {
            const UINT64 ibSize = m_IndexCount * sizeof(IndexType);
            D3D12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
            CHECK_HR(g_Renderer->GetMemoryAllocator()->CreateResource(
                &allocDesc,
                &ibDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, // pOptimizedClearValue
                &m_IndexBuffer,
                IID_NULL, NULL)); // riidResource, ppvResource
            SetD3D12ObjectName(m_IndexBuffer->GetResource(), std::format(L"{} - index buffer", name));
        }

        // Map and fill vertex buffer.
        {
            CD3DX12_RANGE readEmptyRange{0, 0};
            void* ibMappedPtr = nullptr;
            CHECK_HR(m_IndexBuffer->GetResource()->Map(0, D3D12_RANGE_NONE, &ibMappedPtr));
            memcpy(ibMappedPtr, indices.data(), indices.size() * sizeof(IndexType));
            m_IndexBuffer->GetResource()->Unmap(0, D3D12_RANGE_ALL);
        }
    }
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView() const
{
    assert(m_VertexBuffer);
    return D3D12_VERTEX_BUFFER_VIEW{
        m_VertexBuffer->GetResource()->GetGPUVirtualAddress(),
        (UINT)(m_VertexCount * sizeof(Vertex)), // SizeInBytes
        sizeof(Vertex) }; // StrideInBytes
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView() const
{
    assert(m_IndexBuffer);
    return D3D12_INDEX_BUFFER_VIEW{
        m_IndexBuffer->GetResource()->GetGPUVirtualAddress(),
        (UINT)(m_IndexCount * sizeof(IndexType)), // SizeInBytes
        s_IndexFormat };
}
