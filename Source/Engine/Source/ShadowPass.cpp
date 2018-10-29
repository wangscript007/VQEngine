//	VQEngine | DirectX11 Renderer
//	Copyright(C) 2018  - Volkan Ilbeyli
//
//	This program is free software : you can redistribute it and / or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.If not, see <http://www.gnu.org/licenses/>.
//
//	Contact: volkanilbeyli@gmail.com

#include "RenderPasses.h"
#include "SceneResources.h"
#include "Engine.h"
#include "GameObject.h"
#include "Light.h"
#include "SceneView.h"

#include "Renderer/Renderer.h"

#if _DEBUG
#include "Utilities/Log.h"
#endif

constexpr int DRAW_INSTANCED_COUNT_DEPTH_PASS = 256;


vec2 ShadowMapPass::GetDirectionalShadowMapDimensions(Renderer* pRenderer) const
{
	if (this->mDepthTarget_Directional == -1)
		return vec2(0, 0);

	const float dim = static_cast<float>(pRenderer->GetTextureObject(pRenderer->GetDepthTargetTexture(this->mDepthTarget_Directional))._width);
	return vec2(dim);
}



void ShadowMapPass::Initialize(Renderer* pRenderer, const Settings::ShadowMap& shadowMapSettings)
{
	this->mpRenderer = pRenderer;
	this->mShadowMapShader = EShaders::SHADOWMAP_DEPTH;
	const ShaderDesc instancedShaderDesc = { "DepthShader",
		ShaderStageDesc{"DepthShader_vs.hlsl", {
			ShaderMacro{ "INSTANCED"     , "1" },
			ShaderMacro{ "INSTANCE_COUNT", std::to_string(DRAW_INSTANCED_COUNT_DEPTH_PASS) }
		}},
		ShaderStageDesc{"DepthShader_ps.hlsl" , {} }
	};
	this->mShadowMapShaderInstanced = pRenderer->CreateShader(instancedShaderDesc);

	InitializeSpotLightShadowMaps(shadowMapSettings);
	InitializePointLightShadowMaps(shadowMapSettings);
}


void ShadowMapPass::InitializeSpotLightShadowMaps(const Settings::ShadowMap& shadowMapSettings)
{
	assert(mpRenderer);
	this->mShadowMapDimension_Spot = static_cast<int>(shadowMapSettings.dimension);

#if _DEBUG
	//pRenderer->m_Direct3D->ReportLiveObjects("--------SHADOW_PASS_INIT");
#endif

	// check feature support & error handle:
	// https://msdn.microsoft.com/en-us/library/windows/apps/dn263150
	const bool bDepthOnly = true;
	const EImageFormat format = bDepthOnly ? R32 : R24G8;

	DepthTargetDesc depthDesc;
	depthDesc.format = bDepthOnly ? D32F : D24UNORM_S8U;

	TextureDesc& texDesc = depthDesc.textureDesc;
	texDesc.format = format;
	texDesc.usage = static_cast<ETextureUsage>(DEPTH_TARGET | RESOURCE);
	texDesc.height = texDesc.width = mShadowMapDimension_Spot;
	texDesc.arraySize = NUM_SPOT_LIGHT_SHADOW;

	// CREATE DEPTH TARGETS: SPOT LIGHTS
	//
	auto DepthTargetIDs = mpRenderer->AddDepthTarget(depthDesc);
	this->mDepthTargets_Spot.resize(NUM_SPOT_LIGHT_SHADOW);
	std::copy(RANGE(DepthTargetIDs), this->mDepthTargets_Spot.begin());

	this->mShadowMapTextures_Spot = this->mDepthTargets_Spot.size() > 0
		? mpRenderer->GetDepthTargetTexture(this->mDepthTargets_Spot[0])
		: -1;


}
void ShadowMapPass::InitializePointLightShadowMaps(const Settings::ShadowMap& shadowMapSettings)
{
	assert(mpRenderer);
	this->mShadowMapDimension_Point = static_cast<int>(shadowMapSettings.dimension);

#if _DEBUG
	//pRenderer->m_Direct3D->ReportLiveObjects("--------SHADOW_PASS_INIT");
#endif

	// check feature support & error handle:
	// https://msdn.microsoft.com/en-us/library/windows/apps/dn263150
	const bool bDepthOnly = true;
	const EImageFormat format = bDepthOnly ? R32 : R24G8;


	// CREATE DEPTH TARGETS: POINT LIGHTS
	//
	DepthTargetDesc depthDesc;
	depthDesc.format = bDepthOnly ? D32F : D24UNORM_S8U;

	TextureDesc& texDesc = depthDesc.textureDesc;
	texDesc.format = format;
	texDesc.usage = static_cast<ETextureUsage>(DEPTH_TARGET | RESOURCE);
	texDesc.bIsCubeMap = true;
	texDesc.height = texDesc.width = this->mShadowMapDimension_Point;
	texDesc.arraySize = NUM_POINT_LIGHT_SHADOW;
	texDesc.texFileName = "Point Light Shadow Maps";

	auto DepthTargetIDs = mpRenderer->AddDepthTarget(depthDesc);
	this->mDepthTargets_Point.resize(NUM_POINT_LIGHT_SHADOW * 6 /*each face = one depth target*/);
	std::copy(RANGE(DepthTargetIDs), this->mDepthTargets_Point.begin());

	this->mShadowMapTextures_Point = this->mDepthTargets_Point.size() > 0
		? mpRenderer->GetDepthTargetTexture(this->mDepthTargets_Point[0])
		: -1;

}
void ShadowMapPass::InitializeDirectionalLightShadowMap(const Settings::ShadowMap & shadowMapSettings)
{
	assert(mpRenderer);
	const int textureDimension = static_cast<int>(shadowMapSettings.dimension);
	const EImageFormat format = R32;

	DepthTargetDesc depthDesc;
	depthDesc.format = D32F;
	TextureDesc& texDesc = depthDesc.textureDesc;
	texDesc.format = format;
	texDesc.usage = static_cast<ETextureUsage>(DEPTH_TARGET | RESOURCE);
	texDesc.height = texDesc.width = textureDimension;
	texDesc.arraySize = 1;

	// first time - add target
	if (this->mDepthTarget_Directional == -1)
	{
		this->mDepthTarget_Directional = mpRenderer->AddDepthTarget(depthDesc)[0];
		this->mShadowMapTexture_Directional = mpRenderer->GetDepthTargetTexture(this->mDepthTarget_Directional);
	}
	else // other times - check if dimension changed
	{
		const bool bDimensionChanged = shadowMapSettings.dimension != mpRenderer->GetTextureObject(mpRenderer->GetDepthTargetTexture(this->mDepthTarget_Directional))._width;
		if (bDimensionChanged)
		{
			mpRenderer->RecycleDepthTarget(this->mDepthTarget_Directional, depthDesc);
		}
	}
}


#define INSTANCED_DRAW 1
void ShadowMapPass::RenderShadowMaps(Renderer* pRenderer, const ShadowView& shadowView, GPUProfiler* pGPUProfiler) const
{
	//-----------------------------------------------------------------------------------------------
	struct PerObjectMatrices { XMMATRIX wvp; };
	struct InstancedObjectCBuffer { PerObjectMatrices objMatrices[DRAW_INSTANCED_COUNT_DEPTH_PASS]; };
	auto Is2DGeometry = [](MeshID mesh)
	{
		return mesh == EGeometry::TRIANGLE || mesh == EGeometry::QUAD || mesh == EGeometry::GRID;
	};
	auto RenderDepth = [&](const GameObject* pObj, const XMMATRIX& viewProj)
	{
		const ModelData& model = pObj->GetModelData();
		const PerObjectMatrices objMats = PerObjectMatrices({ pObj->GetTransform().WorldTransformationMatrix() * viewProj });

		pRenderer->SetConstantStruct("ObjMats", &objMats);
		std::for_each(model.mMeshIDs.begin(), model.mMeshIDs.end(), [&](MeshID id)
		{
			const RasterizerStateID rasterizerState = Is2DGeometry(id) ? EDefaultRasterizerState::CULL_NONE : EDefaultRasterizerState::CULL_FRONT;
			const auto IABuffer = SceneResourceView::GetVertexAndIndexBuffersOfMesh(ENGINE->mpActiveScene, id);

			pRenderer->SetRasterizerState(rasterizerState);
			pRenderer->SetVertexBuffer(IABuffer.first);
			pRenderer->SetIndexBuffer(IABuffer.second);
			pRenderer->Apply();
			pRenderer->DrawIndexed();
		});
	};
	//-----------------------------------------------------------------------------------------------
	D3D11_VIEWPORT viewPort = {};
	viewPort.MinDepth = 0.f;
	viewPort.MaxDepth = 1.f;

	const bool bNoShadowingLights = shadowView.spots.empty() && shadowView.points.empty() && shadowView.pDirectional == nullptr;
	if (bNoShadowingLights) return;

	pRenderer->SetDepthStencilState(EDefaultDepthStencilState::DEPTH_WRITE);
	pRenderer->SetShader(mShadowMapShader);					// shader for rendering z buffer

	//-----------------------------------------------------------------------------------------------
	// SPOT LIGHT SHADOW MAPS
	//-----------------------------------------------------------------------------------------------
	viewPort.Height = static_cast<float>(mShadowMapDimension_Spot);
	viewPort.Width = static_cast<float>(mShadowMapDimension_Spot);
	pGPUProfiler->BeginEntry("Spots");
	pRenderer->SetViewport(viewPort);
	for (size_t i = 0; i < shadowView.spots.size(); i++)
	{
		const XMMATRIX viewProj = shadowView.spots[i]->GetLightSpaceMatrix();
		pRenderer->BeginEvent("Spot[" + std::to_string(i) + "]: DrawSceneZ()");
#if _DEBUG
		if (shadowView.shadowMapRenderListLookUp.find(shadowView.spots[i]) == shadowView.shadowMapRenderListLookUp.end())
		{
			Log::Error("Spot light not found in shadowmap render list lookup");
			continue;
		}
#endif
		pRenderer->BindDepthTarget(mDepthTargets_Spot[i]);	// only depth stencil buffer
		pRenderer->BeginRender(ClearCommand::Depth(1.0f));
		pRenderer->Apply();

		for (const GameObject* pObj : shadowView.shadowMapRenderListLookUp.at(shadowView.spots[i]))
		{
			RenderDepth(pObj, viewProj);
		}
		pRenderer->EndEvent();
	}
	pGPUProfiler->EndEntry();	// spots


#if 1
	//-----------------------------------------------------------------------------------------------
	// POINT LIGHT SHADOW MAPS
	//-----------------------------------------------------------------------------------------------
	viewPort.Height = static_cast<float>(mShadowMapDimension_Point);
	viewPort.Width = static_cast<float>(mShadowMapDimension_Point);
	pGPUProfiler->BeginEntry("Points");
	pRenderer->SetViewport(viewPort);
	for (size_t i = 0; i < shadowView.points.size(); i++)
	{
#if _DEBUG
		if (shadowView.shadowMapRenderListLookUp.find(shadowView.points[i]) == shadowView.shadowMapRenderListLookUp.end())
		{
			Log::Error("Point light not found in shadowmap render list lookup");
			continue;;
		}
#endif
		
		pRenderer->BeginEvent("Point[" + std::to_string(i) + "]: DrawSceneZ()");
		for (int face = 0; face < 6; ++face)
		{
			const XMMATRIX viewProj = 
				  Texture::CubemapUtility::GetViewMatrix(face, shadowView.points[i]->transform._position) 
				* shadowView.points[i]->GetProjectionMatrix();

			const size_t depthTargetIndex = i * 6 + face;
			pRenderer->BindDepthTarget(mDepthTargets_Point[depthTargetIndex]);	// only depth stencil buffer
			pRenderer->BeginRender(ClearCommand::Depth(1.0f));
			pRenderer->Apply();

			// todo: objects per view direction?
			for (const GameObject* pObj : shadowView.shadowMapRenderListLookUp.at(shadowView.points[i]))
			{
				RenderDepth(pObj, viewProj);
			}
		}
		pRenderer->EndEvent();
	}
	pGPUProfiler->EndEntry();	// Points
#endif


	//-----------------------------------------------------------------------------------------------
	// DIRECTIONAL SHADOW MAP
	//-----------------------------------------------------------------------------------------------
	if (shadowView.pDirectional != nullptr)
	{
		const XMMATRIX viewProj = shadowView.pDirectional->GetLightSpaceMatrix();

		pGPUProfiler->BeginEntry("Directional");
		pRenderer->BeginEvent("Directional: DrawSceneZ()");

		// RENDER NON-INSTANCED SCENE OBJECTS
		//
		const int shadowMapDimension = pRenderer->GetTextureObject(pRenderer->GetDepthTargetTexture(this->mDepthTarget_Directional))._width;
		viewPort.Height = static_cast<float>(shadowMapDimension);
		viewPort.Width = static_cast<float>(shadowMapDimension);
		pRenderer->SetViewport(viewPort);
		pRenderer->BindDepthTarget(mDepthTarget_Directional);
		pRenderer->Apply();
		pRenderer->BeginRender(ClearCommand::Depth(1.0f));
		for (const GameObject* pObj : shadowView.casters)
		{
			RenderDepth(pObj, viewProj);
		}


		// RENDER INSTANCED SCENE OBJECTS
		//
		pRenderer->SetShader(mShadowMapShaderInstanced);
		pRenderer->BindDepthTarget(mDepthTarget_Directional);

		InstancedObjectCBuffer cbuffer;
		for (const RenderListLookupEntry& MeshID_RenderList : shadowView.RenderListsPerMeshType)
		{
			const MeshID& mesh = MeshID_RenderList.first;
			const std::vector<const GameObject*>& renderList = MeshID_RenderList.second;

			const RasterizerStateID rasterizerState = EDefaultRasterizerState::CULL_NONE;// Is2DGeometry(mesh) ? EDefaultRasterizerState::CULL_NONE : EDefaultRasterizerState::CULL_FRONT;
			const auto IABuffer = SceneResourceView::GetVertexAndIndexBuffersOfMesh(ENGINE->mpActiveScene, mesh);

			pRenderer->SetRasterizerState(rasterizerState);
			pRenderer->SetVertexBuffer(IABuffer.first);
			pRenderer->SetIndexBuffer(IABuffer.second);

			int batchCount = 0;
			do
			{
				int instanceID = 0;
				for (; instanceID < DRAW_INSTANCED_COUNT_DEPTH_PASS; ++instanceID)
				{
					const int renderListIndex = DRAW_INSTANCED_COUNT_DEPTH_PASS * batchCount + instanceID;
					if (renderListIndex == renderList.size())
						break;

					cbuffer.objMatrices[instanceID] =
					{
						renderList[renderListIndex]->GetTransform().WorldTransformationMatrix() * viewProj
					};
				}

				pRenderer->SetConstantStruct("ObjMats", &cbuffer);
				pRenderer->Apply();
				pRenderer->DrawIndexedInstanced(instanceID);
			} while (batchCount++ < renderList.size() / DRAW_INSTANCED_COUNT_DEPTH_PASS);
		}

		pRenderer->EndEvent();
		pGPUProfiler->EndEntry();
	}
}

