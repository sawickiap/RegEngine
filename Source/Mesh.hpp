#pragma once

struct Vertex
{
    packed_vec3 m_Position;
    packed_vec3 m_Normal;
    packed_vec3 m_Tangent;
    packed_vec3 m_Bitangent;
    packed_vec2 m_TexCoord;
    packed_vec4 m_Color;

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
        const wstr_view& name,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType,
        D3D12_PRIMITIVE_TOPOLOGY topology,
        std::span<const Vertex> vertices,
        std::span<const IndexType> indices);

    D3D12_PRIMITIVE_TOPOLOGY_TYPE GetTopologyType() const { return m_TopologyType; }
    D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return m_Topology; }
    bool HasIndices() const { return m_IndexCount > 0; }
    uint32_t GetVertexCount() const { return m_VertexCount; }
    uint32_t GetIndexCount() const { return m_IndexCount; }
    ID3D12Resource* GetVertexBuffer() const { return m_VertexBuffer->GetResource(); }
    ID3D12Resource* GetIndexBuffer() const { return m_IndexBuffer->GetResource(); }
    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;

private:
    D3D12_PRIMITIVE_TOPOLOGY_TYPE m_TopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    D3D12_PRIMITIVE_TOPOLOGY m_Topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    uint32_t m_VertexCount = 0;
    uint32_t m_IndexCount = 0;
    ComPtr<D3D12MA::Allocation> m_VertexBuffer;
    ComPtr<D3D12MA::Allocation> m_IndexBuffer;
};
