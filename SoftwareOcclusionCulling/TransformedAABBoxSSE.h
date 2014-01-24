//--------------------------------------------------------------------------------------
// Copyright 2011 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//
//--------------------------------------------------------------------------------------


#ifndef TRANSFORMEDAABBOXSSE_H
#define TRANSFORMEDAABBOXSSE_H

#include "CPUT_DX11.h"
#include "Constants.h"
#include "HelperSSE.h"

class TransformedAABBoxSSE : public HelperSSE
{
	public:
		TransformedAABBoxSSE();
		~TransformedAABBoxSSE();
		void CreateAABBVertexIndexList(CPUTModelDX11 *pModel);
		void IsInsideViewFrustum(CPUTCamera *pCamera);
		void TransformAABBoxAndDepthTest();

		bool IsTooSmall(__m128 *pViewMatrix, __m128 *pProjMatrix, CPUTCamera *pCamera);

		void TransformAABBox();

		void RasterizeAndDepthTestAABBox(UINT *pRenderTargetPixels);

		inline void SetInsideViewFrustum(bool insideVF){mInsideViewFrustum = insideVF;}
		inline bool IsInsideViewFrustum(){ return mInsideViewFrustum;}
		inline void SetVisible(bool *visible){mVisible = visible;}
		inline void SetOccludeeSizeThreshold(float occludeeSizeThreshold){mOccludeeSizeThreshold = occludeeSizeThreshold;}

	private:
		static UINT	mBBIndexList[AABB_INDICES];

		CPUTModelDX11 *mpCPUTModel;
		__m128 *mWorldMatrix;
		__m128 *mpBBVertexList;
		__m128 *mpXformedPos;
		__m128 *mCumulativeMatrix; 
		bool   *mVisible;
		float   mOccludeeSizeThreshold;
		__m128 *mViewPortMatrix; 

		float3 mBBCenterWS;
		float3 mBBHalfWS;
		bool    mInsideViewFrustum;
		float3 mBBCenter;
		float3 mBBHalf;

		void Gather(vFloat4 pOut[3], UINT triId);
};


#endif // TRANSFORMEDAABBOXSSE_H