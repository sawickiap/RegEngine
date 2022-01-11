#pragma once

#define GLM_FORCE_CXX17
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_LEFT_HANDED // The coordinate system we decide to use.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // The convention Direct3D uses.
#define GLM_FORCE_SILENT_WARNINGS

/*
#include "../ThirdParty/glm/glm/glm.hpp"
#include "../ThirdParty/glm/glm/ext.hpp"
*/
#include "../ThirdParty/glm/glm/vec2.hpp"               // vec2, bvec2, dvec2, ivec2 and uvec2
#include "../ThirdParty/glm/glm/vec3.hpp"               // vec3, bvec3, dvec3, ivec3 and uvec3
#include "../ThirdParty/glm/glm/vec4.hpp"               // vec4, bvec4, dvec4, ivec4 and uvec4
#include "../ThirdParty/glm/glm/mat2x2.hpp"             // mat2, dmat2
#include "../ThirdParty/glm/glm/mat2x3.hpp"             // mat2x3, dmat2x3
#include "../ThirdParty/glm/glm/mat2x4.hpp"             // mat2x4, dmat2x4
#include "../ThirdParty/glm/glm/mat3x2.hpp"             // mat3x2, dmat3x2
#include "../ThirdParty/glm/glm/mat3x3.hpp"             // mat3, dmat3
#include "../ThirdParty/glm/glm/mat3x4.hpp"             // mat3x4, dmat2
#include "../ThirdParty/glm/glm/mat4x2.hpp"             // mat4x2, dmat4x2
#include "../ThirdParty/glm/glm/mat4x3.hpp"             // mat4x3, dmat4x3
#include "../ThirdParty/glm/glm/mat4x4.hpp"             // mat4, dmat4
#include "../ThirdParty/glm/glm/common.hpp"             // all the GLSL common functions: abs, min, mix, isnan, fma, etc.
#include "../ThirdParty/glm/glm/exponential.hpp"        // all the GLSL exponential functions: pow, log, exp2, sqrt, etc.
#include "../ThirdParty/glm/glm/geometric.hpp"          // all the GLSL geometry functions: dot, cross, reflect, etc.
#include "../ThirdParty/glm/glm/integer.hpp"            // all the GLSL integer functions: findMSB, bitfieldExtract, etc.
#include "../ThirdParty/glm/glm/matrix.hpp"             // all the GLSL matrix functions: transpose, inverse, etc.
#include "../ThirdParty/glm/glm/packing.hpp"            // all the GLSL packing functions: packUnorm4x8, unpackHalf2x16, etc.
#include "../ThirdParty/glm/glm/trigonometric.hpp"      // all the GLSL trigonometric functions: radians, cos, asin, etc.
#include "../ThirdParty/glm/glm/vector_relational.hpp"  // all the GLSL vector relational functions: equal, less, etc.

#include "../ThirdParty/glm/glm/gtc/type_ptr.hpp"

using glm::vec2;
using glm::vec3;
using glm::vec4;
