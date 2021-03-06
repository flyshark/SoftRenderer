#include "StdAfx.h"
#include "BasicShaders.h"
#include "../SrSwRendererCpu/SrFragmentBuffer.h"

#include "mmgr/mmgr.h"


GlobalEnvironment* gEnv = NULL;

extern "C" __declspec(dllexport) void ModuleInit( GlobalEnvironment* pgEnv )
{
	gEnv = pgEnv;
}

extern "C" __declspec(dllexport) SrSwShader* LoadShader( const char* shaderName )
{
	std::vector<SrSwShader*> shaders;
	shaders.push_back( &g_FlatShadingShader );
	shaders.push_back( &g_PhongShadingShader );
	shaders.push_back( &g_GourandShadingShader );
	shaders.push_back( &g_PhongShadingWithNormalShader );

	for (uint32 i=0; i < shaders.size(); ++i)
	{
		if ( !stricmp(shaders[i]->getName(), shaderName))
		{
			return shaders[i];
		}
	}

	return NULL;
}

SrFlatShader g_FlatShadingShader;
SrPhongShader g_PhongShadingShader;
SrGourandShader g_GourandShadingShader;
SrPhongWithNormalShader g_PhongShadingWithNormalShader;

// local data format
struct SrPhongShading_Vert2Frag
{
	float4 pos;				// channel0

	float4 worldpos_tx;		// channel1:xyz
	float4 normal_ty;		// channel1:w channel2:xy
	float4 tangent;			// channel2:zw channel3:x
	
};

struct SrGourandShading_Vert2Frag
{
	float4 pos;					// channel0
	

	float4 diffuseAcc_tx;			// channel1:w channel2:xy
	float4 specularAcc_ty;			// channel1:w channel2:xy

	float4 texcoord;			// channel4:xy
};

struct SrFlatShading_Vert2Frag
{
	float4 pos;					// channel0
	

	float4 diffuseAcc_tx;			// channel1:w channel2:xy
	float4 specularAcc_ty;			// channel1:w channel2:xy

	float4 texcoord;			// channel4:xy
};

void SrFlatShader::ProcessPatch( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrFlatShading_Vert2Frag* inTHREE[3] = {(SrFlatShading_Vert2Frag*)vInRef0, (SrFlatShading_Vert2Frag*)vInRef1, (SrFlatShading_Vert2Frag*)vInRef2};
	SrFlatShading_Vert2Frag* outTHREE[3] = {(SrFlatShading_Vert2Frag*)vOut, (SrFlatShading_Vert2Frag*)vOut1, (SrFlatShading_Vert2Frag*)vOut2};

	float4 diffAvg = inTHREE[0]->diffuseAcc_tx + inTHREE[1]->diffuseAcc_tx + inTHREE[2]->diffuseAcc_tx;
	float4 specAvg = inTHREE[0]->specularAcc_ty + inTHREE[1]->specularAcc_ty + inTHREE[2]->specularAcc_ty;
	
	diffAvg /= 3.f;
	specAvg /= 3.f;

	outTHREE[0]->diffuseAcc_tx = diffAvg;
	outTHREE[1]->diffuseAcc_tx = diffAvg;
	outTHREE[2]->diffuseAcc_tx = diffAvg;

	outTHREE[0]->specularAcc_ty = specAvg;
	outTHREE[1]->specularAcc_ty = specAvg;
	outTHREE[2]->specularAcc_ty = specAvg;

}

void SrFlatShader::ProcessVertex( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrVertexP3N3T2* in = (SrVertexP3N3T2*)vInRef0;
	SrFlatShading_Vert2Frag* out = (SrFlatShading_Vert2Frag*)vOut;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	// pos处理到世界空间，保存
	float3 worldpos = (context->matrixs[eMd_World] * in->pos).xyz;

	// pos处理到投影空间
	out->pos = context->matrixs[eMd_WorldViewProj] * in->pos;

	// 直接进行光照	// 转换normal到世界空间
	float3 normal = (context->matrixs[eMd_World].RotateVector3(in->normal));
	normal.normalize();

	// 视线方向计算
	float3 viewWS = context->matrixs[eMd_ViewInverse].GetTranslate() - worldpos;
	viewWS.normalize();
	
	float4 diffuseAcc = gEnv->sceneMgr->GetSkyLightColor() * (normal.y * 0.4f + 0.6f);
	float4 specularAcc(0.f);

	CalcLights(context, worldpos, normal, viewWS, diffuseAcc, specularAcc);

	// 存入worldpos，供ps阶段使用
	out->diffuseAcc_tx = diffuseAcc * cBuffer->difColor;
	out->specularAcc_ty = specularAcc;

	out->texcoord.xy = in->texcoord;
}

void SrFlatShader::ProcessRasterize( void* rOut, const void* rInRef0, const void* rInRef1, const void* rInRef2, float ratio, const SrShaderContext* context, bool final ) const
{
	const SrFlatShading_Vert2Frag* verA = static_cast<const SrFlatShading_Vert2Frag*>(rInRef0);
	const SrFlatShading_Vert2Frag* verB = static_cast<const SrFlatShading_Vert2Frag*>(rInRef1);
	SrFlatShading_Vert2Frag* verO = static_cast<SrFlatShading_Vert2Frag*>(rOut);

	float inv_ratio = 1.f - ratio;

	// 线性插值project space pos
	verO->pos = SrFastLerp( verA->pos, verB->pos, ratio, inv_ratio );

	// 已经除w
	// 直接插值，其他channel
	verO->texcoord = SrFastLerp( verA->texcoord, verB->texcoord, ratio, inv_ratio );
	verO->diffuseAcc_tx = verA->diffuseAcc_tx;
	verO->specularAcc_ty = verA->specularAcc_ty;
	// 对于scanline扫描的，将透视插值坐标，插值回正常值
	if (final)
	{
		verO->texcoord *= (1.f / verO->pos.w);
		verO->diffuseAcc_tx *= (1.f / verO->pos.w);
		verO->specularAcc_ty *= (1.f / verO->pos.w);
	}
}

void SrFlatShader::ProcessPixel( uint32* pOut, const void* pIn, const SrShaderContext* context, uint32 address ) const
{
	SrFlatShading_Vert2Frag* in = (SrFlatShading_Vert2Frag*)pIn;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	float2 texcoord = in->texcoord.xy;

	// 采样diffuse颜色
	uint32 col = context->Tex2D( texcoord, 0 );
	float4 colf = uint32_2_float4(col);

	// srgb
	colf = colf * colf;

	colf = colf * in->diffuseAcc_tx;
	colf += cBuffer->spcColor * in->specularAcc_ty;

	// 截断到0-1
	colf = Clamp(colf, 0.f, 1.f);	

	// srgb还原
	colf.sqrt();

	*pOut = float4_2_uint32(colf );
}


void SrGourandShader::ProcessVertex( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrVertexP3N3T2* in = (SrVertexP3N3T2*)vInRef0;
	SrGourandShading_Vert2Frag* out = (SrGourandShading_Vert2Frag*)vOut;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	// pos处理到世界空间，保存
	float3 worldpos = (context->matrixs[eMd_World] * in->pos).xyz;

	// pos处理到投影空间
	out->pos = context->matrixs[eMd_WorldViewProj] * in->pos;

	// 直接进行光照
	// 转换normal到世界空间
	float3 normal = (context->matrixs[eMd_World].RotateVector3(in->normal));
	normal.normalize();

	// 视线方向计算
	float3 viewWS = context->matrixs[eMd_ViewInverse].GetTranslate() - worldpos;
	viewWS.normalize();


	float4 diffuseAcc = gEnv->sceneMgr->GetSkyLightColor() * (normal.y * 0.4f + 0.6f);
	float4 specularAcc(0.f);

	CalcLights(context, worldpos, normal, viewWS, diffuseAcc, specularAcc);

	// 存入worldpos，供ps阶段使用
	out->diffuseAcc_tx = diffuseAcc * cBuffer->difColor;
	out->specularAcc_ty = specularAcc;

	out->texcoord.xy = in->texcoord;
}

void SrGourandShader::ProcessRasterize( void* rOut, const void* rInRef0, const void* rInRef1, const void* rInRef2, float ratio, const SrShaderContext* context, bool final ) const
{
	const SrGourandShading_Vert2Frag* verA = static_cast<const SrGourandShading_Vert2Frag*>(rInRef0);
	const SrGourandShading_Vert2Frag* verB = static_cast<const SrGourandShading_Vert2Frag*>(rInRef1);
	SrGourandShading_Vert2Frag* verO = static_cast<SrGourandShading_Vert2Frag*>(rOut);

	// 线性插值project space pos
	float inv_ratio = 1.f - ratio;
	verO->pos = SrFastLerp( verA->pos, verB->pos, ratio, inv_ratio );

	// 已经除w
	// 直接插值，其他channel
	verO->texcoord = SrFastLerp( verA->texcoord, verB->texcoord, ratio, inv_ratio );
	verO->diffuseAcc_tx = SrFastLerp( verA->diffuseAcc_tx, verB->diffuseAcc_tx, ratio, inv_ratio );
	verO->specularAcc_ty = SrFastLerp( verA->specularAcc_ty, verB->specularAcc_ty, ratio, inv_ratio );
	// 对于scanline扫描的，将透视插值坐标，插值回正常值
	if (final)
	{
		verO->texcoord *= (1.f / verO->pos.w);
		verO->diffuseAcc_tx *= (1.f / verO->pos.w);
		verO->specularAcc_ty *= (1.f / verO->pos.w);
	}
}

void SrGourandShader::ProcessPixel( uint32* pOut, const void* pIn, const SrShaderContext* context, uint32 address ) const
{
	SrGourandShading_Vert2Frag* in = (SrGourandShading_Vert2Frag*)pIn;
	uint32* out = (uint32*)pOut;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	// 采样diffuse颜色
	uint32 col = context->Tex2D( in->texcoord.xy, 0 );
	float4 colf = uint32_2_float4(col);

	// diffuse 作 srgb
	colf = colf * colf;

	colf = colf * in->diffuseAcc_tx;
	colf += cBuffer->spcColor * in->specularAcc_ty;

	// 截断到0-1
	colf = Clamp(colf, 0.f, 1.f);	

	// srgb还原
	colf.sqrt();

	*out = float4_2_uint32(colf );
}


void SrPhongShader::ProcessVertex( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrVertexP3N3T2* in = (SrVertexP3N3T2*)vInRef0;
	SrPhongShading_Vert2Frag* out = (SrPhongShading_Vert2Frag*)vOut;

	// pos处理到投影空间
	out->pos = context->matrixs[eMd_WorldViewProj] * in->pos;
	// pos处理到世界空间，保存
	out->worldpos_tx = (context->matrixs[eMd_World] * in->pos);
	out->worldpos_tx.w = in->texcoord.x;
	// normal,tangent处理到世界空间
	out->normal_ty = float4((context->matrixs[eMd_World].RotateVector3(in->normal)), in->texcoord.y);
}

void SrPhongShader::ProcessRasterize( void* rOut, const void* rInRef0, const void* rInRef1, const void* rInRef2, float ratio, const SrShaderContext* context, bool final ) const
{
	const SrPhongShading_Vert2Frag* verA = static_cast<const SrPhongShading_Vert2Frag*>(rInRef0);
	const SrPhongShading_Vert2Frag* verB = static_cast<const SrPhongShading_Vert2Frag*>(rInRef1);
	SrPhongShading_Vert2Frag* verO = static_cast<SrPhongShading_Vert2Frag*>(rOut);

	// 线性插值project space pos
	float inv_ratio = 1.f - ratio;

#ifdef SR_USE_AVX
	FastRasterize( (SrRendVertex*)rOut, (SrRendVertex*)rInRef0, (SrRendVertex*)rInRef1, ratio, inv_ratio);

	if (final)
	{
		FastFinalRasterize( (SrRendVertex*)rOut, ((SrRendVertex*)rOut)->pos.w);
	}
#else
	verO->pos = SrFastLerp( verA->pos, verB->pos, ratio, inv_ratio );

	// 已经除w
	// 直接插值，其他channel
	verO->normal_ty = SrFastLerp( verA->normal_ty, verB->normal_ty, ratio, inv_ratio );
	verO->worldpos_tx = SrFastLerp( verA->worldpos_tx, verB->worldpos_tx, ratio, inv_ratio );

	// 对于scanline扫描的，将透视插值坐标，插值回正常值
	if (final)
	{
		verO->normal_ty /= verO->pos.w;
		verO->worldpos_tx /= verO->pos.w;
	}
#endif
}

void SrPhongShader::ProcessPixel( uint32* pOut, const void* pIn, const SrShaderContext* context, uint32 address ) const
{
	SrPhongShading_Vert2Frag* in = (SrPhongShading_Vert2Frag*)pIn;
	uint32* out = (uint32*)pOut;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	// 采样diffuse颜色
	float2 tc0( in->worldpos_tx.w, in->normal_ty.w );
	uint32 col = context->Tex2D( tc0, 0 );
	float4 matDiff = uint32_2_float4(col);
	float matSpec = matDiff.a;
	// diffuse 作 srgb
	matDiff = matDiff * matDiff;

	// 法线扰动处理
	float3 normalDir = in->normal_ty.xyz;
	normalDir.normalize();

// 	*out = float4_2_uint32(float4((normalDir * 0.5f + float3(0.5f)),1.f));
// 	return;


	// 视线方向计算
	float3 viewWS = context->matrixs[eMd_ViewInverse].GetTranslate() - in->worldpos_tx.xyz;
	viewWS.normalize();

	float4 diffuseAcc = gEnv->sceneMgr->GetSkyLightColor();
	diffuseAcc *= (normalDir.y * 0.4f + 0.6f);
	float4 specularAcc(0.f);

	CalcLights(context, in->worldpos_tx.xyz, normalDir, viewWS, diffuseAcc, specularAcc);
	
	// 截断到0-1

	//diffuseAcc = (diffuseAcc + 0.5f) * matDiff;
	diffuseAcc = diffuseAcc * matDiff * cBuffer->difColor;
	diffuseAcc += cBuffer->spcColor * specularAcc * matSpec;
	diffuseAcc = Clamp(diffuseAcc, 0.f, 1.f);	

	// srgb还原
	diffuseAcc.sqrt();

	*out = float4_2_uint32(diffuseAcc);
}

void SrPhongWithNormalShader::ProcessPatch( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrPhongShading_Vert2Frag* inTHREE[3] = {(SrPhongShading_Vert2Frag*)vInRef0, (SrPhongShading_Vert2Frag*)vInRef1, (SrPhongShading_Vert2Frag*)vInRef2};
	SrPhongShading_Vert2Frag* outTHREE[3] = {(SrPhongShading_Vert2Frag*)vOut, (SrPhongShading_Vert2Frag*)vOut1, (SrPhongShading_Vert2Frag*)vOut2};

	//tangentProcess
	// calc tangent space in vertex shader!
	float3 pos1 = inTHREE[0]->tangent.xyz;
	float3 pos2 = inTHREE[1]->tangent.xyz;
	float3 pos3 = inTHREE[2]->tangent.xyz;

	float2 uv1(inTHREE[0]->worldpos_tx.w, inTHREE[0]->normal_ty.w);
	float2 uv2(inTHREE[1]->worldpos_tx.w, inTHREE[1]->normal_ty.w);
	float2 uv3(inTHREE[2]->worldpos_tx.w, inTHREE[2]->normal_ty.w);

	outTHREE[0]->tangent = CalculateTangentSpaceVector(pos1, pos2, pos3, uv1, uv2, uv3);
	outTHREE[1]->tangent = CalculateTangentSpaceVector(pos2, pos1, pos3, uv2, uv1, uv3);
	outTHREE[2]->tangent = CalculateTangentSpaceVector(pos3, pos2, pos1, uv3, uv2, uv1);

	outTHREE[0]->tangent.xyz = (context->matrixs[eMd_World].RotateVector3(outTHREE[0]->tangent.xyz ));
	outTHREE[1]->tangent.xyz = (context->matrixs[eMd_World].RotateVector3(outTHREE[1]->tangent.xyz ));
	outTHREE[2]->tangent.xyz = (context->matrixs[eMd_World].RotateVector3(outTHREE[2]->tangent.xyz ));
}

void SrPhongWithNormalShader::ProcessVertex( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrVertexP3N3T2* in = (SrVertexP3N3T2*)vInRef0;
	SrPhongShading_Vert2Frag* out = (SrPhongShading_Vert2Frag*)vOut;


	// pos处理到投影空间
	out->pos = context->matrixs[eMd_WorldViewProj] * in->pos;
	// pos处理到世界空间，保存
	out->worldpos_tx = (context->matrixs[eMd_World] * in->pos);
	out->worldpos_tx.w = in->texcoord.x;
	// normal,tangent处理到世界空间
	out->normal_ty = float4( (context->matrixs[eMd_World].RotateVector3(in->normal)), in->texcoord.y);

	// 暂时把pos存入tangent，供geometry阶段计算tangent
	out->tangent.xyz = in->pos.xyz;// * out->pos.w;
	//out->texcoord2 = in->texcoord * out->pos.w;
}

void SrPhongWithNormalShader::ProcessRasterize( void* rOut, const void* rInRef0, const void* rInRef1, const void* rInRef2, float ratio, const SrShaderContext* context, bool final ) const
{
	const SrPhongShading_Vert2Frag* verA = static_cast<const SrPhongShading_Vert2Frag*>(rInRef0);
	const SrPhongShading_Vert2Frag* verB = static_cast<const SrPhongShading_Vert2Frag*>(rInRef1);
	SrPhongShading_Vert2Frag* verO = static_cast<SrPhongShading_Vert2Frag*>(rOut);

	// 线性插值project space pos
	float inv_ratio = 1.f - ratio;

#ifdef SR_USE_AVX
	FastRasterize( (SrRendVertex*)rOut, (SrRendVertex*)rInRef0, (SrRendVertex*)rInRef1, ratio, inv_ratio);

	if (final)
	{
		FastFinalRasterize( (SrRendVertex*)rOut, ((SrRendVertex*)rOut)->pos.w);
	}
#else
	verO->pos = SrFastLerp( verA->pos, verB->pos, ratio, inv_ratio );

	// 已经除w
	// 直接插值，其他channel
	verO->normal_ty = SrFastLerp( verA->normal_ty, verB->normal_ty, ratio, inv_ratio );
	verO->worldpos_tx = SrFastLerp( verA->worldpos_tx, verB->worldpos_tx, ratio, inv_ratio );
	verO->tangent = SrFastLerp( verA->tangent, verB->tangent, ratio, inv_ratio );

	// 对于scanline扫描的，将透视插值坐标，插值回正常值
	if (final)
	{
		verO->normal_ty /= verO->pos.w;
		verO->worldpos_tx /= verO->pos.w;
		verO->tangent /= verO->pos.w;
	}
#endif

}

float2 g_reflect[16] = 
{

	//0,0,0,0
// 	float2( sinf( 0 * SR_PI / 8.f), cosf( 0 * SR_PI / 8.f) ),
// 	float2( sinf( 8 * SR_PI / 8.f), cosf( 8 * SR_PI / 8.f) ),
// 	float2( sinf( 2 * SR_PI / 8.f), cosf( 2 * SR_PI / 8.f) ),
// 	float2( sinf( 10 * SR_PI / 8.f), cosf( 10 * SR_PI / 8.f) ),

	// 0,1,0,1
// 	float2( sinf( 4 * SR_PI / 8.f), cosf( 4 * SR_PI / 8.f) ),
// 	float2( sinf( 13 * SR_PI / 8.f), cosf( 13 * SR_PI / 8.f) ),
// 	float2( sinf( 6 * SR_PI / 8.f), cosf( 6 * SR_PI / 8.f) ),
// 	float2( sinf( 15 * SR_PI / 8.f), cosf( 15 * SR_PI / 8.f) ),

	// 1,0,1,0
// 	float2( sinf( 5 * SR_PI / 8.f), cosf( 5 * SR_PI / 8.f) ),
// 	float2( sinf( 12 * SR_PI / 8.f), cosf( 12 * SR_PI / 8.f) ),
// 	float2( sinf( 7 * SR_PI / 8.f), cosf( 7 * SR_PI / 8.f) ),
// 	float2( sinf( 14 * SR_PI / 8.f), cosf( 14 * SR_PI / 8.f) ),


// 
// 
// 	float2( sinf( 1 * SR_PI / 8.f), cosf( 1 * SR_PI / 8.f) ),
// 	float2( sinf( 9 * SR_PI / 8.f), cosf( 9 * SR_PI / 8.f) ),
// 	float2( sinf( 3 * SR_PI / 8.f), cosf( 3 * SR_PI / 8.f) ),
// 	float2( sinf( 11 * SR_PI / 8.f), cosf( 11 * SR_PI / 8.f) ),
// 
// 	float2( sinf( 5 * SR_PI / 8.f), cosf( 5 * SR_PI / 8.f) ),
// 	float2( sinf( 12 * SR_PI / 8.f), cosf( 12 * SR_PI / 8.f) ),
// 	float2( sinf( 7 * SR_PI / 8.f), cosf( 7 * SR_PI / 8.f) ),
// 	float2( sinf( 14 * SR_PI / 8.f), cosf( 14 * SR_PI / 8.f) ),
// 
// 	float2( sinf( 1 * SR_PI / 8.f), cosf( 1 * SR_PI / 8.f) ),
// 	float2( sinf( 9 * SR_PI / 8.f), cosf( 9 * SR_PI / 8.f) ),
// 	float2( sinf( 3 * SR_PI / 8.f), cosf( 3 * SR_PI / 8.f) ),
// 	float2( sinf( 11 * SR_PI / 8.f), cosf( 11 * SR_PI / 8.f) ),

// 
// 	float2( sinf( 5 * SR_PI / 8.f), cosf( 5 * SR_PI / 8.f) ),
// 	float2( sinf( 12 * SR_PI / 8.f), cosf( 12 * SR_PI / 8.f) ),
// 	float2( sinf( 7 * SR_PI / 8.f), cosf( 7 * SR_PI / 8.f) ),
// 	float2( sinf( 14 * SR_PI / 8.f), cosf( 14 * SR_PI / 8.f) ),


	float2( sinf( 1.25f * SR_PI / 8.f),	cosf( 1.25f * SR_PI / 8.f) ),
		float2( sinf( 0.25f * SR_PI / 8.f),	cosf( 0.25f * SR_PI / 8.f) ),

	float2( sinf( 3.25f * SR_PI / 8.f),	cosf( 3.25f * SR_PI / 8.f) ),
		float2( sinf( 2.25f * SR_PI / 8.f),	cosf( 2.25f * SR_PI / 8.f) ),

		float2( sinf( 4.25f * SR_PI / 8.f),	cosf( 4.25f * SR_PI / 8.f) ),
	float2( sinf( 5.25f * SR_PI / 8.f),	cosf( 5.25f * SR_PI / 8.f) ),

		float2( sinf( 6.25f * SR_PI / 8.f),	cosf( 6.25f * SR_PI / 8.f) ),
	float2( sinf( 7.25f * SR_PI / 8.f),	cosf( 7.25f * SR_PI / 8.f) ),



	float2( sinf( 9.25f * SR_PI / 8.f),	cosf( 9.25f * SR_PI / 8.f) ),
		float2( sinf( 8.25f * SR_PI / 8.f),	cosf( 8.25f * SR_PI / 8.f) ),

	float2( sinf( 11.25f * SR_PI / 8.f),	cosf( 11.25f * SR_PI / 8.f) ),
		float2( sinf( 10.25f * SR_PI / 8.f),	cosf( 10.25f * SR_PI / 8.f) ),

	float2( sinf( 12.25f * SR_PI / 8.f),	cosf( 12.25f * SR_PI / 8.f) ),
	float2( sinf( 13.25f * SR_PI / 8.f),	cosf( 13.25f * SR_PI / 8.f) ),
	float2( sinf( 14.25f * SR_PI / 8.f),	cosf( 14.25f * SR_PI / 8.f) ),
	float2( sinf( 15.25f * SR_PI / 8.f),	cosf( 15.25f * SR_PI / 8.f) ),

};

float2 g_kernel[8] = {
	float2(-2.f,0),
	float2(2.f,0),
	float2(0,2.f),
	float2(0,-2.f),
	float2(-0.7071,0.7071),
	float2(0.7071,-0.7071),
	float2(0.7071,0.7071),
	float2(-0.7071,-0.7071),
};

void SRFASTCALL SrPhongWithNormalShader::ProcessPixel( uint32* pOut, const void* pIn, const SrShaderContext* context, uint32 address ) const
{
	SrPhongShading_Vert2Frag* in = (SrPhongShading_Vert2Frag*)pIn;
	uint32* out = (uint32*)pOut;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());
	float2 tc0(in->worldpos_tx.w, in->normal_ty.w);
	
	// 采样diffuse颜色
	uint32 col = context->Tex2D( tc0, 0 );
	float4 matDiff = uint32_2_float4(col);
	float4 matSpec(matDiff.a);
	// diffuse 作 srgb
	matDiff = matDiff * matDiff;

	// 法线扰动处理
	float3 normalDir = in->normal_ty.xyz;
	normalDir.normalize();
	float3 tangentDir = in->tangent.xyz;
	tangentDir.normalize();
	float3 binormalDir = tangentDir % normalDir;

	// 利用世界空间的 切线，法线，副法线 构造从切线空间到世界空间的转换矩阵
	float33 tangent2world( tangentDir, binormalDir, normalDir );

	// 采样normal
	uint32 nor = context->Tex2D( tc0, 1 );
	float4 norf = uint32_2_float4(nor);

	float3 normalTangent = norf.xyz;
	normalTangent = (normalTangent - float3(0.5f)) * 2.f;

	// 将采样到的切线空间法线转换到世界空间
	normalDir = tangent2world * normalTangent;
	normalDir.normalize();

	// 视线方向计算
	float3 viewWS = context->matrixs[eMd_ViewInverse].GetTranslate() - in->worldpos_tx.xyz;
	viewWS.normalize();

	float4 diffuseAcc = gEnv->sceneMgr->GetSkyLightColor() * (normalDir.y * 0.4f + 0.6f);
	float4 specularAcc(0.f);
	
	// 光照计算
	CalcLights(context, in->worldpos_tx.xyz, normalDir, viewWS, diffuseAcc, specularAcc);

	// calc ssao here
	
	// 2.计算旋转的采样address
	//int address = (int)(in->pos.x) % 4 + (int)(in->pos.y) % 4 * 4 ;
	//address %= 16;
	float ao = 0;

	bool jit = (gEnv->context->IsFeatureEnable(eRFeature_JitAA));

	if ( !jit ||  ( ( (address - ((address /  gEnv->context->width) % 2)) % 2 != gEnv->renderer->getFrameCount() % 2) )  )
	{
 		int y = address / gEnv->context->width;
 		int x = address % gEnv->context->width;
		int address = x % 4 + (y % 4) * 4;
		address %= 16;
		// ao累积
		

		// fragment索引
		SrFragment* fragment = (SrFragment*)pIn;

		// 1.得到自己的纹理坐标
		float2 hTc(in->pos.x / (float) gEnv->context->width, in->pos.y / (float) gEnv->context->height);

			// 迭代八次，计算AO
		for ( int i=0; i < 8; ++i)
		{
			float2 tc = g_kernel[i];
			tc.reflect( tc, g_reflect[address]);
			tc = (tc / (float)gEnv->context->width) * 180 * (1- in->pos.z);
			float3 worldposOther = gEnv->context->fBuffer->GetWorldPos( hTc + tc );
			float3 diff = worldposOther - in->worldpos_tx.xyz; 
			float d = diff.length(); 
			if (d < SR_EQUAL_PRECISION)
			{
				//ao += 1;
				continue;
			}
			diff = diff / d;
			d*= 0.3f;
			ao += max(0.0f ,float3::dot(normalDir,diff) ) * ( 1.0 / (1.0 + d) ) * 2.f * (jit ? 2.f : 1.f);
		}

		// 平均AO
		ao *= 0.125f;

		// 截断到0~1
		ao = Clamp(ao, 0.f, 1.f);
		//ao = 1.f - ao;
		// 调和amb
		diffuseAcc *= 1.f - ao;

		ao -= 0.5f;
		//diffuseAcc *= address / 16.f;
	}

	// 截断到0-1
	diffuseAcc = diffuseAcc * matDiff * cBuffer->difColor;
	diffuseAcc *= (1.f - ao);
	diffuseAcc += cBuffer->spcColor * specularAcc * matSpec.x;

	diffuseAcc = Clamp(diffuseAcc, 0.f, 1.f);	

	// srgb还原
	diffuseAcc.sqrt();

	*out = float4_2_uint32(diffuseAcc);
}
