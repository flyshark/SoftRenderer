/**
  @file IRenderer.h
  
  @brief 公共的渲染结构声明

  @author yikaiming

  更改日志 history
  ver:1.0
   
 */

#ifndef IRenderer_h__
#define IRenderer_h__

#include "prerequisite.h"

class SrTexture;
class SrRasterizer;
class SrResourceManager;
class SrShader;

struct SrTextLine
{
	std::string text;
	int2 pos;
	uint32 color;
	uint32 size;
};
typedef std::vector<SrTextLine> SrTextLines;

enum EHwTimerElement
{
	eHt_GpuFlushTime = 0,
	eHt_GpuHairTime,
	eHt_GpuPostProcessTime,

	eHt_Count,
};

enum ERendererType
{
	eRt_Software,
	eRt_HardwareD3D9,
	eRt_HardwareD3D11,
};

// Renderer Interface
class IRenderer
{
public:
	IRenderer::IRenderer( ERendererType type ):m_rendererType(type)
	{
		m_matrixStack.assign( eMd_Count, float44::CreateIdentity() );
		m_frameCount = 0;
	}
	virtual ~IRenderer(void)
	{

	}

	virtual const char* getName() =0;

	// 启动，关闭函数
	virtual bool Resize(uint32 width, uint32 height)=0;
	virtual uint32 getWidth() =0;
	virtual uint32 getHeight() =0;

	// 帧控制函数
	virtual void BeginFrame()=0;
	virtual void EndFrame()=0;

	// 硬件Clear
	virtual bool HwClear()=0;

	// 获取ScreenBuffer函数
	virtual void* getBuffer() =0;

	// 纹理通道设置
	virtual bool SetTextureStage( const SrTexture* texture, int stage )=0;
	virtual void ClearTextureStage()=0;

	virtual bool UpdateVertexBuffer(SrVertexBuffer* target) {return true;}
	virtual bool UpdateIndexBuffer(SrIndexBuffer* target) {return true;}

	// 渲染调用
	virtual bool DrawPrimitive( SrPrimitve* primitive )=0;
	virtual bool DrawLine(const float3& from, const float3& to)=0;
	virtual bool DrawScreenText(const char* str, int x,int y, uint32 size, DWORD color = SR_UICOLOR_HIGHLIGHT) =0;

	// Shader设置
	virtual bool SetShader(const SrShader* shader) =0;
	virtual bool SetShaderConstant( uint32 slot, const float* constantStart, uint32 vec4Count ) =0;

	virtual void SetGpuMarkStart(EHwTimerElement element) {}
	virtual void SetGpuMarkEnd(EHwTimerElement element) {}
	virtual float GetGpuTime(EHwTimerElement element) {return 0;}

	void SetMatrix( EMatrixDefine index, const float44& matrix )
	{
		if (index < eMd_Count)
		{
			m_matrixStack[index] = matrix;
		}	
	}

	float44 GetMatrix( EMatrixDefine index )
	{
		float44 ret = float44::CreateIdentity();
		if (index < eMd_Count)
		{
			ret = m_matrixStack[index];
		}

		return ret;
	}

	// TEX2D for swRenderer
	virtual uint32 Tex2D(float2& texcoord, const SrTexture* texture ) const =0;


	// FrameCount
	uint32 getFrameCount() {return m_frameCount;}

	ERendererType m_rendererType;

	virtual bool InitRenderer(HWND hWnd, int width, int height, int bpp) =0;
	virtual bool ShutdownRenderer() =0;

protected:
	SrMatrixArray m_matrixStack;
	
	uint32 m_frameCount;	
};



#endif // IRenderer_h__