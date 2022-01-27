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
    out += Format(L"  Mesh %u: Name=\"%hs\", MaterialIndex=%u, PrimitiveTypes=0x%X\n",
        meshIndex,
        mesh->mName.C_Str(),
        mesh->mMaterialIndex,
        (uint32_t)mesh->mPrimitiveTypes);
    out += Format(L"    NumFaces=%u, NumVertices=%u, NumColorChannels=%u, NumUVChannels=%u, NumBones=%u\n",
        mesh->mNumVertices,
        mesh->mNumFaces,
        mesh->GetNumColorChannels(),
        mesh->GetNumUVChannels(),
        mesh->mNumBones);
    out += Format(L"    HasPositions=%u, HasNormals=%u, HasTangentsAndBitangents=%u\n",
        mesh->HasPositions() ? 1 : 0,
        mesh->HasNormals() ? 1 : 0,
        mesh->HasTangentsAndBitangents() ? 1 : 0);
    out += Format(L"    AABB=%g %g %g; %g %g %g\n",
        mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z,
        mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z);
    for(uint32_t i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++i)
    {
        if(mesh->HasTextureCoords(i))
        {
            out += Format(L"    TextureCoords %u: NumUVComponents=%u\n",
                i, mesh->mNumUVComponents[i]);
        }
    }
    for(uint32_t i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++i)
    {
        if(mesh->HasVertexColors(i))
        {
            out += Format(L"    VertexColors %u\n", i);
        }
    }
}

static void AppendMaterialPropertyDataItem(wstring& out, const float *v)
{
    out += Format(L"%g", (double)*v);
}
static void AppendMaterialPropertyDataItem(wstring& out, const double *v)
{
    out += Format(L"%g", *v);
}
static void AppendMaterialPropertyDataItem(wstring& out, const int32_t *v)
{
    out += Format(L"%d", *v);
}

// Appends string inline, no indentation or new line.
template<typename T>
static void AppendMaterialPropertyData(wstring& out, std::span<const char> data)
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

            out += Format(L"    Texture %s %u: Path=\"%hs\", Mapping=%s, UVIndex=%u, Blend=%g, Op=%s, MapMode=%s,%s,%s\n",
                TEXTURE_TYPE_NAMES[textureTypeIndex],
                textureIndex,
                path.C_Str(),
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
        return Format(L"%hs, Semantic=%s, Index=%u", #fullName, TEXTURE_TYPE_NAMES[matProp->mSemantic], matProp->mIndex);

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
            keyName = Format(L"\"%hs\", Semantic=%u, Index=%u", prop->mKey.C_Str(), prop->mSemantic, prop->mIndex);

        out += Format(L"    Property %u: Key=%.*s, DataLength=%u, Type=%s",
            i, STR_TO_FORMAT(keyName), prop->mDataLength, TYPE_NAMES[prop->mType]);
        
        switch(prop->mType)
        {
        case aiPTI_Float:
            out += L", Data: ";
            AppendMaterialPropertyData<float>(out, std::span<const char>(prop->mData, prop->mDataLength));
            break;
        case aiPTI_Double:
            out += L", Data: ";
            AppendMaterialPropertyData<double>(out, std::span<const char>(prop->mData, prop->mDataLength));
            break;
        case aiPTI_String:
        {
            const aiString* s = (const aiString*)prop->mData;
            out += Format(L", Data: \"%hs\"", s->C_Str());
            break;
        }
        case aiPTI_Integer:
            out += L", Data: ";
            AppendMaterialPropertyData<int32_t>(out, std::span<const char>(prop->mData, prop->mDataLength));
            break;
        case aiPTI_Buffer:
            // Will implement when it's needed.
            break;
        default:
            assert(0);
        }

        out += L'\n';
    }
}

static void AppendMaterialInfo(wstring& out, uint32_t materialIndex, const aiMaterial* material)
{
    out += Format(L"  Material %u: NumProperties=%u\n", materialIndex, material->mNumProperties);
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
        return Format(L"%d", *valPtr);
    }
    case AI_UINT64:
    {
        const uint64_t* valPtr = (const uint64_t*)entry.mData;
        return Format(L"%llu", *valPtr);
    }
    case AI_FLOAT:
    {
        const float* valPtr = (const float*)entry.mData;
        return Format(L"%g", (double)*valPtr);
    }
    case AI_DOUBLE:
    {
        const double* valPtr = (const double*)entry.mData;
        return Format(L"%g", *valPtr);
    }
    case AI_AISTRING:
    {
        const aiString* valPtr = (const aiString*)entry.mData;
        return Format(L"%hs", valPtr->C_Str());
    }
    case AI_AIVECTOR3D:
    {
        const aiVector3D* valPtr = (const aiVector3D*)entry.mData;
        return Format(L"%g %g %g", valPtr->x, valPtr->y, valPtr->z);
    }
    default:
        assert(0); return {};
    }
}

static void AppendNodeInfo(wstring& out, uint32_t indentLevel, const aiNode* node)
{
    AppendIndent(out, indentLevel++);
    out += Format(L"Node: Name=\"%hs\", NumMeshes=%u, NumChildren=%u\n",
        node->mName.C_Str(), node->mNumMeshes, node->mNumChildren);
    AppendIndent(out, indentLevel);
    out += Format(L"Transformation: %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g\n",
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
            out += Format(L"%u \"%hs\" (%s) = %.*s\n",
                i, node->mMetaData->mKeys[i].C_Str(),
                TYPE_NAMES[node->mMetaData->mValues[i].mType],
                STR_TO_FORMAT(valueStr));
        }
    }
    for(uint32_t i = 0; i < node->mNumMeshes; ++i)
    {
        AppendIndent(out, indentLevel);
        out += Format(L"Mesh %u: Index=%u\n", i, node->mMeshes[i]);
    }
    for(uint32_t i = 0; i < node->mNumChildren; ++i)
        AppendNodeInfo(out, indentLevel, node->mChildren[i]);
}

void PrintAssimpSceneInfo(const aiScene* scene)
{
    wstring out = Format(L"Assimp scene: NumAnimations=%u, NumCameras=%u, NumLights=%u, NumMaterials=%u, NumMeshes=%u, NumTextures=%u\n",
        scene->mNumAnimations,
        scene->mNumCameras,
        scene->mNumLights,
        scene->mNumMaterials,
        scene->mNumMeshes,
        scene->mNumTextures);
    for(uint32_t i = 0; i < scene->mNumMeshes; ++i)
        AppendMeshInfo(out, i, scene->mMeshes[i]);
    for(uint32_t i = 0; i < scene->mNumMaterials; ++i)
        AppendMaterialInfo(out, i, scene->mMaterials[i]);
    if(scene->mRootNode)
        AppendNodeInfo(out, 1, scene->mRootNode);
    // materials
    // textures

    LogMessage(out);
}
