#include "BaseUtils.hpp"
#include "Mesh.hpp"
#include "Renderer.hpp"

static const D3D12_INPUT_ELEMENT_DESC g_MeshInputElements[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType,
    D3D12_PRIMITIVE_TOPOLOGY topology,
    const Vertex* vertices,
    uint32_t vertexCount,
    const IndexType* indices,
    uint32_t indexCount)
{
    assert(!m_VertexBuffer);
    assert(vertexCount > 0 && vertices);

    m_TopologyType = topologyType;
    m_Topology = topology;
    m_VertexCount = vertexCount;
    m_IndexCount = indexCount;

    const D3D12_HEAP_PROPERTIES uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Create vertex buffer.
    {
        const UINT64 vbSize = vertexCount * sizeof(Vertex);
        D3D12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
        CHECK_HR(g_Renderer->GetDevice()->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            IID_PPV_ARGS(&m_VertexBuffer)));
    }

    // Map and fill vertex buffer.
    {
        CD3DX12_RANGE readEmptyRange{0, 0};
        void* vbMappedPtr = nullptr;
        CHECK_HR(m_VertexBuffer->Map(
            0, // Subresource
            &readEmptyRange, // pReadRange
            &vbMappedPtr));
        memcpy(vbMappedPtr, vertices, vertexCount * sizeof(Vertex));
        m_VertexBuffer->Unmap(0, // Subresource
            nullptr); // pWrittenRange
    }

    if(indexCount > 0)
    {
        assert(indices);
        // Create index buffer.
        {
            const UINT64 ibSize = indexCount * sizeof(IndexType);
            D3D12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
            CHECK_HR(g_Renderer->GetDevice()->CreateCommittedResource(
                &uploadHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &ibDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, // pOptimizedClearValue
                IID_PPV_ARGS(&m_IndexBuffer)));
        }

        // Map and fill vertex buffer.
        {
            CD3DX12_RANGE readEmptyRange{0, 0};
            void* ibMappedPtr = nullptr;
            CHECK_HR(m_IndexBuffer->Map(
                0, // Subresource
                &readEmptyRange, // pReadRange
                &ibMappedPtr));
            memcpy(ibMappedPtr, indices, indexCount * sizeof(IndexType));
            m_IndexBuffer->Unmap(0, // Subresource
                nullptr); // pWrittenRange
        }
    }
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView() const
{
    assert(m_VertexBuffer);
    return D3D12_VERTEX_BUFFER_VIEW{
        m_VertexBuffer->GetGPUVirtualAddress(),
        m_VertexCount * sizeof(Vertex), // SizeInBytes
        sizeof(Vertex) }; // StrideInBytes
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView() const
{
    assert(m_IndexBuffer);
    return D3D12_INDEX_BUFFER_VIEW{
        m_IndexBuffer->GetGPUVirtualAddress(),
        m_IndexCount * sizeof(IndexType), // SizeInBytes
        s_IndexFormat };
}
