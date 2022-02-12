#include "BaseUtils.hpp"
#include "AssimpUtils.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/vector2.h>
#include <assimp/vector3.h>

static void AppendIndent(wstring& out, uint32_t indentLevel)
{
    for(uint32_t i = 0; i < indentLevel; ++i)
        out += L"  ";
}

static void AppendMeshInfo(wstring& out, uint32_t meshIndex, const aiMesh* mesh)
{
    out += std::format(L"  Mesh {}: Name=\"{}\", MaterialIndex={}, PrimitiveTypes=0x{:X}\n",
        meshIndex,
        str_view(mesh->mName.C_Str()),
        mesh->mMaterialIndex,
        (uint32_t)mesh->mPrimitiveTypes);
    out += std::format(L"    NumFaces={}, NumVertices={}, NumColorChannels={}, NumUVChannels={}, NumBones={}\n",
        mesh->mNumVertices,
        mesh->mNumFaces,
        mesh->GetNumColorChannels(),
        mesh->GetNumUVChannels(),
        mesh->mNumBones);
    out += std::format(L"    HasPositions={:d}, HasNormals={:d}, HasTangentsAndBitangents={:d}\n",
        mesh->HasPositions(),
        mesh->HasNormals(),
        mesh->HasTangentsAndBitangents());
    /* Seems to always be all zeros. Also doesn't appear in the documentation.
    out += std::format(L"    AABB={},{},{};{},{},{}\n",
        mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z,
        mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z);
    */
    for(uint32_t i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++i)
    {
        if(mesh->HasTextureCoords(i))
        {
            out += std::format(L"    TextureCoords {}: NumUVComponents={}\n",
                i, mesh->mNumUVComponents[i]);
        }
    }
    for(uint32_t i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++i)
    {
        if(mesh->HasVertexColors(i))
        {
            out += std::format(L"    VertexColors {}\n", i);
        }
    }
}

static void AppendMaterialPropertyDataItem(wstring& out, const float *v)
{
    out += std::format(L"{}", *v);
}
static void AppendMaterialPropertyDataItem(wstring& out, const double *v)
{
    out += std::format(L"{}", *v);
}
static void AppendMaterialPropertyDataItem(wstring& out, const int32_t *v)
{
    out += std::format(L"{}", *v);
}

// Appends string inline, no indentation or new line.
template<typename T>
static void AppendMaterialPropertyValue(wstring& out, std::span<const char> data)
{
    if(data.empty())
        return;
    const size_t itemCount = data.size() / sizeof(T);
    const size_t itemCountToPrint = std::min<size_t>(itemCount, 8);
    AppendMaterialPropertyDataItem(out, (const T*)data.data());
    for(size_t i = 1; i < itemCountToPrint; ++i)
    {
        out += L" ";
        AppendMaterialPropertyDataItem(out, (const T*)data.data() + i);
    }
    if(itemCount > itemCountToPrint)
        out += L" ...";
}

static void AppendMaterialPropertyData(wstring& out, std::span<const char> data)
{
    if(data.empty())
        return;
    const size_t byteCountToPrint = std::min<size_t>(data.size(), 8);
    out += std::format(L"{:02X}", (uint8_t)data[0]);
    for(size_t i = 1; i < byteCountToPrint; ++i)
    out += std::format(L" {:02X}", (uint8_t)data[0]);
    if(data.size() > byteCountToPrint)
        out += L" ...";
}

static const wchar_t* TEXTURE_TYPE_NAMES[] = {
    L"NONE", L"DIFFUSE", L"SPECULAR", L"AMBIENT", L"EMISSIVE", L"HEIGHT", L"NORMALS",
    L"SHININESS", L"OPACITY", L"DISPLACEMENT", L"LIGHTMAP", L"REFLECTION",
    L"BASE_COLOR", L"NORMAL_CAMERA", L"EMISSION_COLOR", L"METALNESS", L"DIFFUSE_ROUGHNESS", L"AMBIENT_OCCLUSION",
    L"UNKNOWN",
};
static_assert(_countof(TEXTURE_TYPE_NAMES) == aiTextureType_UNKNOWN + 1);

static const wchar_t* TEXTURE_MAPPING_NAMES[] = {
    L"UV", L"SPHERE", L"CYLINDER", L"BOX", L"PLANE", L"OTHER", L"(Unknown)"
};
static_assert(_countof(TEXTURE_MAPPING_NAMES) == aiTextureMapping_OTHER + 2);

static const wchar_t* TEXTURE_OP_NAMES[] = {
    L"Multiply", L"Add", L"Subtract", L"Divide", L"SmoothAdd", L"SignedAdd", L"(Unknown)"
};
static_assert(_countof(TEXTURE_OP_NAMES) == aiTextureOp_SignedAdd + 2);

static const wchar_t* TEXTURE_MAP_MODE_NAMES[] = {
    L"Wrap", L"Clamp", L"Mirror", L"Decal", L"(Unknown)"
};
static_assert(_countof(TEXTURE_MAP_MODE_NAMES) == aiTextureMapMode_Decal + 2);

static void AppendMaterialTextures(wstring& out, const aiMaterial* material)
{
    aiString path;
    aiTextureMapping mapping;
    uint32_t uvindex;
    float blend;
    aiTextureOp op;
    aiTextureMapMode mapmodes[3];
    for(size_t textureTypeIndex = 0; textureTypeIndex < aiTextureType_UNKNOWN; ++textureTypeIndex)
    {
        const aiTextureType textureType = (aiTextureType)textureTypeIndex;
        const uint32_t textureCount = material->GetTextureCount(textureType);
        for(uint32_t textureIndex = 0; textureIndex < textureCount; ++textureIndex)
        {
            const aiReturn ret = material->GetTexture(textureType, textureIndex,
                &path, &mapping, &uvindex, &blend, &op, mapmodes);
            assert(ret == aiReturn_SUCCESS);
            
            // Make the values safe, as I met out of range values.
            if(mapping < 0 || mapping > aiTextureMapping_OTHER)
                mapping = (aiTextureMapping)(aiTextureMapping_OTHER + 1);
            if(op < 0 || op > aiTextureOp_SignedAdd)
                op = (aiTextureOp)(aiTextureOp_SignedAdd + 1);
            if(mapmodes[0] < 0 || mapmodes[0] > aiTextureMapMode_Decal)
                mapmodes[0] = (aiTextureMapMode)(aiTextureMapMode_Decal + 1);
            if(mapmodes[1] < 0 || mapmodes[1] > aiTextureMapMode_Decal)
                mapmodes[1] = (aiTextureMapMode)(aiTextureMapMode_Decal + 1);
            if(mapmodes[2] < 0 || mapmodes[2] > aiTextureMapMode_Decal)
                mapmodes[2] = (aiTextureMapMode)(aiTextureMapMode_Decal + 1);

            out += std::format(L"    Texture {} {}: Path=\"{}\", Mapping={}, UVIndex={}, Blend={}, Op={}, MapMode={},{},{}\n",
                TEXTURE_TYPE_NAMES[textureTypeIndex],
                textureIndex,
                str_view(path.C_Str()),
                TEXTURE_MAPPING_NAMES[mapping],
                uvindex,
                blend,
                TEXTURE_OP_NAMES[op],
                TEXTURE_MAP_MODE_NAMES[mapmodes[0]], TEXTURE_MAP_MODE_NAMES[mapmodes[1]], TEXTURE_MAP_MODE_NAMES[mapmodes[2]]);
        }
    }
}

// If not found, returns empty string.
static wstring GetMaterialKeyStandardName(const aiMaterialProperty* matProp)
{
    /*
#define MATKEY(macroName, keyStr, semantic, index) \
    assert(semantic == 0 && index == 0); \
    if(str_view(matProp->mKey.C_Str()) == str_view(keyStr)) \
        return macroName;
    */
    const char* keyStr; uint32_t semantic, index;

#define MATKEY(aiMatKeyMacro) \
    std::tie(keyStr, semantic, index) = std::make_tuple(aiMatKeyMacro); \
    if(str_view(matProp->mKey.C_Str()) == str_view(keyStr)) \
        return L"" #aiMatKeyMacro;
#define TEXTURE_MATKEY(fullName, baseName) \
    if(str_view(matProp->mKey.C_Str()) == str_view(baseName)) \
        return std::format(L"{}, Semantic={}, Index={}", str_view(#fullName), TEXTURE_TYPE_NAMES[matProp->mSemantic], matProp->mIndex);

    MATKEY(AI_MATKEY_NAME)
    MATKEY(AI_MATKEY_TWOSIDED)
    MATKEY(AI_MATKEY_SHADING_MODEL)
    MATKEY(AI_MATKEY_ENABLE_WIREFRAME)
    MATKEY(AI_MATKEY_BLEND_FUNC)
    MATKEY(AI_MATKEY_OPACITY)
    MATKEY(AI_MATKEY_TRANSPARENCYFACTOR)
    MATKEY(AI_MATKEY_BUMPSCALING)
    MATKEY(AI_MATKEY_SHININESS)
    MATKEY(AI_MATKEY_REFLECTIVITY)
    MATKEY(AI_MATKEY_SHININESS_STRENGTH)
    MATKEY(AI_MATKEY_REFRACTI)
    MATKEY(AI_MATKEY_COLOR_DIFFUSE)
    MATKEY(AI_MATKEY_COLOR_AMBIENT)
    MATKEY(AI_MATKEY_COLOR_SPECULAR)
    MATKEY(AI_MATKEY_COLOR_EMISSIVE)
    MATKEY(AI_MATKEY_COLOR_TRANSPARENT)
    MATKEY(AI_MATKEY_COLOR_REFLECTIVE)
    MATKEY(AI_MATKEY_GLOBAL_BACKGROUND_IMAGE)
    MATKEY(AI_MATKEY_GLOBAL_SHADERLANG)
    MATKEY(AI_MATKEY_SHADER_VERTEX)
    MATKEY(AI_MATKEY_SHADER_FRAGMENT)
    MATKEY(AI_MATKEY_SHADER_GEO)
    MATKEY(AI_MATKEY_SHADER_TESSELATION)
    MATKEY(AI_MATKEY_SHADER_PRIMITIVE)
    MATKEY(AI_MATKEY_SHADER_COMPUTE)

    TEXTURE_MATKEY(AI_MATKEY_TEXTURE, _AI_MATKEY_TEXTURE_BASE)
    TEXTURE_MATKEY(AI_MATKEY_UVWSRC, _AI_MATKEY_UVWSRC_BASE)
    TEXTURE_MATKEY(AI_MATKEY_TEXOP, _AI_MATKEY_TEXOP_BASE)
    TEXTURE_MATKEY(AI_MATKEY_MAPPING, _AI_MATKEY_MAPPING_BASE)
    TEXTURE_MATKEY(AI_MATKEY_TEXBLEND, _AI_MATKEY_TEXBLEND_BASE)
    TEXTURE_MATKEY(AI_MATKEY_MAPPINGMODE_U, _AI_MATKEY_MAPPINGMODE_U_BASE)
    TEXTURE_MATKEY(AI_MATKEY_MAPPINGMODE_V, _AI_MATKEY_MAPPINGMODE_V_BASE)
    TEXTURE_MATKEY(AI_MATKEY_TEXMAP_AXIS, _AI_MATKEY_TEXMAP_AXIS_BASE)
    TEXTURE_MATKEY(AI_MATKEY_UVTRANSFORM, _AI_MATKEY_UVTRANSFORM_BASE)
    TEXTURE_MATKEY(AI_MATKEY_TEXFLAGS, _AI_MATKEY_TEXFLAGS_BASE)

#undef TEXTURE_MATKEY
#undef MATKEY
    return {};
}

static void AppendMaterialProperties(wstring& out, const aiMaterial* material)
{
    const wchar_t* TYPE_NAMES[] = {
        L"", L"Float", L"Double", L"String", L"Integer", L"Buffer"};

    for(size_t i = 0; i < material->mNumProperties; ++i)
    {
        const aiMaterialProperty* prop = material->mProperties[i];

        wstring keyName = GetMaterialKeyStandardName(prop);
        if(keyName.empty())
            keyName = std::format(L"\"{}\", Semantic={}, Index={}", str_view(prop->mKey.C_Str()), prop->mSemantic, prop->mIndex);

        out += std::format(L"    Property {}: Key={}, DataLength={}, Type={}",
            i, keyName, prop->mDataLength, TYPE_NAMES[prop->mType]);
        
        switch(prop->mType)
        {
        case aiPTI_Float:
            out += L", Data: ";
            AppendMaterialPropertyValue<float>(out, std::span<const char>(prop->mData, prop->mDataLength));
            break;
        case aiPTI_Double:
            out += L", Data: ";
            AppendMaterialPropertyValue<double>(out, std::span<const char>(prop->mData, prop->mDataLength));
            break;
        case aiPTI_String:
        {
            const aiString* s = (const aiString*)prop->mData;
            out += std::format(L", Data: \"{}\"", str_view(s->C_Str()));
            break;
        }
        case aiPTI_Integer:
            out += L", Data: ";
            AppendMaterialPropertyValue<int32_t>(out, std::span<const char>(prop->mData, prop->mDataLength));
            break;
        case aiPTI_Buffer:
            out += L", Buffer: ";
            AppendMaterialPropertyData(out, std::span<const char>(prop->mData, prop->mDataLength));
            break;
        default:
            assert(0);
        }

        out += L'\n';
    }
}

static void AppendMaterialInfo(wstring& out, uint32_t materialIndex, const aiMaterial* material)
{
    out += std::format(L"  Material {}: NumProperties={}\n", materialIndex, material->mNumProperties);
    AppendMaterialTextures(out, material);
    AppendMaterialProperties(out, material);
}

static wstring FormatMetaDataProperty(const aiMetadataEntry& entry)
{
    switch(entry.mType)
    {
    case AI_BOOL:
    {
        const bool* valPtr = (const bool*)entry.mData;
        return *valPtr ? L"1" : L"0";
    }
    case AI_INT32:
    {
        const int32_t* valPtr = (const int32_t*)entry.mData;
        return std::format(L"{}", *valPtr);
    }
    case AI_UINT64:
    {
        const uint64_t* valPtr = (const uint64_t*)entry.mData;
        return std::format(L"{}", *valPtr);
    }
    case AI_FLOAT:
    {
        const float* valPtr = (const float*)entry.mData;
        return std::format(L"{}", (double)*valPtr);
    }
    case AI_DOUBLE:
    {
        const double* valPtr = (const double*)entry.mData;
        return std::format(L"{}", *valPtr);
    }
    case AI_AISTRING:
    {
        const aiString* valPtr = (const aiString*)entry.mData;
        return ConvertCharsToUnicode(valPtr->C_Str(), CP_ACP);
    }
    case AI_AIVECTOR3D:
    {
        const aiVector3D* valPtr = (const aiVector3D*)entry.mData;
        return std::format(L"{},{},{}", valPtr->x, valPtr->y, valPtr->z);
    }
    default:
        assert(0); return {};
    }
}

static void AppendNodeInfo(wstring& out, uint32_t indentLevel, const aiNode* node)
{
    AppendIndent(out, indentLevel++);
    out += std::format(L"Node: Name=\"{}\", NumMeshes={}, NumChildren={}\n",
        str_view(node->mName.C_Str()), node->mNumMeshes, node->mNumChildren);
    AppendIndent(out, indentLevel);
    out += std::format(L"Transformation: {},{},{},{};{},{},{},{};{},{},{},{};{},{},{},{}\n",
        node->mTransformation.a1, node->mTransformation.a2, node->mTransformation.a3, node->mTransformation.a4,
        node->mTransformation.b1, node->mTransformation.b2, node->mTransformation.b3, node->mTransformation.b4,
        node->mTransformation.c1, node->mTransformation.c2, node->mTransformation.c3, node->mTransformation.c4,
        node->mTransformation.d1, node->mTransformation.d2, node->mTransformation.d3, node->mTransformation.d4);
    if(node->mMetaData)
    {
        AppendIndent(out, indentLevel);
        out += L"MetaData:\n";
        const wchar_t* TYPE_NAMES[] = {
            L"BOOL", L"INT32", L"UINT64", L"FLOAT", L"DOUBLE", L"AISTRING", L"AIVECTOR3D"};
        for(uint32_t i = 0; i < node->mMetaData->mNumProperties; ++i)
        {
            wstring valueStr = FormatMetaDataProperty(node->mMetaData->mValues[i]);
            AppendIndent(out, indentLevel + 1);
            out += std::format(L"{} \"{}\" ({}) = {}\n",
                i, str_view(node->mMetaData->mKeys[i].C_Str()),
                TYPE_NAMES[node->mMetaData->mValues[i].mType],
                valueStr);
        }
    }
    for(uint32_t i = 0; i < node->mNumMeshes; ++i)
    {
        AppendIndent(out, indentLevel);
        out += std::format(L"Mesh {}: Index={}\n", i, node->mMeshes[i]);
    }
    for(uint32_t i = 0; i < node->mNumChildren; ++i)
        AppendNodeInfo(out, indentLevel, node->mChildren[i]);
}

void PrintAssimpSceneInfo(const aiScene* scene)
{
    wstring out = std::format(L"Assimp scene: NumAnimations={}, NumCameras={}, NumLights={}, NumMaterials={}, NumMeshes={}, NumTextures={}\n",
        scene->mNumAnimations,
        scene->mNumCameras,
        scene->mNumLights,
        scene->mNumMaterials,
        scene->mNumMeshes,
        scene->mNumTextures);
    if(scene->mRootNode)
        AppendNodeInfo(out, 1, scene->mRootNode);
    for(uint32_t i = 0; i < scene->mNumMeshes; ++i)
        AppendMeshInfo(out, i, scene->mMeshes[i]);
    for(uint32_t i = 0; i < scene->mNumMaterials; ++i)
        AppendMaterialInfo(out, i, scene->mMaterials[i]);

    LogMessage(out);
}
