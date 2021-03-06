#ifndef SHARED_TYPES_HLSLI
#define SHARED_TYPES_HLSLI

// ==================== Material ====================
struct Material
{
	uint   BSDFType;
	float3 baseColor;
	float  metallic;
	float  subsurface;
	float  specular;
	float  roughness;
	float  specularTint;
	float  anisotropic;
	float  sheen;
	float  sheenTint;
	float  clearcoat;
	float  clearcoatGloss;

	// Used by Glass BxDF
	float3 T;
	float  etaA, etaB;

	int Albedo;
};

// ==================== Light ====================
#define LightType_Point (0)
#define LightType_Quad	(1)

struct Light
{
	uint   Type;
	float3 Position;
	float4 Orientation;
	float  Width;
	float  Height;
	float3 Points[4]; // World-space points that are pre-computed on the Cpu so we don't have to compute them in shader
					  // for every ray

	float3 I;
};

// ==================== Mesh ====================
struct Mesh
{
	uint	 VertexOffset;
	uint	 IndexOffset;
	uint	 MaterialIndex;
	uint	 InstanceIDAndMask;
	uint	 InstanceContributionToHitGroupIndexAndFlags;
	uint64_t AccelerationStructure;
	matrix	 World;
	matrix	 PreviousWorld;
	float3x4 Transform;
};

// ==================== Camera ====================
struct Camera
{
	float FoVY; // Degrees
	float AspectRatio;
	float NearZ;
	float FarZ;

	float FocalLength;
	float RelativeAperture;
	float __Padding0;
	float __Padding1;

	float4 Position;

	matrix World;
	matrix View;
	matrix Projection;
	matrix ViewProjection;
	matrix InvView;
	matrix InvProjection;
	matrix InvViewProjection;

	RayDesc GenerateCameraRay(float2 ndc, inout uint seed)
	{
		// Setup the ray
		RayDesc ray;
		ray.Origin = World[3].xyz;
		ray.TMin   = 0.0f;
		ray.TMax   = FLT_MAX;

		// Extract the aspect ratio and field of view from the projection matrix
		float tanHalfFoVY = tan(radians(FoVY) * 0.5f);

		// Compute the ray direction for this pixel
		ray.Direction = normalize(
			(ndc.x * World[0].xyz * tanHalfFoVY * AspectRatio) + (ndc.y * World[1].xyz * tanHalfFoVY) + World[2].xyz);

		return ray;
	}
};

#endif // SHARED_TYPES_HLSLI
