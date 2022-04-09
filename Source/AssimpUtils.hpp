#pragma once

struct aiMaterialProperty;
struct aiMaterial;
struct aiScene;

const aiMaterialProperty* FindMaterialProperty(const aiMaterial* material, const str_view& key);

bool GetFloatMaterialProperty(float& out, const aiMaterial* material, const str_view& key);

bool GetStringMaterialProperty(string& out, const aiMaterial* material, const str_view& key);

// Property must be a buffer of the same size as outBuf.
bool GetFixedBufferMaterialProperty(std::span<char> outBuf, const aiMaterial* material, const str_view& key);

void PrintAssimpSceneInfo(const aiScene* scene);
