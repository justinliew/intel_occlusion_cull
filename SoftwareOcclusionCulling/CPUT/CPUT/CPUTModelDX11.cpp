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
//--------------------------------------------------------------------------------------
#include "CPUTModelDX11.h"
#include "CPUTMaterialDX11.h"
#include "CPUTRenderParamsDX.h"
#include "CPUTFrustum.h"
#include "CPUTTextureDX11.h"
#include "CPUTBufferDX11.h"

//-----------------------------------------------------------------------------
CPUTModelDX11::~CPUTModelDX11(){
    SAFE_RELEASE(mpCPUTConstantBuffer);
    SAFE_RELEASE(mpModelConstantBuffer);
}

// Return the mesh at the given index (cast to the GFX api version of CPUTMeshDX11)
//-----------------------------------------------------------------------------
CPUTMeshDX11* CPUTModelDX11::GetMesh(const UINT index) const
{
    return ( 0==mMeshCount || index > mMeshCount) ? NULL : (CPUTMeshDX11*)mpMesh[index];
}

float3 gLightDir = float3(0.7f, -0.5f, -0.1f);

// Set the render state before drawing this object
//-----------------------------------------------------------------------------
void CPUTModelDX11::SetRenderStates(CPUTRenderParameters &renderParams)
{
    // TODO: need to update the constant buffer only when the model moves.
    // But, requires individual, per-model constant buffers
    ID3D11DeviceContext *pContext  = ((CPUTRenderParametersDX*)&renderParams)->mpContext;

    CPUTModelConstantBuffer *pCb;

    // update parameters of constant buffer
    D3D11_MAPPED_SUBRESOURCE mapInfo;
    pContext->Map( mpModelConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapInfo );
    {
        // TODO: remove construction of XMM type
        XMMATRIX    world((float*)GetWorldMatrix());
        XMVECTOR    determinant = XMMatrixDeterminant(world);
        CPUTCamera *pCamera     = gpSample->GetCamera();
        XMMATRIX    view((float*)pCamera->GetViewMatrix());
        XMMATRIX    projection((float*)pCamera->GetProjectionMatrix());
        float      *pCameraPos = (float*)&pCamera->GetPosition();
        XMVECTOR    cameraPos = XMLoadFloat3(&XMFLOAT3( pCameraPos[0], pCameraPos[1], pCameraPos[2] ));

        pCb = (CPUTModelConstantBuffer*)mapInfo.pData;
		XMMATRIX ViewProj = view * projection;
        pCb->World               = world;
        pCb->ViewProjection      = view  * ViewProj;
        pCb->WorldViewProjection = world  * ViewProj;
        pCb->InverseWorld        = XMMatrixInverse(&determinant, XMMatrixTranspose(world));
        // pCb->LightDirection      = XMVector3Transform(gLightDir, pCb->InverseWorld );
        // pCb->EyePosition         = XMVector3Transform(cameraPos, pCb->InverseWorld );
        // TODO: Tell the lights to set their render states

        XMVECTOR lightDirection = XMLoadFloat3(&XMFLOAT3( gLightDir.x, gLightDir.y, gLightDir.z ));
        pCb->LightDirection      = XMVector3Normalize(lightDirection);
        pCb->EyePosition         = cameraPos;
        float *bbCWS = (float*)&mBoundingBoxCenterWorldSpace;
        float *bbHWS = (float*)&mBoundingBoxHalfWorldSpace;
        float *bbCOS = (float*)&mBoundingBoxCenterObjectSpace;
        float *bbHOS = (float*)&mBoundingBoxHalfObjectSpace;
        pCb->BoundingBoxCenterWorldSpace  = XMLoadFloat3(&XMFLOAT3( bbCWS[0], bbCWS[1], bbCWS[2] )); ;
        pCb->BoundingBoxHalfWorldSpace    = XMLoadFloat3(&XMFLOAT3( bbHWS[0], bbHWS[1], bbHWS[2] )); ;
        pCb->BoundingBoxCenterObjectSpace = XMLoadFloat3(&XMFLOAT3( bbCOS[0], bbCOS[1], bbCOS[2] )); ;
        pCb->BoundingBoxHalfObjectSpace   = XMLoadFloat3(&XMFLOAT3( bbHOS[0], bbHOS[1], bbHOS[2] )); ;

        // Shadow camera
        XMMATRIX    shadowView, shadowProjection;
        CPUTCamera *pShadowCamera = gpSample->GetShadowCamera();
        if( pShadowCamera )
        {
            shadowView = XMMATRIX((float*)pShadowCamera->GetViewMatrix());
            shadowProjection = XMMATRIX((float*)pShadowCamera->GetProjectionMatrix());
            pCb->LightWorldViewProjection = world * shadowView * shadowProjection;
        }
    }
    pContext->Unmap(mpModelConstantBuffer,0);
}

// Render - render this model (only)
extern int gWireFrame;
//-----------------------------------------------------------------------------
void CPUTModelDX11::Render(CPUTRenderParameters &renderParams)
{
if( gWireFrame != 6 )
{
    CPUTRenderParametersDX *pParams = (CPUTRenderParametersDX*)&renderParams;
    CPUTCamera             *pCamera = pParams->mpCamera;

#ifdef SUPPORT_DRAWING_BOUNDING_BOXES 
    if( renderParams.mShowBoundingBoxes && (!pCamera || pCamera->mFrustum.IsVisible( mBoundingBoxCenterWorldSpace, mBoundingBoxHalfWorldSpace )))
    {
        DrawBoundingBox( renderParams );
    }
#endif
    if( !renderParams.mDrawModels ) { return; }

    bool isVisible = true;
    if( gWireFrame != 5 )
    {
        isVisible = !pParams->mRenderOnlyVisibleModels || !pCamera || pCamera->mFrustum.IsVisible( mBoundingBoxCenterWorldSpace, mBoundingBoxHalfWorldSpace );
    }

    // TODO: add world-space bounding box to model so we don't need to do that work every frame
    // if( !pParams->mRenderOnlyVisibleModels || !pCamera || pCamera->mFrustum.IsVisible( mBoundingBoxCenterWorldSpace, mBoundingBoxHalfWorldSpace ) )
    if( isVisible )
    {
        // loop over all meshes in this model and draw them
        for(UINT ii=0; ii<mMeshCount; ii++)
        {
if( gWireFrame != 4)
{
if( gWireFrame != 1) 
{
            CPUTMaterialDX11 *pMaterial = (CPUTMaterialDX11*)(mpMaterial[ii]);
            pMaterial->SetRenderStates(renderParams);
}

if( gWireFrame != 2) 
{
            // We would like to set the model's render states only once (and then iterate over materials)
            // But, the material resource lists leave holes for per-model resources (e.g., constant buffers)
            // We need to 'fixup' the bound resources.  The material sets some to 0, and the model overwrites them with the correct values.
            SetRenderStates(renderParams);
}
if( gWireFrame != 3 )
{
            ((CPUTMeshDX11*)mpMesh[ii])->Draw(renderParams, this);
}
}
        }
    }
}
}

// Render - render this model (only)
//-----------------------------------------------------------------------------
void CPUTModelDX11::RenderShadow(CPUTRenderParameters &renderParams)
{
    CPUTRenderParametersDX *pParams = (CPUTRenderParametersDX*)&renderParams;
    CPUTCamera             *pCamera = pParams->mpCamera;

#ifdef SUPPORT_DRAWING_BOUNDING_BOXES 
    if( renderParams.mShowBoundingBoxes && (!pCamera || pCamera->mFrustum.IsVisible( mBoundingBoxCenterWorldSpace, mBoundingBoxHalfWorldSpace )))
    {
        DrawBoundingBox( renderParams );
    }
#endif
    if( !renderParams.mDrawModels ) { return; }

    // TODO: add world-space bounding box to model so we don't need to do that work every frame
    if( !pParams->mRenderOnlyVisibleModels || !pCamera || pCamera->mFrustum.IsVisible( mBoundingBoxCenterWorldSpace, mBoundingBoxHalfWorldSpace ) )
    {
        // loop over all meshes in this model and draw them
        for(UINT ii=0; ii<mMeshCount; ii++)
        {
            CPUTMaterialDX11 *pMaterial = (CPUTMaterialDX11*)(mpShadowCastMaterial);
            pMaterial->SetRenderStates(renderParams);

            // We would like to set the model's render states only once (and then iterate over materials)
            // But, the material resource lists leave holes for per-model resources (e.g., constant buffers)
            // We need to 'fixup' the bound resources.  The material sets some to 0, and the model overwrites them with the correct values.
            SetRenderStates(renderParams);

            // Potentially need to use a different vertex-layout object!
            CPUTVertexShaderDX11 *pVertexShader = pMaterial->GetVertexShader();
            ((CPUTMeshDX11*)mpMesh[ii])->DrawShadow(renderParams, this);
        }
    }
}




#ifdef SUPPORT_DRAWING_BOUNDING_BOXES
//-----------------------------------------------------------------------------
void CPUTModelDX11::DrawBoundingBox(CPUTRenderParameters &renderParams)
{
    CPUTRenderParametersDX *pParams = (CPUTRenderParametersDX*)&renderParams;

    SetRenderStates(renderParams);
    CPUTMaterialDX11 *pMaterial = (CPUTMaterialDX11*)mpBoundingBoxMaterial;

    mpBoundingBoxMaterial->SetRenderStates(renderParams);
    CPUTVertexShaderDX11 *pVertexShader = pMaterial->GetVertexShader();
    ((CPUTMeshDX11*)mpBoundingBoxMesh)->Draw(renderParams, this);
}
#endif

// Load the set file definition of this object
// 1. Parse the block of name/parent/transform info for model block
// 2. Load the model's binary payload (i.e., the meshes)
// 3. Assert the # of meshes matches # of materials
// 4. Load each mesh's material
//-----------------------------------------------------------------------------
CPUTResult CPUTModelDX11::LoadModel(CPUTConfigBlock *pBlock, int *pParentID, CPUTModel *pMasterModel)
{
    CPUTResult result = CPUT_SUCCESS;
    CPUTAssetLibraryDX11 *pAssetLibrary = (CPUTAssetLibraryDX11*)CPUTAssetLibrary::GetAssetLibrary();

    cString modelSuffix = ptoc(this);

    // set the model's name
    mName = pBlock->GetValueByName(_L("name"))->ValueAsString();
    mName = mName + _L(".mdl");

    // resolve the full path name
    cString modelLocation;
    cString resolvedPathAndFile;
    modelLocation = ((CPUTAssetLibraryDX11*)CPUTAssetLibrary::GetAssetLibrary())->GetModelDirectory();
    modelLocation = modelLocation+mName;
    CPUTOSServices::GetOSServices()->ResolveAbsolutePathAndFilename(modelLocation, &resolvedPathAndFile);	

    // Get the parent ID.  Note: the caller will use this to set the parent.
    *pParentID = pBlock->GetValueByName(_L("parent"))->ValueAsInt();

    LoadParentMatrixFromParameterBlock( pBlock );

    // Get the bounding box information
	float3 center(0.0f), half(0.0f);
    pBlock->GetValueByName(_L("BoundingBoxCenter"))->ValueAsFloatArray(center.f, 3);
	pBlock->GetValueByName(_L("BoundingBoxHalf"))->ValueAsFloatArray(half.f, 3);
    mBoundingBoxCenterObjectSpace = center;
    mBoundingBoxHalfObjectSpace   = half;

    // the # of meshes in the binary file better match the number of meshes in the .set file definition
    mMeshCount = pBlock->GetValueByName(_L("meshcount"))->ValueAsInt();
    mpMesh     = new CPUTMesh*[mMeshCount];
    mpMaterial = new CPUTMaterial*[mMeshCount];
    memset( mpMaterial, 0, mMeshCount * sizeof(CPUTMaterial*) );
    
    // get the material names, load them, and match them up with each mesh
    cString materialName;
    char pNumber[4];
    cString materialValueName;

    CPUTModelDX11 *pMasterModelDX = (CPUTModelDX11*)pMasterModel;

    for(UINT ii=0; ii<mMeshCount; ii++)
    {
        if(pMasterModelDX)
        {
            // Reference the master model's mesh.  Don't create a new one.
            mpMesh[ii] = pMasterModelDX->mpMesh[ii];
            mpMesh[ii]->AddRef();
        }
        else
        {
            mpMesh[ii] = new CPUTMeshDX11();
        }
    }
    if( !pMasterModelDX )
    {
        // Not a clone/instance.  So, load the model's binary payload (i.e., vertex and index buffers)
        // TODO: Change to use GetModel()
        result = LoadModelPayload(resolvedPathAndFile);
        ASSERT( CPUTSUCCESS(result), _L("Failed loading model") );
    }
    // Create the model constant buffer.
    HRESULT hr;
    D3D11_BUFFER_DESC bd = {0};
    bd.ByteWidth = sizeof(CPUTModelConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = (CPUT_DX11::GetDevice())->CreateBuffer( &bd, NULL, &mpModelConstantBuffer );
    ASSERT( !FAILED( hr ), _L("Error creating constant buffer.") );
    CPUTSetDebugName( mpModelConstantBuffer, _L("Model Constant buffer") );
    cString name = _L("#cbPerModelValues");
    mpCPUTConstantBuffer = new CPUTBufferDX11(name, mpModelConstantBuffer);
    pAssetLibrary->AddConstantBuffer( name, mpCPUTConstantBuffer, this ); // Note: don't include mesh index as this is not mesh dependent
//    pBuffer->Release(); // We're done with it.  We added it to the library.  Release our reference.

    static CPUTBufferDX11 *pMasterConstBuffer = NULL;
    if( !pMasterConstBuffer )
    {
        name = _L("#cbPerModelValues");
        pMasterConstBuffer = new CPUTBufferDX11(name, mpModelConstantBuffer);
        pAssetLibrary->AddConstantBuffer( name, pMasterConstBuffer );
        pMasterConstBuffer->Release(); // We're done with it.  We added it to the library.  Release our reference.
    }
    
#if 0
    cString assetSetDirectoryName = pAssetLibrary->GetAssetSetDirectoryName();
    cString modelDirectory        = pAssetLibrary->GetModelDirectory();
    cString materialDirectory     = pAssetLibrary->GetMaterialDirectory();
    cString textureDirectory      = pAssetLibrary->GetTextureDirectory();
    cString shaderDirectory       = pAssetLibrary->GetShaderDirectory();
    cString fontDirectory         = pAssetLibrary->GetFontDirectory();
    cString up2MediaDirName       = assetSetDirectoryName + _L("..\\..\\");
    pAssetLibrary->SetMediaDirectoryName( up2MediaDirName );
    mpShadowCastMaterial = pAssetLibrary->GetMaterial( _L("shadowCast"), false, this, -2 ); // -2 signifies shadow material.  TODO: find a clearer way (e.g., enum?)
    pAssetLibrary->SetAssetSetDirectoryName( assetSetDirectoryName );
    pAssetLibrary->SetModelDirectoryName( modelDirectory ); 
    pAssetLibrary->SetMaterialDirectoryName( materialDirectory );
    pAssetLibrary->SetTextureDirectoryName( textureDirectory );
    pAssetLibrary->SetShaderDirectoryName( shaderDirectory );
    pAssetLibrary->SetFontDirectoryName( fontDirectory );
#endif
    for(UINT ii=0; ii<mMeshCount; ii++)
    {
        // get the right material number ('material0', 'material1', 'material2', etc)
        materialValueName = _L("material");
        _itoa_s(ii, pNumber, 4, 10);
        materialValueName.append(s2ws(pNumber));
        materialName = pBlock->GetValueByName(materialValueName)->ValueAsString();

        // Get/load material for this mesh
        CPUTMaterialDX11 *pMaterial = (CPUTMaterialDX11*)pAssetLibrary->GetMaterial(materialName, false, this, ii);
        ASSERT( pMaterial, _L("Couldn't find material.") );

        // set the material on this mesh
        // TODO: Model owns the materials.  That allows different models to share meshes (aka instancing) that have different materials
        SetMaterial(ii, pMaterial);

        // Release the extra refcount we're holding from the GetMaterial operation earlier
        // now the asset library, and this model have the only refcounts on that material
        pMaterial->Release();

        // Create two ID3D11InputLayout objects, one for each material.
        mpMesh[ii]->BindVertexShaderLayout( mpMaterial[ii], mpShadowCastMaterial);
        // mpShadowCastMaterial->Release()
    }


    return result;
}

// Set the material associated with this mesh and create/re-use a
//-----------------------------------------------------------------------------
void CPUTModelDX11::SetMaterial(UINT ii, CPUTMaterial *pMaterial)
{
    CPUTModel::SetMaterial(ii, pMaterial);

    // Can't bind the layout if we haven't loaded the mesh yet.
    CPUTMeshDX11 *pMesh = (CPUTMeshDX11*)mpMesh[ii];
    D3D11_INPUT_ELEMENT_DESC *pDesc = pMesh->GetLayoutDescription();
    if( pDesc )
    {
        pMesh->BindVertexShaderLayout(pMaterial, mpMaterial[ii]);
    }
}

//-----------------------------------------------------------------------------
CPUTBuffer *CPUTModelDX11::GetModelConstantBuffer() const
{
    mpCPUTConstantBuffer->AddRef();
    return mpCPUTConstantBuffer;
}

#ifdef SUPPORT_DRAWING_BOUNDING_BOXES
//-----------------------------------------------------------------------------
void CPUTModelDX11::CreateBoundingBoxMesh()
{
    CPUTResult result = CPUT_SUCCESS;
    if( !mpBoundingBoxMesh )
    {
        float3 pVertices[8] = {
            float3(  1.0f,  1.0f,  1.0f ), // 0
            float3(  1.0f,  1.0f, -1.0f ), // 1
            float3( -1.0f,  1.0f,  1.0f ), // 2
            float3( -1.0f,  1.0f, -1.0f ), // 3
            float3(  1.0f, -1.0f,  1.0f ), // 4
            float3(  1.0f, -1.0f, -1.0f ), // 5
            float3( -1.0f, -1.0f,  1.0f ), // 6
            float3( -1.0f, -1.0f, -1.0f )  // 7
        };
        USHORT pIndices[24] = {
           0,1,  1,3,  3,2,  2,0,  // Top
           4,5,  5,7,  7,6,  6,4,  // Bottom
           0,4,  1,5,  2,6,  3,7   // Verticals
        };
        CPUTVertexElementDesc pVertexElements[] = {
            { CPUT_VERTEX_ELEMENT_POSITON, tFLOAT, 12, 0 },
        };

        mpBoundingBoxMesh = new CPUTMeshDX11();
        mpBoundingBoxMesh->SetMeshTopology(CPUT_TOPOLOGY_INDEXED_LINE_LIST);

        CPUTBufferInfo vertexElementInfo;
        vertexElementInfo.mpSemanticName         = "POSITION";
        vertexElementInfo.mSemanticIndex         = 0;
        vertexElementInfo.mElementType           = CPUT_F32;
        vertexElementInfo.mElementComponentCount = 3;
        vertexElementInfo.mElementSizeInBytes    = 12;
        vertexElementInfo.mElementCount          = 8;
        vertexElementInfo.mOffset                = 0;

        CPUTBufferInfo indexDataInfo;
        indexDataInfo.mElementType           = CPUT_U16;
        indexDataInfo.mElementComponentCount = 1;
        indexDataInfo.mElementSizeInBytes    = sizeof(UINT16);
        indexDataInfo.mElementCount          = 24; // 12 lines, 2 verts each
        indexDataInfo.mOffset                = 0;
        indexDataInfo.mSemanticIndex         = 0;
        indexDataInfo.mpSemanticName         = NULL;

        result = mpBoundingBoxMesh->CreateNativeResources(this, -1,
            1,                    // vertexFormatDesc.mFormatDescriptorCount,
            &vertexElementInfo,
            pVertices,            // (void*)vertexFormatDesc.mpVertices,
            &indexDataInfo,
            pIndices              // &vertexFormatDesc.mpIndices[0]
        );
        ASSERT( CPUTSUCCESS(result), _L("Failed building bounding box mesh") );
    }
    if( !mpBoundingBoxMaterialMaster )
    {
        mpBoundingBoxMaterialMaster = CPUTAssetLibrary::GetAssetLibrary()->GetMaterial(_L("BoundingBox"), false, NULL, -3 ); // -1 == mesh independent.  -2 == shadow cast material.  -3 == bounding box.  TODO: how to formalize (enum?)
    }
    mpBoundingBoxMaterial = mpBoundingBoxMaterialMaster->CloneMaterial( _L("BoundingBox"), this, -3 );
    mpBoundingBoxMesh->BindVertexShaderLayout( mpBoundingBoxMaterial,  NULL);
}
#endif
