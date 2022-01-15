#pragma once

struct Vertex
{
    glm::vec<3, float, glm::packed_highp> m_Position;
    glm::vec<2, float, glm::packed_highp> m_TexCoord;
    glm::vec<4, float, glm::packed_highp> m_Color;

    static const D3D12_INPUT_ELEMENT_DESC* GetInputElements();
    static const uint32_t GetInputElementCount();
};

/*
Represents a triangle mesh - vertex and (optional) index buffer.
*/
class Mesh
{
public:
    typedef uint16_t IndexType;
    static constexpr DXGI_FORMAT s_IndexFormat = DXGI_FORMAT_R16_UINT;
    void Init(
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType,
        D3D12_PRIMITIVE_TOPOLOGY topology,
        const Vertex* vertices,
        uint32_t vertexCount,
        const IndexType* indices,
        uint32_t indexCount);

    D3D12_PRIMITIVE_TOPOLOGY_TYPE GetTopologyType() const { return m_TopologyType; }
    D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return m_Topology; }
    bool HasIndices() const { return m_IndexCount > 0; }
    uint32_t GetVertexCount() const { return m_VertexCount; }
    uint32_t GetIndexCount() const { return m_IndexCount; }
    ID3D12Resource* GetVertexBuffer() const { return m_VertexBuffer.Get(); }
    ID3D12Resource* GetIndexBuffer() const { return m_IndexBuffer.Get(); }
    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;

private:
    D3D12_PRIMITIVE_TOPOLOGY_TYPE m_TopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    D3D12_PRIMITIVE_TOPOLOGY m_Topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    uint32_t m_VertexCount = 0;
    uint32_t m_IndexCount = 0;
    ComPtr<ID3D12Resource> m_VertexBuffer;
    ComPtr<ID3D12Resource> m_IndexBuffer;
};
