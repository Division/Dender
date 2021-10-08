#define PI 3.1415926535

float LogBase(float x, float base)
{
    return log(x) / log(base);
}

SamplerState SamplerLinearWrap;
SamplerState SamplerLinearClamp;

#if defined(SKINNING)
StructuredBuffer<float4x4> SkinningMatrices : register(t3, space1);

cbuffer SkinningOffset : register(b4, space1)
{
    uint skinning_offset;
};

#endif


struct VS_in
{
    float4 position : POSITION;
#if defined(TEXTURE0) || defined(NORMAL_MAP)
    float2 texcoord0 : TEXCOORD;
#endif

#if defined(LIGHTING)
    float4 normal : NORMAL;
#if defined(NORMAL_MAP)
    float4 tangent : TANGENT;
#endif
#endif

#if defined(SKINNING)
    float4 joint_weights : BLENDWEIGHT;
    uint4 joint_indices : BLENDINDICES;
#endif
};

struct VS_out
{
    float4 position : SV_POSITION;

#if defined(TEXTURE0) || defined(NORMAL_MAP)
    float2 texcoord0 : TEXCOORD;
#endif

#if defined(LIGHTING)
    float4 normal_worldspace : NORMAL;
    float4 tangent_worldspace : TANGENT;
    float linear_depth : POSITION1;
#endif

#if !defined(DEPTH_ONLY)
    float4 position_worldspace : POSITION;
#endif

    float4 frag_coord : SV_Position;
};

struct VertexData
{
    float3 position_worldspace;
    float2 texcoord0;
    float4 normal_worldspace;
    float4 tangent_worldspace;
    float4 frag_coord;
    float linear_depth;
};

float SrgbToLinear(float value)
{
    float gamma = 2.2;
    return pow(value, gamma);
}

float LinearizeDepth(float depth_ndc, float near, float far)
{
    return (2.0 * near * far) / (far + near - (depth_ndc * 2.0f - 1.0f) * (far - near));
}

VertexData GetDefaultVertexData(VS_in input, float4x4 object_model_matrix)
{
    VertexData result;
    float4x4 model_matrix = object_model_matrix;
#if defined(SKINNING)
    model_matrix = float4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        uint joint_index = input.joint_indices[i];
        float joint_weight = input.joint_weights[i];
        model_matrix += SkinningMatrices[skinning_offset + joint_index] * joint_weight;
    }

    model_matrix = mul(object_model_matrix, model_matrix);
#endif

    result.position_worldspace = mul(model_matrix, float4(input.position.xyz, 1.0)).xyz;

#if defined(LIGHTING)
    result.normal_worldspace = normalize(mul(model_matrix, float4(input.normal.xyz, 0)));
    result.tangent_worldspace = 0.0f;
#if defined(NORMAL_MAP)
    result.tangent_worldspace = normalize(mul(model_matrix, float4(input.tangent.xyz, 0)));
    result.tangent_worldspace.w = input.tangent.w;
#endif
    result.linear_depth = 0;
#endif

#if defined(TEXTURE0) || defined(NORMAL_MAP)
    result.texcoord0 = input.texcoord0;
#endif

    result.frag_coord = 0;

    return result;
}

VertexData GePSVertexData(VS_out input)
{
    VertexData result;

#if !defined(DEPTH_ONLY)
    result.position_worldspace = input.position_worldspace.xyz;
#endif

#if defined(LIGHTING)
    result.normal_worldspace = normalize(float4(input.normal_worldspace.xyz, 0));
    result.tangent_worldspace = 0.0f;
#if defined(NORMAL_MAP)
    result.tangent_worldspace = normalize(float4(input.tangent_worldspace.xyz, 0));
    result.tangent_worldspace.w = input.tangent_worldspace.w;
#endif
    result.linear_depth = input.linear_depth;
#endif

#if defined(TEXTURE0) || defined(NORMAL_MAP)
    result.texcoord0 = input.texcoord0;
#endif

    result.frag_coord = input.frag_coord;

    return result;
}

VS_out GetVSOut(VertexData data, float4x4 view_projection)
{
    VS_out result;

#if defined(TEXTURE0) || defined(NORMAL_MAP)
    result.texcoord0 = data.texcoord0;
#endif

    float4 position_worldspace = float4(data.position_worldspace, 1);

#if !defined(DEPTH_ONLY)
    result.position_worldspace = position_worldspace;
#endif

    result.position = mul(view_projection, position_worldspace);

#if defined(LIGHTING)
    result.normal_worldspace = data.normal_worldspace;
    result.tangent_worldspace = data.tangent_worldspace;
    result.linear_depth = LinearizeDepth(result.position.z / result.position.w, camera.zMin, camera.zMax);
#endif

    return result;
}

float4 GetTBNNormal(VertexData vertex_data, float4 normal_sampled)
{
    float3 normal_tangentspace = normalize(normal_sampled.xyz * 2.0f - 1.0f);

    float3 normal_worldspace_final = normalize(vertex_data.normal_worldspace.xyz);
    float3 tangent_worldspace_final = normalize(vertex_data.tangent_worldspace.xyz);
    float3 bitangent_worldspace_final = normalize(cross(normal_worldspace_final, tangent_worldspace_final) * vertex_data.tangent_worldspace.w);

    float3x3 TBN = transpose(float3x3(tangent_worldspace_final, bitangent_worldspace_final, normal_worldspace_final));

    return float4(normalize(mul(TBN, normal_tangentspace)), 0);
}